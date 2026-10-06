#include <iostream>
#include <random>
#include <algorithm>
#include <SFML/Graphics.hpp>

#include "Config.hpp"
#include "Types.hpp"
#include "Physics.hpp"
#include "UI.hpp"
#include "NeutronTransport.hpp"
#include "MainMenu.hpp"

int main() {
    sf::RenderWindow window(sf::VideoMode({ 1000, 700 }), "H2 Fusion Simulation", sf::Style::Default);
    window.setFramerateLimit(60);

    // Иконка приложения (icon.png должна лежать рядом с .exe)
    sf::Image appIcon;
    const bool iconLoaded = appIcon.loadFromFile("icon.png");
    if (iconLoaded) window.setIcon(appIcon);

    SpatialGrid grid;
    grid.init(-BOX_MAX * 0.5f, BOX_MAX, VDW_CUTOFF);   // сетка всегда покрывает BOX_MAX
    sf::View camera(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(30.0f, 21.0f));
    window.setView(camera);

    std::vector<Atom> atoms;
    atoms.push_back(makeHydrogen({ -2.0f, 0.0f }, { 0.5f, 0.0f }));
    atoms.push_back(makeHydrogen({ 2.0f, 0.0f }, { -0.5f, 0.0f }));

    std::vector<Wall> walls;

    sf::Font font;
    bool fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");
    if (!fontLoaded) {
        std::cerr << "Не удалось загрузить шрифт arial.ttf — текст не будет отображён.\n";
    }

    UIState ui;
    ui.batchCounts.assign(ATOM_TYPES.size() + 1, 10);  // +Water row в песочнице

    float physStepAccumulator = 0.0f;

    bool isFullscreen = false;
    bool isPanning = false;
    sf::Vector2i lastMousePos;

    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<float> normalDist(0.0f, 1.0f);
    std::vector<Neutron> neutrons;
    std::vector<Gamma>   gammas;
    std::vector<Neutron> delayedPool;

    // ============================================================
    // Главное меню + состояние приложения
    // ============================================================
    GameState state = GameState::MainMenu;
    MainMenu  mainMenu;
    mainMenu.init(rng);
    sf::Clock frameClock;   // для dt в меню

    sf::RenderTexture sceneRT;
    (void)sceneRT.resize(window.getSize());
    bool rtOk = (sceneRT.getSize().x > 0 && sceneRT.getSize().y > 0);

    sf::Shader fxaaShader;
    bool fxaaOk = fxaaShader.loadFromFile("fxaa.frag", sf::Shader::Type::Fragment);

    // ============================================================
    // Подсчёт молекул для задач уровня 2
    // ============================================================
    auto countWaterMolecules = [](const std::vector<Atom>& atoms) {
        int n = 0;
        for (const auto& a : atoms) {
            if (a.elementId != 1) continue;         // только O
            int hCount = 0;
            bool hasO = false;
            for (int k : a.bonds) {
                if (k < 0) continue;
                if (atoms[k].elementId == 0) hCount++;
                else if (atoms[k].elementId == 1) hasO = true;
            }
            if (hCount == 2 && !hasO) n++;
        }
        return n;
        };

    auto countH2O2Molecules = [](const std::vector<Atom>& atoms) {
        // Считаем O, у которого ровно 1 H и 1 O-партнёр; индексы сравниваем,
        // чтобы не посчитать одну молекулу дважды.
        int n = 0;
        for (size_t i = 0; i < atoms.size(); ++i) {
            if (atoms[i].elementId != 1) continue;
            int hCount = 0;
            int oPartner = -1;
            for (int k : atoms[i].bonds) {
                if (k < 0) continue;
                if (atoms[k].elementId == 0) hCount++;
                else if (atoms[k].elementId == 1) oPartner = k;
            }
            if (hCount != 1) continue;
            if (oPartner < 0 || oPartner <= (int)i) continue;
            const Atom& p = atoms[oPartner];
            int pH = 0; bool pO = false;
            for (int k : p.bonds) {
                if (k < 0) continue;
                if (atoms[k].elementId == 0) pH++;
                else if (atoms[k].elementId == 1) pO = true;
            }
            if (pH == 1 && pO) n++;
        }
        return n;
        };

    auto countActiveHBonds = [](const std::vector<Atom>& atoms) {
        int n = 0;
        for (const auto& a : atoms)
            for (int hb : a.hbonds) if (hb >= 0) n++;
        return n / 2;   // каждая H-связь записана у обоих атомов
        };

    // H3O+: O, у которого ровно 3 H-связи и нет O-партнёра.
    // Получается исключительно через диссоциацию HCl (см. updateBonds).
    auto countH3OMolecules = [](const std::vector<Atom>& atoms) {
        int n = 0;
        for (const auto& a : atoms) {
            if (a.elementId != 1) continue;             // центр — O
            int hCount = 0;
            bool hasO = false;
            for (int k : a.bonds) {
                if (k < 0) continue;
                if (atoms[k].elementId == 0) hCount++;
                else if (atoms[k].elementId == 1) hasO = true;
            }
            if (hCount == 3 && !hasO) n++;
        }
        return n;
        };

    // Одиночный Cl: нет ни одной ковалентной связи
// (ни HCl, ни Cl2, ни NaCl). H-связи игнорируем — они не «склеивают» Cl.
    auto countSingleCl = [](const std::vector<Atom>& atoms) {
        int n = 0;
        for (const auto& a : atoms) {
            if (a.elementId != 2) continue;             // Cl
            if (countBonds(a) == 0) n++;
        }
        return n;
        };

    // HCl: H, связанный ровно с одним Cl.
    // У H maxBonds = 1, поэтому каждый H имеет максимум одну связь —
    // достаточно посчитать все H, у которых партнёр — Cl.
    // (Проверка «k > i» ошибочно отбрасывала HCl, если Cl был
    //  заспавнен раньше H — а так и происходит в реальной игре.)
    auto countHClMolecules = [](const std::vector<Atom>& atoms) {
        int n = 0;
        for (const auto& a : atoms) {
            if (a.elementId != 0) continue;   // H
            for (int k : a.bonds) {
                if (k < 0 || k >= (int)atoms.size()) continue;
                if (atoms[k].elementId == 2) { n++; break; }
            }
        }
        return n;
        };

    ui.showDetailedAtoms = mainMenu.getSettings().detailedAtoms;
    ui.fxaaEnabled = mainMenu.getSettings().fxaaEnabled;
    ui.language = mainMenu.getSettings().language;

    auto commitEdit = [&]() {
        if (ui.editingCountIndex < 0) return;
        int val = 10;
        if (!ui.editBuffer.empty()) {
            try { val = std::stoi(ui.editBuffer); }
            catch (...) { val = 10; }
        }
        val = std::clamp(val, 0, 500);
        ui.batchCounts[ui.editingCountIndex] = val;
        ui.editingCountIndex = -1;
        ui.editBuffer.clear();
        };

    auto commitTempEdit = [&]() {
        if (!ui.tempEditing) return;
        try {
            float val = std::stof(ui.tempEditBuffer);
            if (val < TEMP_MIN_C) val = TEMP_MIN_C;
            ui.targetTempCelsius = val;
        }
        catch (...) {}
        ui.tempEditing = false;
        ui.tempEditBuffer.clear();
        };

    const float RMB_MENU_W = 160.0f;
    const float RMB_ITEM_H = 30.0f;
    const int   RMB_ITEMS = 3;
    const float RMB_MENU_H = RMB_ITEM_H * RMB_ITEMS + 4.0f;

    while (window.isOpen()) {
        sf::Vector2u winSize = window.getSize();
        const float trackY = PANEL_HEIGHT / 2.0f;
        const float trackRight = SLIDER_LEFT + SLIDER_WIDTH;

        float S = std::max(1.0f, (float)winSize.y / 1080.0f);

        // Масштабированные метрики меню (нужны для hit-тестов)
        const float mLeft = MENU_LEFT * S;
        const float mTop = PANEL_HEIGHT + 10.0f * S;
        const float mWidth = MENU_WIDTH * S;
        const float mHeaderH = MENU_HEADER_H * S;
        const float mRowH = MENU_ROW_H * S;
        const float mFooterH = MENU_FOOTER_H * S;
        // Количество строк в spawn-меню зависит от режима/уровня.
        // В L2 доступны H и O — 2 строки. Это должно совпадать
        // с rowCount в UI.cpp, иначе hit-тесты промахиваются.
        int spawnRowCount;
        if (!ui.campaignMode)               spawnRowCount = (int)ATOM_TYPES.size() + 1;
        else if (ui.campaignLevel == 1)     spawnRowCount = 1;
        else if (ui.campaignLevel == 2)     spawnRowCount = 2;
        else if (ui.campaignLevel == 3)     spawnRowCount = 3;
        else if (ui.campaignLevel == 4)     spawnRowCount = 3;  // H, O, Water
        else if (ui.campaignLevel == 6)     spawnRowCount = 0;  // ← НОВОЕ: L6 без меню
        else                                spawnRowCount = (int)ATOM_TYPES.size() + 1;
        const float mTotalH = mHeaderH + (float)spawnRowCount * mRowH + mFooterH;

        const float rIconX = ROW_ICON_X * S;
        const float rNameX = ROW_NAME_X * S;
        const float rMinusX = ROW_MINUS_X * S;
        const float rFieldX = ROW_FIELD_X * S;
        const float rFieldW = ROW_FIELD_W * S;
        const float rPlusX = ROW_PLUS_X * S;
        const float rBtnW = ROW_BTN_W * S;
        const float rBtnH = ROW_BTN_H * S;
        const float rSelectW = ROW_SELECT_W * S;

        sf::FloatRect menuRect({ mLeft, mTop }, { mWidth, mTotalH });

        // --- Box panel geometry (для hit-теста) ---
        const float bpLeft = BOX_PANEL_LEFT * S;
        const float bpWidth = BOX_PANEL_WIDTH * S;
        const float bpHeight = BOX_PANEL_HEIGHT * S;
        const float bpTop = (float)winSize.y - BOX_PANEL_BOTTOM_OFFSET * S - bpHeight;
        sf::FloatRect boxPanelRect({ bpLeft, bpTop }, { bpWidth, bpHeight });

        const float bTrackX1 = bpLeft + BOX_TRACK_X * S;
        const float bTrackX2 = bTrackX1 + BOX_TRACK_W * S;
        const float bRow1Y = bpTop + BOX_ROW1_Y * S;
        const float bRow2Y = bpTop + BOX_ROW2_Y * S;

        // --- Gravity panel geometry ---
        const float gpW = GRAV_PANEL_W * S;
        const float gpH = GRAV_PANEL_H * S;
        const float gpRight = (float)winSize.x - GRAV_PANEL_RIGHT_MARGIN * S;
        const float gpLeft = gpRight - gpW;
        const float gpTop = (float)winSize.y - GRAV_PANEL_BOTTOM_OFFSET * S - gpH;
        sf::FloatRect gravPanelRect({ gpLeft, gpTop }, { gpW, gpH });

        const float gpCheckX = gpLeft + GRAV_CHECKBOX_X * S;
        const float gpCheckY = gpTop + GRAV_ROW1_Y * S - GRAV_CHECKBOX_SIZE * S * 0.5f;
        const float gpSliderX1 = gpLeft + GRAV_SLIDER_X * S;
        const float gpSliderX2 = gpSliderX1 + GRAV_SLIDER_W * S;
        const float gpRow2Y = gpTop + GRAV_ROW2_Y * S;
        const float gpRow3Y = gpTop + GRAV_ROW3_Y * S;

        // ============================================================
        // События
        // ============================================================
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();

            // ====================================================
            // Глобальная обработка (работает и в меню, и в симуляции):
            // F11 (fullscreen) и Resized.
            // ====================================================
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::F11) {
                    isFullscreen = !isFullscreen;
                    if (isFullscreen) {
                        window.create(sf::VideoMode::getDesktopMode(),
                            "H2 Fusion Simulation",
                            sf::Style::Default, sf::State::Fullscreen);
                    }
                    else {
                        window.create(sf::VideoMode({ 1000, 700 }),
                            "H2 Fusion Simulation",
                            sf::Style::Default, sf::State::Windowed);
                    }
                    window.setFramerateLimit(60);
                    if (iconLoaded) window.setIcon(appIcon);   // ← вернуть иконку
                    float aspect = (float)window.getSize().x / (float)window.getSize().y;
                    sf::Vector2f size = camera.getSize();
                    size.x = size.y * aspect;
                    camera.setSize(size);
                    winSize = window.getSize();
                    continue;
                }
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                float aspect = (float)resized->size.x / (float)resized->size.y;
                sf::Vector2f size = camera.getSize();
                size.x = size.y * aspect;
                camera.setSize(size);
                winSize = { resized->size.x, resized->size.y };
                continue;
            }


            // ====================================================
            // Если в меню — остальные события идут только туда
            // ====================================================
            if (state == GameState::MainMenu) {
                int action = mainMenu.handleEvent(*event, winSize, rng);
                if (action == 0) {
                    // Sandbox → обычная симуляция.
                    state = GameState::Simulation;
                    ui.campaignMode = false;
                    ui.campaignLevel = 0;

                    if (ui.sandboxEverEntered) {
                        // Уже играли в песочнице — спрашиваем, что делать
                        // с прошлой картой.
                        ui.sandboxAskDialog = true;
                    }
                    else {
                        // Первый заход — сразу ставим стандартную карту,
                        // без бессмысленного диалога.
                        atoms.clear();
                        walls.clear();
                        neutrons.clear();
                        gammas.clear();
                        delayedPool.clear();
                        ui.pillars.clear();
                        atoms.push_back(makeHydrogen({ -2.0f, 0.0f },
                            { 0.5f, 0.0f }));
                        atoms.push_back(makeHydrogen({ 2.0f, 0.0f },
                            { -0.5f, 0.0f }));
                        ui.boxSizeX = BOX_DEFAULT;
                        ui.boxSizeY = BOX_DEFAULT;
                        ui.targetTempCelsius = TEMP_DEFAULT_C;
                        ui.gravityEnabled = false;
                        ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                        ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                        ui.speedSlider.value = 1.0f;
                        ui.savedSpeedValue = 1.0f;
                        ui.focusedAtomIndex = -1;
                        ui.stockLimited = false;
                        ui.stockH = 0;
                        ui.stockO = 0;
                        ui.spawnCooldownTimer = 0.0f;
                        physStepAccumulator = 0.0f;
                        ui.sandboxEverEntered = true;
                    }
                }
                else if (action == 2) window.close();            // Exit
                else if (action == 4) {
                    // Campaign level 2 → Water's many faces
                    state = GameState::Simulation;
                    ui.campaignMode = true;
                    ui.campaignLevel = 2;

                    ui.targetTempCelsius = 80.0f;
                    ui.tempSliderDragging = false;
                    ui.tempEditing = false;

                    ui.gravityDirDragging = false;
                    ui.gravityMagDragging = false;
                    ui.boxSizeXDragging = false;
                    ui.boxSizeYDragging = false;
                    ui.selectedSpawnType = -1;
                    ui.focusedAtomIndex = -1;

                    // ← СБРОС КАРТЫ ОТ ПРОШЛЫХ УРОВНЕЙ
                    ui.boxSizeX = BOX_DEFAULT;
                    ui.boxSizeY = BOX_DEFAULT;
                    ui.gravityEnabled = false;
                    ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                    ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                    ui.pillars.clear();
                    ui.barriers.clear();
                    ui.activeSpawnZone.active = false;

                    {
                        float aspect = (float)winSize.x / (float)winSize.y;
                        sf::Vector2f cs(21.0f * aspect, 21.0f);
                        camera.setSize(cs);
                        camera.setCenter({ 0.0f, 0.0f });
                    }

                    atoms.clear();
                    walls.clear();
                    neutrons.clear();
                    gammas.clear();
                    delayedPool.clear();

                    // Доступны H и O
                    ui.batchCounts.assign(ATOM_TYPES.size(), 0);
                    ui.batchCounts[0] = 10;   // Hydrogen
                    ui.batchCounts[1] = 5;    // Oxygen
                    physStepAccumulator = 0.0f;

                    ui.spawnMenuOpen = true;

                    ui.taskPhase = 1;
                    ui.taskH2Count = 0;
                    ui.taskH2MaxSeen = 0;
                    ui.taskH2Target = 5;
                    ui.taskH2Complete = false;
                    ui.taskH2OCount = 0;
                    ui.taskH2O2Count = 0;
                    ui.taskHBondCount = 0;
                    ui.hintButtonOpen = false;
                    ui.taskTextAlpha = 1.0f;
                    ui.nextLevelButtonHovered = false;
                    ui.copyPendingSpawn = false;
                }
                else if (action == 3) {
                    // Campaign level 1 → Origins of chemistry
                    state = GameState::Simulation;
                    ui.campaignMode = true;
                    ui.campaignLevel = 1;

                    // Фиксируем температуру среды
                    ui.targetTempCelsius = -15.0f;
                    ui.tempSliderDragging = false;
                    ui.tempEditing = false;

                    // Гасим всё, что могло быть активно из прошлых сессий
                    ui.gravityDirDragging = false;
                    ui.gravityMagDragging = false;
                    ui.boxSizeXDragging = false;
                    ui.boxSizeYDragging = false;
                    ui.selectedSpawnType = -1;
                    ui.focusedAtomIndex = -1;

                    // ← СБРОС КАРТЫ ОТ ПРОШЛЫХ УРОВНЕЙ:
                    //   размер коробки, гравитация, препятствия, борта,
                    //   зона спавна L6.
                    ui.boxSizeX = BOX_DEFAULT;
                    ui.boxSizeY = BOX_DEFAULT;
                    ui.gravityEnabled = false;
                    ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                    ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                    ui.pillars.clear();
                    ui.barriers.clear();
                    ui.activeSpawnZone.active = false;

                    // Сбрасываем камеру к дефолтному виду (иначе после
                    // L4 она может быть отзумлена под 60×36).
                    {
                        float aspect = (float)winSize.x / (float)winSize.y;
                        sf::Vector2f cs(21.0f * aspect, 21.0f);
                        camera.setSize(cs);
                        camera.setCenter({ 0.0f, 0.0f });
                    }

                    // Пустая коробка, только водород в batchCounts
                    atoms.clear();
                    walls.clear();
                    neutrons.clear();
                    gammas.clear();
                    delayedPool.clear();
                    ui.batchCounts.assign(ATOM_TYPES.size(), 0);
                    ui.batchCounts[0] = 10;
                    physStepAccumulator = 0.0f;

                    // Меню спавна открыто, т.к. это обучающий уровень
                    ui.spawnMenuOpen = true;

                    // Сбрасываем прогресс задач и висящие pending-флаги
                    ui.taskPhase = 1;
                    ui.taskH2Count = 0;
                    ui.taskH2MaxSeen = 0;
                    ui.taskH2Target = 5;
                    ui.taskH2Complete = false;
                    ui.taskTextAlpha = 1.0f;
                    ui.nextLevelButtonHovered = false;
                    ui.copyPendingSpawn = false;
                    ui.hintButtonOpen = false;
                }
                else if (action == 5) {
                    // Campaign level 3 → Acids and ions
                    state = GameState::Simulation;
                    ui.campaignMode = true;
                    ui.campaignLevel = 3;

                    // Начальная температура среды
                    ui.targetTempCelsius = -15.0f;
                    ui.tempSliderDragging = false;
                    ui.tempEditing = false;

                    ui.gravityDirDragging = false;
                    ui.gravityMagDragging = false;
                    ui.boxSizeXDragging = false;
                    ui.boxSizeYDragging = false;
                    ui.selectedSpawnType = -1;
                    ui.focusedAtomIndex = -1;

                    // ← СБРОС КАРТЫ ОТ ПРОШЛЫХ УРОВНЕЙ
                    ui.boxSizeX = BOX_DEFAULT;
                    ui.boxSizeY = BOX_DEFAULT;
                    ui.gravityEnabled = false;
                    ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                    ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                    ui.pillars.clear();
                    ui.barriers.clear();
                    ui.activeSpawnZone.active = false;

                    {
                        float aspect = (float)winSize.x / (float)winSize.y;
                        sf::Vector2f cs(21.0f * aspect, 21.0f);
                        camera.setSize(cs);
                        camera.setCenter({ 0.0f, 0.0f });
                    }

                    atoms.clear();
                    walls.clear();
                    neutrons.clear();
                    gammas.clear();
                    delayedPool.clear();

                    // Доступны H, O, Cl
                    ui.batchCounts.assign(ATOM_TYPES.size(), 0);
                    ui.batchCounts[0] = 10;   // Hydrogen
                    ui.batchCounts[1] = 5;    // Oxygen
                    ui.batchCounts[2] = 5;    // Chlorine
                    physStepAccumulator = 0.0f;

                    ui.spawnMenuOpen = true;

                    ui.taskPhase = 0;
                    ui.taskH2Count = 0;
                    ui.taskH2MaxSeen = 0;
                    ui.taskH2Target = 5;
                    ui.taskH2Complete = false;
                    ui.taskH2OCount = 0;
                    ui.taskH2O2Count = 0;
                    ui.taskHBondCount = 0;
                    ui.taskHClCount = 0;
                    ui.taskH3OCount = 0;
                    ui.taskSingleClCount = 0;
                    ui.taskSingleClBest = 0;
                    ui.taskClHoldTimer = 0.0f;
                    ui.taskClTimerRunning = false;
                    ui.taskClFlashTimer = 0.0f;
                    ui.hintButtonOpen = false;
                    ui.taskTextAlpha = 1.0f;
                    ui.nextLevelButtonHovered = false;
                    ui.copyPendingSpawn = false;
                }
                else if (action == 6) {
                    // Campaign level 4 → Molecular bridge
                    state = GameState::Simulation;
                    ui.campaignMode = true;
                    ui.campaignLevel = 4;

                    ui.boxSizeX = L4_BOX_W;
                    ui.boxSizeY = L4_BOX_H;

                    ui.targetTempCelsius = L4_TEMP_C;
                    ui.tempSliderDragging = false;
                    ui.tempEditing = false;

                    ui.gravityEnabled = true;
                    ui.gravityMagnitude = L4_GRAVITY_MAG;
                    ui.gravityDirDeg = L4_GRAVITY_DIR_DEG;
                    ui.gravityDirDragging = false;
                    ui.gravityMagDragging = false;

                    ui.speedSlider.value = 1.0f;
                    ui.savedSpeedValue = 1.0f;

                    ui.boxSizeXDragging = false;
                    ui.boxSizeYDragging = false;
                    ui.selectedSpawnType = -1;
                    ui.spawnWaterSelected = false;
                    ui.waterSpawnRotation = 0.0f;
                    ui.focusedAtomIndex = -1;

                    atoms.clear();
                    walls.clear();
                    neutrons.clear();
                    gammas.clear();
                    delayedPool.clear();
                    ui.pillars.clear();
                    ui.pillars.push_back({ { -L4_PILLAR_X, 0.0f }, L4_PILLAR_HALF_SIZE });
                    ui.pillars.push_back({ {  0.0f,        0.0f }, L4_PILLAR_HALF_SIZE });
                    ui.pillars.push_back({ {  L4_PILLAR_X, 0.0f }, L4_PILLAR_HALF_SIZE });

                    ui.batchCounts.assign(3, 0);
                    ui.batchCounts[0] = 5;   // H
                    ui.batchCounts[1] = 5;   // O
                    ui.batchCounts[2] = 5;   // Water
                    physStepAccumulator = 0.0f;

                    ui.stockLimited = true;
                    ui.stockH = L4_START_H;
                    ui.stockO = L4_START_O;
                    ui.spawnCooldownTimer = 0.0f;

                    ui.taskPhase = 1;
                    ui.taskTextAlpha = 1.0f;
                    ui.taskPillarsConnected = 0;
                    ui.taskBridgeHoldTimer = 0.0f;
                    ui.taskBridgeTimerRunning = false;
                    ui.nextLevelButtonHovered = false;
                    ui.copyPendingSpawn = false;
                    ui.hintButtonOpen = false;
                }
                else if (action == 7) {
                    // Campaign level 6 → Neutron Basketball
                    state = GameState::Simulation;
                    ui.campaignMode = true;
                    ui.campaignLevel = 6;

                    ui.spawnMenuOpen = false;    // ← НОВОЕ: на L6 меню спавна не нужно

                    ui.boxSizeX = L6_BOX_W;
                    ui.boxSizeY = L6_BOX_H;

                    ui.targetTempCelsius = L6_TEMP_C;
                    ui.tempSliderDragging = false;
                    ui.tempEditing = false;

                    ui.gravityEnabled = true;
                    ui.gravityMagnitude = L6_GRAVITY_MAG;
                    ui.gravityDirDeg = L6_GRAVITY_DIR_DEG;
                    ui.gravityDirDragging = false;
                    ui.gravityMagDragging = false;

                    ui.speedSlider.value = 1.0f;
                    ui.savedSpeedValue = 1.0f;

                    ui.boxSizeXDragging = false;
                    ui.boxSizeYDragging = false;
                    ui.selectedSpawnType = -1;
                    ui.spawnWaterSelected = false;
                    ui.waterSpawnRotation = 0.0f;
                    ui.focusedAtomIndex = -1;

                    atoms.clear();
                    walls.clear();
                    neutrons.clear();
                    gammas.clear();
                    delayedPool.clear();
                    ui.pillars.clear();
                    ui.barriers.clear();

                    // Стартовая площадка
                    ui.pillars.push_back({ { L6_START_X, L6_START_Y },
                        L6_PILLAR_HALF_SIZE });

                    // 4 кольца-корзины + борта
                    for (int i = 0; i < L6_RING_COUNT; ++i) {
                        sf::Vector2f c{ L6_RING_X[i], L6_RING_Y[i] };
                        ui.pillars.push_back({ c, L6_PILLAR_HALF_SIZE });

                        // Борта: слева и справа от колонны, чуть выше
                        // центра — на уровне, где лежит уран.
                        sf::Vector2f bL = c + sf::Vector2f(
                            -(L6_PILLAR_HALF_SIZE + L6_BARRIER_HALF_W),
                            -L6_URANIUM_Y_OFFSET + 0.5f);
                        sf::Vector2f bR = c + sf::Vector2f(
                            +(L6_PILLAR_HALF_SIZE + L6_BARRIER_HALF_W),
                            -L6_URANIUM_Y_OFFSET + 0.5f);
                        ui.barriers.push_back({ bL,
                            { L6_BARRIER_HALF_W, L6_BARRIER_HALF_H } });
                        ui.barriers.push_back({ bR,
                            { L6_BARRIER_HALF_W, L6_BARRIER_HALF_H } });
                    }

                    // Активная spawn-зона — на стартовой площадке
                    ui.activeSpawnZone.pos = { L6_START_X, L6_START_Y - 3.0f };
                    ui.activeSpawnZone.radius = L6_SPAWN_ZONE_RADIUS;
                    ui.activeSpawnZone.active = true;

                    ui.batchCounts.assign(ATOM_TYPES.size() + 1, 0);
                    physStepAccumulator = 0.0f;

                    ui.ringIndex = 0;
                    ui.ringUraniumTotal = L6_URANIUM_PER_RING;
                    ui.ringUraniumRemaining = 0;   // заполним ниже

                    ui.taskPhase = 1;
                    ui.taskTextAlpha = 1.0f;
                    ui.cameraTransitionActive = false;
                    ui.phaseDelayTimer = 0.0f;
                    ui.nextLevelButtonHovered = false;
                    ui.copyPendingSpawn = false;
                    ui.hintButtonOpen = false;
                    ui.stockLimited = false;

                    // Спавним уран на первом кольце
                    {
                        int idx = ui.ringIndex;
                        sf::Vector2f c{ L6_RING_X[idx], L6_RING_Y[idx] };
                        float baseY = c.y - L6_URANIUM_Y_OFFSET;
                        float totalW = (L6_URANIUM_PER_RING - 1)
                            * L6_URANIUM_SPACING;
                        float x0 = c.x - totalW * 0.5f;
                        for (int k = 0; k < L6_URANIUM_PER_RING; ++k) {
                            sf::Vector2f p(x0 + k * L6_URANIUM_SPACING, baseY);
                            atoms.push_back(makeAtom(3, p, { 0.0f, 0.0f }));
                            ui.ringUraniumRemaining++;
                        }
                    }
                }
                // Помечаем, что игрок уже был в симуляции (песочница
                // или кампания). Тогда при следующем входе в песочницу
                // покажем диалог выбора карты — независимо от того,
                // выходил игрок из песочницы или из кампании.
                if (action == 0 || action == 3 || action == 4
                    || action == 5 || action == 6 || action == 7)
                {
                    ui.sandboxEverEntered = true;
                }

                // action == 1 → Settings (не используется)
                continue;
            }

            // ====================================================
// Диалог выбора карты песочницы: пока он открыт, все
// события идут только сюда.
// ====================================================
            if (ui.sandboxAskDialog) {
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        sf::Vector2i mp = sf::Mouse::getPosition(window);
                        sf::Vector2f mpF((float)mp.x, (float)mp.y);

                        float S2 = std::max(1.0f, (float)winSize.y / 1080.0f);
                        const float bw = 320.0f * S2;
                        const float bh = 60.0f * S2;
                        const float gap = 20.0f * S2;
                        const float cx = (float)winSize.x * 0.5f;
                        const float btnY = (float)winSize.y * 0.55f;

                        sf::FloatRect defBtn({ cx - bw - gap * 0.5f,
                                               btnY - bh * 0.5f },
                            { bw, bh });
                        sf::FloatRect keepBtn({ cx + gap * 0.5f,
                                                btnY - bh * 0.5f },
                            { bw, bh });

                        if (defBtn.contains(mpF)) {
                            // Стандартная песочница: полный сброс к
                            // исходному состоянию.
                            atoms.clear();
                            walls.clear();
                            neutrons.clear();
                            gammas.clear();
                            delayedPool.clear();
                            ui.pillars.clear();
                            atoms.push_back(makeHydrogen({ -2.0f, 0.0f },
                                { 0.5f, 0.0f }));
                            atoms.push_back(makeHydrogen({ 2.0f, 0.0f },
                                { -0.5f, 0.0f }));
                            ui.boxSizeX = BOX_DEFAULT;
                            ui.boxSizeY = BOX_DEFAULT;
                            ui.targetTempCelsius = TEMP_DEFAULT_C;
                            ui.gravityEnabled = false;
                            ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                            ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                            ui.speedSlider.value = 1.0f;
                            ui.savedSpeedValue = 1.0f;
                            ui.focusedAtomIndex = -1;
                            ui.stockLimited = false;
                            ui.stockH = 0;
                            ui.stockO = 0;
                            ui.spawnCooldownTimer = 0.0f;
                            physStepAccumulator = 0.0f;
                            ui.sandboxAskDialog = false;
                            ui.sandboxEverEntered = true;
                        }
                        else if (keepBtn.contains(mpF)) {
                            // Оставить прошлую карту — просто закрываем.
                            ui.sandboxAskDialog = false;
                            ui.sandboxEverEntered = true;
                        }
                    }
                    continue;
                }
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Escape) {
                        ui.sandboxAskDialog = false;
                        ui.sandboxEverEntered = true;   // ← добавить
                        continue;
                    }
                }
                // Все прочие события глушим, пока диалог открыт.
                continue;
            }


            // ====================================================
            // Pause-меню открыто: обрабатываем только его.
            // Все прочие события потребляем, чтобы UI под ним
            // не реагировал на клики и скролл.
            // ====================================================
            if (ui.pauseMenuOpen) {
                // -------- Подменю настроек внутри паузы --------
                if (ui.pauseSettingsOpen) {
                    const float sW = 460.0f * S;
                    const float sH = 52.0f * S;
                    const float sGap = 14.0f * S;
                    const float sCX = (float)winSize.x * 0.5f;
                    const float sCY = (float)winSize.y * 0.48f;
                    // 3 строки: 0 = Detailed Atoms, 1 = FXAA, 2 = Language
                    auto sRowRect = [&](int idx) {
                        float y = sCY + (idx - 1.0f) * (sH + sGap);
                        return sf::FloatRect({ sCX - sW * 0.5f, y - sH * 0.5f }, { sW, sH });
                        };
                    const float bkW = 220.0f * S, bkH = 52.0f * S;
                    sf::FloatRect bkRect({ sCX - bkW * 0.5f,
                                            (float)winSize.y * 0.78f - bkH * 0.5f },
                        { bkW, bkH });

                    // Прямоугольник i-го пункта dropdown языка.
// Открывается ПОД третьей строкой (idx = 2),
// колонка справа — как в главном меню.
                    const float itemH = 40.0f * S;
                    auto langItemRect = [&](int idx) {
                        float rowY = sCY + (2 - 1.0f) * (sH + sGap);
                        float rowBottom = rowY + sH * 0.5f;
                        float ix = sCX + sW * 0.5f - sW * 0.55f;
                        float iw = sW * 0.55f;
                        float iy = rowBottom + idx * itemH;
                        return sf::FloatRect({ ix, iy }, { iw, itemH });
                        };

                    if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                        if (mb->button == sf::Mouse::Button::Left) {
                            sf::Vector2i mp = sf::Mouse::getPosition(window);
                            sf::Vector2f mpF((float)mp.x, (float)mp.y);
                            auto cur = mainMenu.getSettings();

                            // 1) Открытый dropdown — обрабатываем в первую очередь
                            if (ui.pauseLanguageDropdownOpen) {
                                bool handledItem = false;
                                for (int i = 0; i < 3; ++i) {
                                    if (langItemRect(i).contains(mpF)) {
                                        MainMenu::Settings s = cur;
                                        if (i == 0)      s.language = Language::EN;
                                        else if (i == 1) s.language = Language::RU;
                                        else             s.language = Language::BROKEN;
                                        mainMenu.setSettings(s);
                                        ui.pauseLanguageDropdownOpen = false;
                                        handledItem = true;
                                        break;
                                    }
                                }
                                if (!handledItem) {
                                    // Клик мимо пунктов — закрыть dropdown
                                    ui.pauseLanguageDropdownOpen = false;
                                }
                            }
                            // 2) Иначе — обычные строки
                            else if (sRowRect(0).contains(mpF)) {
                                MainMenu::Settings s = cur;
                                s.detailedAtoms = !cur.detailedAtoms;
                                mainMenu.setSettings(s);
                            }
                            else if (sRowRect(1).contains(mpF)) {
                                MainMenu::Settings s = cur;
                                s.fxaaEnabled = !cur.fxaaEnabled;
                                mainMenu.setSettings(s);
                            }
                            else if (sRowRect(2).contains(mpF)) {
                                // Открываем dropdown
                                ui.pauseLanguageDropdownOpen = true;
                            }
                            else if (bkRect.contains(mpF)) {
                                ui.pauseSettingsOpen = false;
                                ui.pauseLanguageDropdownOpen = false;
                            }
                        }
                        continue;
                    }
                    if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                        if (kp->code == sf::Keyboard::Key::Escape) {
                            if (ui.pauseLanguageDropdownOpen) {
                                ui.pauseLanguageDropdownOpen = false;
                            }
                            else {
                                ui.pauseSettingsOpen = false;
                            }
                            continue;
                        }
                    }
                    continue;
                }

                // -------- Основное пауз-меню --------
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        sf::Vector2i mp = sf::Mouse::getPosition(window);
                        sf::Vector2f mpF((float)mp.x, (float)mp.y);
                        for (int i = 0; i < 3; ++i) {
                            if (pauseMenuButtonRect(winSize, i).contains(mpF)) {
                                if (i == 0) { ui.pauseMenuOpen = false; }
                                else if (i == 1) {
                                    ui.pauseSettingsOpen = true;
                                    ui.pauseLanguageDropdownOpen = false;
                                }
                                else if (i == 2) {
                                    state = GameState::MainMenu;
                                    ui.pauseMenuOpen = false;
                                    ui.campaignMode = false;
                                    ui.campaignLevel = 0;
                                    mainMenu.init(rng);
                                }
                                break;
                            }
                        }
                    }
                    continue;
                }
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Escape) {
                        ui.pauseMenuOpen = false;
                        continue;
                    }
                }
                continue;
            }

            if (const auto* text = event->getIf<sf::Event::TextEntered>()) {
                std::uint32_t u = text->unicode;
                if (ui.editingCountIndex >= 0) {
                    if (u >= '0' && u <= '9' && ui.editBuffer.size() < 3) {
                        ui.editBuffer += static_cast<char>(u);
                    }
                }
                else if (ui.tempEditing) {
                    bool isDigit = (u >= '0' && u <= '9');
                    bool isDot = (u == '.');
                    bool isMinus = (u == '-' && ui.tempEditBuffer.empty());
                    bool dotOk = (!isDot) || (ui.tempEditBuffer.find('.') == std::string::npos);
                    if ((isDigit || (isDot && dotOk) || isMinus) && ui.tempEditBuffer.size() < 15) {
                        ui.tempEditBuffer += static_cast<char>(u);
                    }
                }
            }

            if (const auto* scrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (scrolled->wheel == sf::Mouse::Wheel::Vertical) {
                    sf::Vector2i mp = sf::Mouse::getPosition(window);
                    bool overTop = mp.y < (int)PANEL_HEIGHT;
                    bool overMenu = ui.spawnMenuOpen && menuRect.contains({ (float)mp.x, (float)mp.y });

                    bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
                    if (ctrlHeld && !overTop && !overMenu
                        && ui.spawnWaterSelected)
                    {
                        // Поворот превью H2O
                        float rotStep = (scrolled->delta > 0) ? 15.0f : -15.0f;
                        ui.waterSpawnRotation += rotStep * 3.14159265f / 180.0f;
                    }
                    else if (ui.copyMode && ctrlHeld && !overTop && !overMenu) {
                        float rotStep = (scrolled->delta > 0) ? 15.0f : -15.0f;
                        ui.copyRotation += rotStep * 3.14159265f / 180.0f;
                    }
                    else {
                        float zoomFactor = (scrolled->delta > 0) ? 0.9f : 1.1f;
                        sf::Vector2i pivotPixel = mp;
                        if (ui.focusedAtomIndex >= 0 && ui.focusedAtomIndex < (int)atoms.size()) {
                            pivotPixel = window.mapCoordsToPixel(atoms[ui.focusedAtomIndex].pos, camera);
                        }
                        if (!overTop && !overMenu) {
                            sf::Vector2f mwBefore = window.mapPixelToCoords(pivotPixel, camera);
                            sf::Vector2f newSize = camera.getSize() * zoomFactor;
                            if (newSize.x >= 0.001f && newSize.x <= 2000.0f) {
                                camera.setSize(newSize);
                                sf::Vector2f mwAfter = window.mapPixelToCoords(pivotPixel, camera);
                                camera.move(mwBefore - mwAfter);
                            }
                        }
                    }
                }
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2i mp = sf::Mouse::getPosition(window);
                    sf::Vector2f mpF((float)mp.x, (float)mp.y);
                    bool handled = false;

                    // Commit pending edits if click outside
                    if (ui.editingCountIndex >= 0) {
                        float rowTop = mTop + mHeaderH + ui.editingCountIndex * mRowH;
                        sf::FloatRect fieldRect(
                            { mLeft + rFieldX, rowTop + (mRowH - rBtnH) / 2.0f },
                            { rFieldW, rBtnH });
                        if (!fieldRect.contains(mpF)) commitEdit();
                    }
                    if (ui.tempEditing) {
                        sf::FloatRect tFieldRect(
                            { TEMP_FIELD_LEFT, trackY - TEMP_FIELD_H / 2.0f },
                            { TEMP_FIELD_W, TEMP_FIELD_H });
                        if (!tFieldRect.contains(mpF)) commitTempEdit();
                    }

                    // RMB-меню
                    if (ui.rmbMenuOpen) {
                        float mx = (float)ui.rmbMenuPos.x;
                        float my = (float)ui.rmbMenuPos.y;
                        if (mx + RMB_MENU_W > (float)winSize.x) mx = (float)winSize.x - RMB_MENU_W;
                        if (my + RMB_MENU_H > (float)winSize.y) my = (float)winSize.y - RMB_MENU_H;

                        sf::FloatRect menuBounds({ mx, my }, { RMB_MENU_W, RMB_MENU_H });
                        if (menuBounds.contains(mpF)) {
                            int idx = (int)((mpF.y - my - 2.0f) / RMB_ITEM_H);
                            idx = std::clamp(idx, 0, RMB_ITEMS - 1);

                            if (idx == 0) {
                                if (!(ui.campaignMode && ui.campaignLevel == 4)) {
                                    sf::Vector2f centroid(0.0f, 0.0f);
                                    int cnt = 0;
                                    for (const auto& a : atoms) if (a.selected) { centroid += a.pos; cnt++; }
                                    for (const auto& w : walls) if (w.selected) {
                                        centroid += (w.a + w.b) * 0.5f; cnt++;
                                    }
                                    if (cnt > 0) {
                                        centroid /= (float)cnt;
                                        ui.copyOffsets.clear();
                                        ui.copyTemplates.clear();
                                        ui.copyWallA.clear();
                                        ui.copyWallB.clear();
                                        for (const auto& a : atoms) {
                                            if (!a.selected) continue;
                                            ui.copyOffsets.push_back(a.pos - centroid);
                                            ui.copyTemplates.push_back(a);
                                        }
                                        for (const auto& w : walls) {
                                            if (!w.selected) continue;
                                            ui.copyWallA.push_back(w.a - centroid);
                                            ui.copyWallB.push_back(w.b - centroid);
                                        }
                                        ui.copyMode = true;
                                        ui.copyRotation = 0.0f;
                                    }
                                }
                            }
                            else if (idx == 1) {
                                // L4: возвращаем запас за удаляемые атомы
                                if (ui.campaignMode && ui.campaignLevel == 4) {
                                    for (const auto& a : atoms) {
                                        if (!a.selected) continue;
                                        if (a.elementId == 0) ui.stockH++;
                                        else if (a.elementId == 1) ui.stockO++;
                                    }
                                }
                                // Delete: атомы + стены, с корректным ремапом индексов

                                // 1) Строим old→new маппинг
                                std::vector<int> remap(atoms.size(), -1);
                                int newIdx = 0;
                                for (size_t k = 0; k < atoms.size(); ++k) {
                                    if (!atoms[k].selected) remap[k] = newIdx++;
                                }

                                // 2) Перенаправляем все ссылки
                                for (auto& a : atoms) {
                                    if (a.selected) continue;
                                    for (auto& b : a.bonds) {
                                        if (b < 0) continue;
                                        b = (b < (int)remap.size()) ? remap[b] : -1;
                                    }
                                    for (auto& b : a.hbonds) {
                                        if (b < 0) continue;
                                        b = (b < (int)remap.size()) ? remap[b] : -1;
                                    }
                                }

                                // 3) Удаляем — теперь все индексы согласованы
                                atoms.erase(std::remove_if(atoms.begin(), atoms.end(),
                                    [](const Atom& a) { return a.selected; }), atoms.end());

                                walls.erase(std::remove_if(walls.begin(), walls.end(),
                                    [](const Wall& w) { return w.selected; }), walls.end());

                                ui.focusedAtomIndex = -1;
                            }
                            else if (idx == 2) {
                                for (auto& a : atoms) a.selected = false;
                                for (auto& w : walls) w.selected = false;
                            }
                            ui.rmbMenuOpen = false;
                            handled = true;
                        }
                        else {
                            ui.rmbMenuOpen = false;
                        }
                    }

                    // Copy mode — pending spawn: ничего не создаём сейчас,
                    // только запоминаем точку. Атомы и стены появятся при
                    // отпускании ЛКМ, уже с нужной скоростью.
                    else if (ui.copyMode) {
                        sf::Vector2f worldPos = window.mapPixelToCoords(mp, camera);
                        float halfX = ui.boxSizeX / 2.0f;
                        float halfY = ui.boxSizeY / 2.0f;
                        worldPos.x = std::clamp(worldPos.x, -halfX, halfX);
                        worldPos.y = std::clamp(worldPos.y, -halfY, halfY);

                        ui.spawnDragActive = true;
                        ui.spawnDragOrigin = worldPos;
                        ui.spawnDragAtomIndices.clear();
                        ui.spawnDragNeutronIndices.clear();
                        ui.copyPendingSpawn = true;

                        handled = true;
                    }

                    if (!handled) {
                        // Кнопка "Next Level" — видна в финальной фазе уровня.
                        if (ui.campaignMode && ui.campaignLevel == 1
                            && ui.taskPhase == 3)
                        {
                            sf::FloatRect nr = nextLevelButtonRect(winSize, 400.0f);
                            if (nr.contains(mpF)) {
                                // Переход на уровень 2
                                ui.campaignLevel = 2;
                                ui.targetTempCelsius = 80.0f;
                                ui.tempSliderDragging = false;
                                ui.tempEditing = false;
                                ui.selectedSpawnType = -1;
                                ui.focusedAtomIndex = -1;

                                atoms.clear();
                                walls.clear();
                                neutrons.clear();
                                gammas.clear();
                                delayedPool.clear();

                                ui.pillars.clear();
                                ui.barriers.clear();
                                ui.activeSpawnZone.active = false;
                                ui.gravityEnabled = false;
                                ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                                ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                                ui.boxSizeX = BOX_DEFAULT;
                                ui.boxSizeY = BOX_DEFAULT;

                                ui.batchCounts.assign(ATOM_TYPES.size(), 0);
                                ui.batchCounts[0] = 10;
                                ui.batchCounts[1] = 5;
                                physStepAccumulator = 0.0f;

                                ui.taskPhase = 1;
                                ui.taskH2Count = 0;
                                ui.taskH2MaxSeen = 0;
                                ui.taskH2Target = 5;
                                ui.taskH2Complete = false;
                                ui.taskH2OCount = 0;
                                ui.taskH2O2Count = 0;
                                ui.taskHBondCount = 0;
                                ui.hintButtonOpen = false;
                                ui.taskTextAlpha = 1.0f;
                                ui.nextLevelButtonHovered = false;
                                ui.copyPendingSpawn = false;

                                handled = true;
                            }
                        }
                        else if (ui.campaignMode && ui.campaignLevel == 2
                            && ui.taskPhase == 5)
                        {
                            sf::FloatRect nr = nextLevelButtonRect(winSize, 480.0f);
                            if (nr.contains(mpF)) {
                                // Переход на уровень 3
                                ui.campaignLevel = 3;
                                ui.targetTempCelsius = -15.0f;
                                ui.tempSliderDragging = false;
                                ui.tempEditing = false;
                                ui.selectedSpawnType = -1;
                                ui.focusedAtomIndex = -1;

                                atoms.clear();
                                walls.clear();
                                neutrons.clear();
                                gammas.clear();
                                delayedPool.clear();

                                ui.pillars.clear();
                                ui.barriers.clear();
                                ui.activeSpawnZone.active = false;
                                ui.gravityEnabled = false;
                                ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                                ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                                ui.boxSizeX = BOX_DEFAULT;
                                ui.boxSizeY = BOX_DEFAULT;

                                ui.batchCounts.assign(ATOM_TYPES.size(), 0);
                                ui.batchCounts[0] = 10;
                                ui.batchCounts[1] = 5;
                                ui.batchCounts[2] = 5;
                                physStepAccumulator = 0.0f;

                                ui.taskPhase = 0;
                                ui.taskH2Count = 0;
                                ui.taskH2MaxSeen = 0;
                                ui.taskH2Target = 5;
                                ui.taskH2Complete = false;
                                ui.taskH2OCount = 0;
                                ui.taskH2O2Count = 0;
                                ui.taskHBondCount = 0;
                                ui.taskHClCount = 0;
                                ui.taskH3OCount = 0;
                                ui.taskSingleClCount = 0;
                                ui.taskSingleClBest = 0;
                                ui.taskClHoldTimer = 0.0f;
                                ui.taskClTimerRunning = false;
                                ui.taskClFlashTimer = 0.0f;
                                ui.hintButtonOpen = false;
                                ui.taskTextAlpha = 1.0f;
                                ui.nextLevelButtonHovered = false;
                                ui.copyPendingSpawn = false;

                                handled = true;
                            }
                        }
                        else if (ui.campaignMode && ui.campaignLevel == 4
                            && ui.taskPhase == 2)
                        {
                            sf::FloatRect nr = nextLevelButtonRect(winSize,
                                L4_TASK_PANEL_H);
                            if (nr.contains(mpF)) {
                                // L4 → L6
                                ui.campaignLevel = 6;
                                ui.spawnMenuOpen = false;    // ← НОВОЕ
                                ui.boxSizeX = L6_BOX_W;
                                ui.boxSizeY = L6_BOX_H;

                                ui.targetTempCelsius = L6_TEMP_C;
                                ui.tempSliderDragging = false;
                                ui.tempEditing = false;

                                ui.gravityEnabled = true;
                                ui.gravityMagnitude = L6_GRAVITY_MAG;
                                ui.gravityDirDeg = L6_GRAVITY_DIR_DEG;
                                ui.speedSlider.value = 1.0f;
                                ui.savedSpeedValue = 1.0f;

                                ui.selectedSpawnType = -1;
                                ui.spawnWaterSelected = false;
                                ui.waterSpawnRotation = 0.0f;
                                ui.focusedAtomIndex = -1;

                                ui.stockLimited = false;
                                ui.stockH = 0;
                                ui.stockO = 0;
                                ui.spawnCooldownTimer = 0.0f;

                                atoms.clear();
                                walls.clear();
                                neutrons.clear();
                                gammas.clear();
                                delayedPool.clear();

                                ui.pillars.clear();
                                ui.barriers.clear();
                                ui.activeSpawnZone.active = false;
                                ui.gravityEnabled = false;
                                ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                                ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                                ui.boxSizeX = BOX_DEFAULT;
                                ui.boxSizeY = BOX_DEFAULT;

                                ui.pillars.push_back({ { L6_START_X, L6_START_Y },
                                    L6_PILLAR_HALF_SIZE });
                                for (int i = 0; i < L6_RING_COUNT; ++i) {
                                    sf::Vector2f c{ L6_RING_X[i], L6_RING_Y[i] };
                                    ui.pillars.push_back({ c, L6_PILLAR_HALF_SIZE });
                                    sf::Vector2f bL = c + sf::Vector2f(
                                        -(L6_PILLAR_HALF_SIZE + L6_BARRIER_HALF_W),
                                        -L6_URANIUM_Y_OFFSET + 0.5f);
                                    sf::Vector2f bR = c + sf::Vector2f(
                                        +(L6_PILLAR_HALF_SIZE + L6_BARRIER_HALF_W),
                                        -L6_URANIUM_Y_OFFSET + 0.5f);
                                    ui.barriers.push_back({ bL,
                                        { L6_BARRIER_HALF_W, L6_BARRIER_HALF_H } });
                                    ui.barriers.push_back({ bR,
                                        { L6_BARRIER_HALF_W, L6_BARRIER_HALF_H } });
                                }

                                ui.activeSpawnZone.pos = { L6_START_X,
                                    L6_START_Y - 3.0f };
                                ui.activeSpawnZone.radius = L6_SPAWN_ZONE_RADIUS;
                                ui.activeSpawnZone.active = true;

                                ui.batchCounts.assign(ATOM_TYPES.size() + 1, 0);

                                ui.ringIndex = 0;
                                ui.ringUraniumTotal = L6_URANIUM_PER_RING;
                                ui.ringUraniumRemaining = 0;
                                {
                                    sf::Vector2f c{ L6_RING_X[0], L6_RING_Y[0] };
                                    float baseY = c.y - L6_URANIUM_Y_OFFSET;
                                    float totalW = (L6_URANIUM_PER_RING - 1)
                                        * L6_URANIUM_SPACING;
                                    float x0 = c.x - totalW * 0.5f;
                                    for (int k = 0; k < L6_URANIUM_PER_RING; ++k) {
                                        sf::Vector2f p(x0 + k * L6_URANIUM_SPACING,
                                            baseY);
                                        atoms.push_back(makeAtom(3, p, { 0.0f, 0.0f }));
                                        ui.ringUraniumRemaining++;
                                    }
                                }

                                ui.taskPhase = 1;
                                ui.taskTextAlpha = 1.0f;
                                ui.cameraTransitionActive = false;
                                ui.phaseDelayTimer = 0.0f;
                                ui.nextLevelButtonHovered = false;
                                ui.copyPendingSpawn = false;
                                ui.hintButtonOpen = false;
                                handled = true;
                            }
                        }
                        else if (ui.campaignMode && ui.campaignLevel == 6
                            && ui.taskPhase == L6_RING_COUNT + 1)
                        {
                            sf::FloatRect nr = nextLevelButtonRect(winSize,
                                L6_TASK_PANEL_H);
                            if (nr.contains(mpF)) {
                                state = GameState::MainMenu;
                                ui.pauseMenuOpen = false;
                                ui.campaignMode = false;
                                ui.campaignLevel = 0;
                                ui.pillars.clear();
                                ui.barriers.clear();
                                ui.activeSpawnZone.active = false;
                                mainMenu.init(rng);
                                handled = true;
                            }
                        }
                        else if (ui.campaignMode && ui.campaignLevel == 3
                            && ui.taskPhase == 3)
                        {
                            sf::FloatRect nr = nextLevelButtonRect(winSize, 460.0f);
                            if (nr.contains(mpF)) {
                                // L3 → L4
                                ui.campaignLevel = 4;
                                ui.boxSizeX = L4_BOX_W;
                                ui.boxSizeY = L4_BOX_H;

                                ui.targetTempCelsius = L4_TEMP_C;
                                ui.tempSliderDragging = false;
                                ui.tempEditing = false;

                                ui.gravityEnabled = true;
                                ui.gravityMagnitude = L4_GRAVITY_MAG;
                                ui.gravityDirDeg = L4_GRAVITY_DIR_DEG;
                                ui.speedSlider.value = 1.0f;
                                ui.savedSpeedValue = 1.0f;

                                ui.selectedSpawnType = -1;
                                ui.spawnWaterSelected = false;
                                ui.waterSpawnRotation = 0.0f;
                                ui.focusedAtomIndex = -1;

                                atoms.clear();
                                walls.clear();
                                neutrons.clear();
                                gammas.clear();
                                delayedPool.clear();
                                ui.pillars.clear();
                                ui.barriers.clear();
                                ui.activeSpawnZone.active = false;
                                ui.gravityEnabled = false;
                                ui.gravityMagnitude = GRAVITY_DEFAULT_MAG;
                                ui.gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
                                ui.boxSizeX = BOX_DEFAULT;
                                ui.boxSizeY = BOX_DEFAULT;

                                ui.pillars.push_back({ { -L4_PILLAR_X, 0.0f },
                                    L4_PILLAR_HALF_SIZE });
                                ui.pillars.push_back({ {  0.0f,        0.0f },
                                    L4_PILLAR_HALF_SIZE });
                                ui.pillars.push_back({ {  L4_PILLAR_X, 0.0f },
                                    L4_PILLAR_HALF_SIZE });

                                ui.batchCounts.assign(3, 0);
                                ui.batchCounts[0] = 5;
                                ui.batchCounts[1] = 5;
                                ui.batchCounts[2] = 5;

                                ui.stockLimited = true;
                                ui.stockH = L4_START_H;
                                ui.stockO = L4_START_O;
                                ui.spawnCooldownTimer = 0.0f;

                                ui.taskPhase = 1;
                                ui.taskTextAlpha = 1.0f;
                                ui.taskPillarsConnected = 0;
                                ui.taskBridgeHoldTimer = 0.0f;
                                ui.taskBridgeTimerRunning = false;
                                ui.nextLevelButtonHovered = false;
                                ui.copyPendingSpawn = false;
                                ui.hintButtonOpen = false;
                                handled = true;
                            }
                        }

                    }

                    if (!handled) {
                        // Кнопка «Show hint» — доступна в тех же фазах,
                        // что и в UI.cpp:
                        //   L2: фаза 2 (пероксид), фаза 4 (лёд);
                        //   L3: фазы 1, 2, 3.
                        // Y-координату кнопки не считаем «на месте» —
                        // берём ui.hintButtonRect, который заполнил
                        // drawEverything на предыдущем кадре (учитывает
                        // высоту цели: одна строка или две).
                        const bool showHintAvailable =
                            (ui.campaignMode && ui.campaignLevel == 2
                                && (ui.taskPhase == 2 || ui.taskPhase == 4))
                            || (ui.campaignMode && ui.campaignLevel == 3
                                && (ui.taskPhase >= 1 && ui.taskPhase <= 3));

                        if (showHintAvailable) {
                            if (ui.hintButtonRect.contains(mpF)) {
                                ui.hintButtonOpen = !ui.hintButtonOpen;
                                handled = true;
                            }
                        }
                    }

                    if (!handled) {
                        if (!handled && ui.wallDrawMode) {
                            sf::Vector2f worldPos = window.mapPixelToCoords(mp, camera);
                            ui.isDrawingWall = true;
                            ui.wallDrawStart = worldPos;
                            ui.wallDrawCurrent = worldPos;
                            handled = true;
                        }
                        if ((float)mp.y < PANEL_HEIGHT) {
                            const bool l4locked = ui.campaignMode && ui.campaignLevel == 4;
                            if (!l4locked) {
                                // Speed slider
                                float knobX = SLIDER_LEFT + valueToPos(ui.speedSlider.value) * SLIDER_WIDTH;
                                float distX = std::abs(mpF.x - knobX);
                                float distY = std::abs(mpF.y - trackY);
                                bool overKnob = (distX < SLIDER_KNOB_R + 4.0f) && (distY < SLIDER_KNOB_R + 4.0f);
                                bool overTrack = (mpF.x >= SLIDER_LEFT - SLIDER_KNOB_R) &&
                                    (mpF.x <= trackRight + SLIDER_KNOB_R) &&
                                    (distY < SLIDER_KNOB_R + 4.0f);
                                if (overKnob || overTrack) {
                                    ui.speedSlider.isDragging = true;
                                    ui.speedSlider.value = posToValue((mpF.x - SLIDER_LEFT) / SLIDER_WIDTH);
                                    ui.savedSpeedValue = ui.speedSlider.value;
                                }
                                float rbX = SLIDER_LEFT + SLIDER_WIDTH + RESET_BTN_GAP;
                                float rbY = trackY - RESET_BTN_H / 2.0f;
                                sf::FloatRect rbRect({ rbX, rbY }, { RESET_BTN_W, RESET_BTN_H });
                                if (rbRect.contains(mpF)) {
                                    ui.speedSlider.value = 1.0f;
                                    ui.savedSpeedValue = 1.0f;
                                    ui.speedSlider.isDragging = false;
                                }

                                // Temp slider: запрещён в L1 всегда, в L2 — до phase 3
                                const bool tempDisabledNow =
                                    ui.campaignMode &&
                                    (ui.campaignLevel == 1
                                        || (ui.campaignLevel == 2 && ui.taskPhase < 3));

                                // Temp slider и всё ниже — только вне кампании
                                // либо внутри L2 с разблокированной температурой
                                if (!tempDisabledNow) {
                                    // Temp slider
                                    float tSliderRight = TEMP_SLIDER_LEFT + TEMP_SLIDER_WIDTH;
                                    float tKnobX = TEMP_SLIDER_LEFT + tempCelsiusToSliderPos(ui.targetTempCelsius) * TEMP_SLIDER_WIDTH;
                                    float tDistX = std::abs(mpF.x - tKnobX);
                                    bool tOverKnob = (tDistX < SLIDER_KNOB_R + 4.0f) && (distY < SLIDER_KNOB_R + 4.0f);
                                    bool tOverTrack = (mpF.x >= TEMP_SLIDER_LEFT - SLIDER_KNOB_R) &&
                                        (mpF.x <= tSliderRight + SLIDER_KNOB_R) &&
                                        (distY < SLIDER_KNOB_R + 4.0f);
                                    if (tOverKnob || tOverTrack) {
                                        ui.tempSliderDragging = true;
                                        ui.targetTempCelsius = tempSliderPosToCelsius((mpF.x - TEMP_SLIDER_LEFT) / TEMP_SLIDER_WIDTH);
                                    }

                                    sf::FloatRect tFieldRect(
                                        { TEMP_FIELD_LEFT, trackY - TEMP_FIELD_H / 2.0f },
                                        { TEMP_FIELD_W, TEMP_FIELD_H });
                                    if (tFieldRect.contains(mpF) && !ui.tempEditing) {
                                        commitEdit();
                                        ui.tempEditing = true;
                                        ui.tempEditBuffer = formatTempCelsius(ui.targetTempCelsius);
                                        ui.cursorClock.restart();
                                    }

                                    sf::FloatRect tRbRect(
                                        { TEMP_RESET_LEFT, trackY - TEMP_RESET_H / 2.0f },
                                        { TEMP_RESET_W, TEMP_RESET_H });
                                    if (tRbRect.contains(mpF)) {
                                        commitTempEdit();
                                        ui.targetTempCelsius = TEMP_DEFAULT_C;
                                        ui.tempSliderDragging = false;
                                    }

                                    // Кнопка "Charges"
                                    sf::FloatRect chargeBtnRect(
                                        { CHARGE_BTN_LEFT, trackY - CHARGE_BTN_H / 2.0f },
                                        { CHARGE_BTN_W, CHARGE_BTN_H });
                                    if (chargeBtnRect.contains(mpF)) {
                                        ui.showCharges = !ui.showCharges;
                                    }
                                }
                            }
                        }
                        else if (!ui.campaignMode && boxPanelRect.contains(mpF)) {
                            float distY1 = std::abs(mpF.y - bRow1Y);
                            float distY2 = std::abs(mpF.y - bRow2Y);
                            float knobR = BOX_KNOB_R * S;

                            float k1x = bTrackX1 + boxSizeToPos(ui.boxSizeX) * (bTrackX2 - bTrackX1);
                            float k2x = bTrackX1 + boxSizeToPos(ui.boxSizeY) * (bTrackX2 - bTrackX1);

                            bool overX = (std::abs(mpF.x - k1x) < knobR + 4.0f && distY1 < knobR + 4.0f) ||
                                (mpF.x >= bTrackX1 - knobR && mpF.x <= bTrackX2 + knobR && distY1 < knobR + 4.0f);
                            bool overY = (std::abs(mpF.x - k2x) < knobR + 4.0f && distY2 < knobR + 4.0f) ||
                                (mpF.x >= bTrackX1 - knobR && mpF.x <= bTrackX2 + knobR && distY2 < knobR + 4.0f);

                            if (overX) {
                                ui.boxSizeXDragging = true;
                                ui.boxSizeX = boxSizePosToSize((mpF.x - bTrackX1) / (bTrackX2 - bTrackX1));
                                handled = true;
                            }
                            else if (overY) {
                                ui.boxSizeYDragging = true;
                                ui.boxSizeY = boxSizePosToSize((mpF.x - bTrackX1) / (bTrackX2 - bTrackX1));
                                handled = true;
                            }
                        }
                        else if (!ui.campaignMode && gravPanelRect.contains(mpF)) {
                            // Чек-бокс
                            sf::FloatRect checkRect(
                                { gpCheckX, gpCheckY },
                                { GRAV_CHECKBOX_SIZE * S, GRAV_CHECKBOX_SIZE * S });
                            if (checkRect.contains(mpF)) {
                                ui.gravityEnabled = !ui.gravityEnabled;
                                handled = true;
                            }
                            // Angle slider
                            else if (std::abs(mpF.y - gpRow2Y) < 12.0f * S) {
                                ui.gravityDirDragging = true;
                                float t = std::clamp((mpF.x - gpSliderX1) / (gpSliderX2 - gpSliderX1), 0.0f, 1.0f);
                                ui.gravityDirDeg = t * 360.0f;
                                handled = true;
                            }
                            // Magnitude slider
                            else if (std::abs(mpF.y - gpRow3Y) < 12.0f * S) {
                                ui.gravityMagDragging = true;
                                float t = std::clamp((mpF.x - gpSliderX1) / (gpSliderX2 - gpSliderX1), 0.0f, 1.0f);
                                ui.gravityMagnitude = t * GRAVITY_MAG_MAX;
                                handled = true;
                            }
                            else {
                                handled = true;   // не пропускаем клики за панель
                            }
                        }
                        else if (ui.spawnMenuOpen && menuRect.contains(mpF)) {
                            float localY = mpF.y - mTop - mHeaderH;
                            if (localY >= 0.0f && localY < spawnRowCount * mRowH) {
                                int rowIndex = (int)(localY / mRowH);
                                float rowTop = mTop + mHeaderH + rowIndex * mRowH;
                                float localX = mpF.x - mLeft;

                                sf::FloatRect minusRect(
                                    { mLeft + rMinusX, rowTop + (mRowH - rBtnH) / 2.0f },
                                    { rBtnW, rBtnH });
                                sf::FloatRect fieldRect(
                                    { mLeft + rFieldX, rowTop + (mRowH - rBtnH) / 2.0f },
                                    { rFieldW, rBtnH });
                                sf::FloatRect plusRect(
                                    { mLeft + rPlusX, rowTop + (mRowH - rBtnH) / 2.0f },
                                    { rBtnW, rBtnH });

                                if (minusRect.contains(mpF)) {
                                    commitEdit();
                                    ui.batchCounts[rowIndex] = std::max(0, ui.batchCounts[rowIndex] - 1);
                                }
                                else if (plusRect.contains(mpF)) {
                                    commitEdit();
                                    ui.batchCounts[rowIndex] = std::min(500, ui.batchCounts[rowIndex] + 1);
                                }
                                else if (fieldRect.contains(mpF)) {
                                    if (ui.editingCountIndex != rowIndex) {
                                        commitTempEdit();
                                        ui.editingCountIndex = rowIndex;
                                        ui.editBuffer = std::to_string(ui.batchCounts[rowIndex]);
                                        ui.cursorClock.restart();
                                    }
                                }
                                else if (localX < rSelectW) {
                                    commitEdit();
                                    const bool waterRow = isWaterRow(
                                        rowIndex, ui.campaignMode, ui.campaignLevel);
                                    if (waterRow) {
                                        ui.spawnWaterSelected = true;
                                        ui.selectedSpawnType = -1;
                                    }
                                    else {
                                        ui.spawnWaterSelected = false;
                                        ui.selectedSpawnType =
                                            (ui.selectedSpawnType == rowIndex) ? -1 : rowIndex;
                                    }
                                }
                            }
                            else if (localY >= spawnRowCount * mRowH) {
                                float footerTop = mTop + mHeaderH + spawnRowCount * mRowH;

                                sf::FloatRect batchBtnRect(
                                    { mLeft + 10.0f * S, footerTop + 26.0f * S },
                                    { mWidth - 20.0f * S, 26.0f * S });
                                sf::FloatRect clearBtnRect(
                                    { mLeft + 10.0f * S, footerTop + 56.0f * S },
                                    { mWidth - 20.0f * S, 26.0f * S });

                                // В кампании (уровень 1) кнопка Spawn Batch
                                // заблокирована, пока не набрано 5 H2 (фаза 1).
                                const bool batchDisabled =
                                    ui.campaignMode && ui.campaignLevel == 1
                                    && ui.taskPhase == 1;

                                if (batchBtnRect.contains(mpF)) {
                                    if (batchDisabled) {
                                        handled = true;   // клик съедаем
                                    }
                                    else {
                                        float halfX = ui.boxSizeX * 0.5f;
                                        float halfY = ui.boxSizeY * 0.5f;
                                        // В кампании batch ограничен доступными
                                        // элементами. В песочнице — все атомы
                                        // плюс строка Water.
                                        size_t maxTypes = ATOM_TYPES.size() + 1;
                                        if (ui.campaignMode && ui.campaignLevel == 1)
                                            maxTypes = 1;
                                        else if (ui.campaignMode && ui.campaignLevel == 2)
                                            maxTypes = 2;
                                        else if (ui.campaignMode && ui.campaignLevel == 3)
                                            maxTypes = 3;
                                        else if (ui.campaignMode && ui.campaignLevel == 4)
                                            maxTypes = 3;   // H, O, Water
                                        else if (ui.campaignMode && ui.campaignLevel == 6)
                                            maxTypes = 0;   // ← НОВОЕ: L6 без меню
                                        for (size_t ti = 0; ti < maxTypes; ++ti) {
                                            int count = ui.batchCounts[ti];
                                            if (count <= 0) continue;

                                            // ============================================
                                            // Батч-спавн воды (H2O)
                                            // ============================================
                                            // Доступен:
                                            //   • в песочнице (строка Water)
                                            //   • на L4 кампании (строка Water)
                                            // Учитывает:
                                            //   • запас stockH / stockO на L4
                                            //   • препятствия-колонны ui.pillars
                                            //   • уже существующие атомы
                                            if (isWaterRow((int)ti,
                                                ui.campaignMode,
                                                ui.campaignLevel))
                                            {
                                                // На L4 ограничиваем запасом
                                                if (ui.campaignMode
                                                    && ui.campaignLevel == 4)
                                                {
                                                    int canH = ui.stockH / 2;
                                                    int canO = ui.stockO;
                                                    count = std::min(count,
                                                        std::min(canH, canO));
                                                }
                                                if (count <= 0) continue;

                                                const float halfAng =
                                                    WATER_ANGLE_DEG * 0.5f
                                                    * 3.14159265f / 180.0f;
                                                const float bLen =
                                                    getMorsePair(0, 1).re;
                                                const float oR = ATOM_TYPES[1].radius;
                                                const float hR = ATOM_TYPES[0].radius;
                                                const float maxR =
                                                    bLen + std::max(oR, hR) + 0.05f;

                                                std::uniform_real_distribution<float>
                                                    distX(-halfX, halfX);
                                                std::uniform_real_distribution<float>
                                                    distY(-halfY, halfY);
                                                std::uniform_real_distribution<float>
                                                    distRot(0.0f, 6.2831853f);

                                                // Проверка пересечения атома
                                                // с AABB-колонной.
                                                auto overlapsPillar =
                                                    [](sf::Vector2f pos, float r,
                                                        const Pillar& p)
                                                    {
                                                        float cX = std::clamp(
                                                            pos.x,
                                                            p.pos.x - p.halfSize,
                                                            p.pos.x + p.halfSize);
                                                        float cY = std::clamp(
                                                            pos.y,
                                                            p.pos.y - p.halfSize,
                                                            p.pos.y + p.halfSize);
                                                        float dx = pos.x - cX;
                                                        float dy = pos.y - cY;
                                                        return dx * dx + dy * dy
                                                            < r * r;
                                                    };

                                                for (int i = 0; i < count; ++i) {
                                                    bool placed = false;
                                                    for (int attempt = 0;
                                                        attempt < 300 && !placed;
                                                        ++attempt)
                                                    {
                                                        float cx = distX(rng);
                                                        float cy = distY(rng);
                                                        cx = std::clamp(cx,
                                                            -halfX + maxR,
                                                            halfX - maxR);
                                                        cy = std::clamp(cy,
                                                            -halfY + maxR,
                                                            halfY - maxR);

                                                        float rot = distRot(rng);
                                                        sf::Vector2f h1Local =
                                                            rotateVec(
                                                                { std::cos(halfAng),
                                                                  std::sin(halfAng) },
                                                                rot) * bLen;
                                                        sf::Vector2f h2Local =
                                                            rotateVec(
                                                                { std::cos(halfAng),
                                                                 -std::sin(halfAng) },
                                                                rot) * bLen;
                                                        sf::Vector2f h1 = {
                                                            cx + h1Local.x,
                                                            cy + h1Local.y
                                                        };
                                                        sf::Vector2f h2 = {
                                                            cx + h2Local.x,
                                                            cy + h2Local.y
                                                        };

                                                        bool collides = false;

                                                        // Препятствия (L4).
                                                        // В песочнице ui.pillars пуст,
                                                        // поэтому цикл ничего не делает.
                                                        for (const auto& p
                                                            : ui.pillars) {
                                                            if (overlapsPillar(
                                                                { cx, cy },
                                                                oR + 0.05f, p)
                                                                || overlapsPillar(
                                                                    h1,
                                                                    hR + 0.05f, p)
                                                                || overlapsPillar(
                                                                    h2,
                                                                    hR + 0.05f, p))
                                                            {
                                                                collides = true;
                                                                break;
                                                            }
                                                        }
                                                        if (collides) continue;

                                                        // Существующие атомы
                                                        for (const auto& a
                                                            : atoms) {
                                                            float ddx =
                                                                a.pos.x - cx;
                                                            float ddy =
                                                                a.pos.y - cy;
                                                            float minD =
                                                                a.radius + oR + 0.05f;
                                                            if (ddx * ddx + ddy * ddy
                                                                < minD * minD)
                                                            {
                                                                collides = true;
                                                                break;
                                                            }

                                                            ddx = a.pos.x - h1.x;
                                                            ddy = a.pos.y - h1.y;
                                                            minD =
                                                                a.radius + hR + 0.05f;
                                                            if (ddx * ddx + ddy * ddy
                                                                < minD * minD)
                                                            {
                                                                collides = true;
                                                                break;
                                                            }

                                                            ddx = a.pos.x - h2.x;
                                                            ddy = a.pos.y - h2.y;
                                                            minD =
                                                                a.radius + hR + 0.05f;
                                                            if (ddx * ddx + ddy * ddy
                                                                < minD * minD)
                                                            {
                                                                collides = true;
                                                                break;
                                                            }
                                                        }
                                                        if (collides) continue;

                                                        // Спавним молекулу
                                                        int base =
                                                            (int)atoms.size();
                                                        auto mol = makeWaterMolecule(
                                                            base, { cx, cy }, rot,
                                                            { 0.0f, 0.0f });
                                                        atoms.push_back(mol[0]);
                                                        atoms.push_back(mol[1]);
                                                        atoms.push_back(mol[2]);

                                                        if (ui.campaignMode
                                                            && ui.campaignLevel == 4)
                                                        {
                                                            ui.stockH -= 2;
                                                            ui.stockO -= 1;
                                                        }
                                                        placed = true;
                                                    }
                                                    if (!placed) break;
                                                }
                                                continue;
                                            }

                                            // Спецслучай: batch-спавн нейтронов
                                            if ((int)ti == (int)ATOM_TYPES.size() - 1) {
                                                for (int i = 0; i < count; ++i) {
                                                    Neutron n;
                                                    n.pos = { 0.0f, 0.0f };
                                                    n.dir = randomUnitVector2D(rng);
                                                    n.energy_eV = 1e6f;
                                                    n.age = 0.0f;
                                                    n.alive = true;
                                                    n.parentU = -1;
                                                    n.delayed = false;
                                                    neutrons.push_back(n);
                                                }
                                                continue;
                                            }

                                            const AtomType& t = ATOM_TYPES[ti];
                                            std::uniform_real_distribution<float> distX(-halfX, halfX);
                                            std::uniform_real_distribution<float> distY(-halfY, halfY);
                                            for (int i = 0; i < count; ++i) {
                                                for (int attempt = 0; attempt < 200; ++attempt) {
                                                    float x = std::clamp(distX(rng), -halfX + t.radius, halfX - t.radius);
                                                    float y = std::clamp(distY(rng), -halfY + t.radius, halfY - t.radius);
                                                    bool free = true;
                                                    for (const auto& a : atoms) {
                                                        float ddx = a.pos.x - x;
                                                        float ddy = a.pos.y - y;
                                                        float minDist = a.radius + t.radius + 0.05f;
                                                        if (ddx * ddx + ddy * ddy < minDist * minDist) {
                                                            free = false;
                                                            break;
                                                        }
                                                    }
                                                    if (free) {
                                                        atoms.push_back(makeAtom((int)ti, { x, y }, { 0.0f, 0.0f }));
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }   // конец !batchDisabled
                                }
                                else if (clearBtnRect.contains(mpF)) {
                                    atoms.clear();
                                    ui.focusedAtomIndex = -1;
                                    physStepAccumulator = 0.0f;
                                    if (ui.campaignMode && ui.campaignLevel == 4) {
                                        // Полный перезапуск L4: восстановить запас
                                        walls.clear();
                                        neutrons.clear();
                                        gammas.clear();
                                        delayedPool.clear();
                                        ui.stockH = L4_START_H;
                                        ui.stockO = L4_START_O;
                                        ui.spawnCooldownTimer = 0.0f;
                                        ui.batchCounts.assign(3, 0);
                                        ui.batchCounts[0] = 5;
                                        ui.batchCounts[1] = 5;
                                        ui.batchCounts[2] = 5;
                                        ui.taskPhase = 1;
                                        ui.taskPillarsConnected = 0;
                                        ui.taskBridgeHoldTimer = 0.0f;
                                        ui.taskBridgeTimerRunning = false;
                                        ui.selectedSpawnType = -1;
                                        ui.spawnWaterSelected = false;
                                        ui.waterSpawnRotation = 0.0f;
                                    }
                                }
                            }
                        }
                        else if ((ui.campaignMode && ui.campaignLevel == 6)
                            && ui.activeSpawnZone.active) {
                                // L6: спавн разрешён только в активной зоне.
                                sf::Vector2f worldPos =
                                    window.mapPixelToCoords(mp, camera);
                                float ddx = worldPos.x - ui.activeSpawnZone.pos.x;
                                float ddy = worldPos.y - ui.activeSpawnZone.pos.y;
                                float r2 = ddx * ddx + ddy * ddy;
                                float R2 = ui.activeSpawnZone.radius
                                    * ui.activeSpawnZone.radius;
                                if (r2 <= R2) {
                                    ui.spawnDragActive = true;
                                    ui.spawnDragOrigin = worldPos;
                                    ui.spawnDragAtomIndices.clear();
                                    ui.spawnDragNeutronIndices.clear();
                                    ui.selectedSpawnType =
                                        (int)ATOM_TYPES.size() - 1;   // нейтрон
                                }
                        }
                        else if (ui.selectedSpawnType >= 0 || ui.spawnWaterSelected) {
                            sf::Vector2f worldPos = window.mapPixelToCoords(mp, camera);

                            // L4: блокируем спавн при активном cooldown
                            const bool l4Blocked = (ui.campaignMode
                                && ui.campaignLevel == 4
                                && ui.spawnCooldownTimer > 0.0f);

                            if (!l4Blocked) {
                                if (ui.spawnWaterSelected) {
                                    // Превью H2O: просто запоминаем точку
                                    ui.spawnDragActive = true;
                                    ui.spawnDragOrigin = worldPos;
                                    ui.spawnDragAtomIndices.clear();
                                    ui.spawnDragNeutronIndices.clear();
                                }
                                else if (ui.selectedSpawnType == (int)ATOM_TYPES.size() - 1) {
                                    // Нейтрон
                                    ui.spawnDragActive = true;
                                    ui.spawnDragOrigin = worldPos;
                                    ui.spawnDragAtomIndices.clear();
                                    ui.spawnDragNeutronIndices.clear();
                                }
                                else {
                                    const AtomType& t = ATOM_TYPES[ui.selectedSpawnType];
                                    float halfX = ui.boxSizeX / 2.0f;
                                    float halfY = ui.boxSizeY / 2.0f;
                                    worldPos.x = std::clamp(worldPos.x,
                                        -halfX + t.radius, halfX - t.radius);
                                    worldPos.y = std::clamp(worldPos.y,
                                        -halfY + t.radius, halfY - t.radius);

                                    ui.spawnDragActive = true;
                                    ui.spawnDragOrigin = worldPos;
                                    ui.spawnDragAtomIndices.clear();
                                    ui.spawnDragNeutronIndices.clear();
                                }
                            }
                        }
                        else {
                            ui.lmbIsDown = true;
                            ui.lmbIsDragging = false;
                            ui.lmbDownPixel = mp;
                            ui.lmbCurrentPixel = mp;
                        }
                    }
                }

                if (mb->button == sf::Mouse::Button::Right) {
                    // ПКМ отменяет спавн из Spawn Menu — не нужно
                    // удерживать ЛКМ. Срабатывает, если:
                    //   • есть выбранный тип спавна (selectedSpawnType >= 0),
                    //   • или идёт pending-спавн по ЛКМ (drag без созданной
                    //     ещё сущности),
                    //   • или pending-копия через ПКМ-меню.
                    const bool pendingDragCancel = ui.spawnDragActive
                        && ui.spawnDragAtomIndices.empty()
                        && ui.spawnDragNeutronIndices.empty();

                    const bool spawnCancel = ui.selectedSpawnType >= 0
                        || ui.spawnWaterSelected
                        || pendingDragCancel
                        || ui.copyPendingSpawn;

                    if (spawnCancel) {
                        ui.spawnDragActive = false;
                        ui.spawnDragAtomIndices.clear();
                        ui.spawnDragNeutronIndices.clear();
                        ui.copyPendingSpawn = false;
                        ui.selectedSpawnType = -1;
                        ui.spawnWaterSelected = false;

                        commitEdit();
                        commitTempEdit();
                        ui.editingCountIndex = -1;
                        ui.tempEditing = false;
                        ui.rmbIsDown = false;
                        ui.rmbWasDragged = false;
                        ui.lmbIsDown = false;
                        ui.lmbIsDragging = false;
                        isPanning = false;
                    }
                    else {
                        sf::Vector2i mp = sf::Mouse::getPosition(window);
                        sf::Vector2f mpF((float)mp.x, (float)mp.y);
                        bool overTop = mp.y < (int)PANEL_HEIGHT;
                        bool overMenu = ui.spawnMenuOpen && menuRect.contains(mpF);

                        if (overMenu) {
                            // ПКМ по меню спавна — снять выделение типа атома.
                            commitEdit();
                            commitTempEdit();
                            ui.editingCountIndex = -1;
                            ui.tempEditing = false;
                            ui.selectedSpawnType = -1;
                            ui.spawnWaterSelected = false;
                            ui.rmbIsDown = false;
                            ui.rmbWasDragged = false;
                            isPanning = false;
                        }
                        else if (!overTop) {
                            ui.rmbIsDown = true;
                            ui.rmbWasDragged = false;
                            ui.rmbDownPixel = mp;
                            isPanning = true;
                            lastMousePos = mp;
                        }
                    }
                }
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    // Spawn + скорость одновременно при отпускании ЛКМ.
                    if (ui.spawnDragActive) {
                        sf::Vector2i mp = sf::Mouse::getPosition(window);
                        sf::Vector2f mouseWorld = window.mapPixelToCoords(mp, camera);
                        sf::Vector2f dragVec = mouseWorld - ui.spawnDragOrigin;
                        float dragLen = std::sqrt(dragVec.x * dragVec.x + dragVec.y * dragVec.y);

                        sf::Vector2i originPixel = window.mapCoordsToPixel(ui.spawnDragOrigin, camera);
                        float dpx = std::sqrt(
                            (float)(mp.x - originPixel.x) * (mp.x - originPixel.x) +
                            (float)(mp.y - originPixel.y) * (mp.y - originPixel.y));

                        sf::Vector2f dirUnit(0.0f, 0.0f);
                        float speed = 0.0f;
                        if (dpx > SPAWN_DRAG_MIN_PX && dragLen > 1e-6f) {
                            dirUnit = dragVec / dragLen;
                            speed = std::min(dragLen * SPAWN_SPEED_K, SPAWN_SPEED_MAX);
                        }
                        sf::Vector2f vel = dirUnit * speed;

                        const bool pendingSpawn =
                            ui.spawnDragAtomIndices.empty()
                            && ui.spawnDragNeutronIndices.empty();

                        // --- L4: проверяем запас и cooldown перед фактическим спавном ---
                        bool l4Ok = true;
                        bool isWaterSpawn = ui.spawnWaterSelected;
                        int needH = 0, needO = 0;
                        if (isWaterSpawn) { needH = 2; needO = 1; }
                        else if (ui.selectedSpawnType == 0) needH = 1;
                        else if (ui.selectedSpawnType == 1) needO = 1;

                        if (ui.campaignMode && ui.campaignLevel == 4
                            && pendingSpawn)
                        {
                            if (ui.spawnCooldownTimer > 0.0f) l4Ok = false;
                            if (ui.stockH < needH || ui.stockO < needO) l4Ok = false;
                        }

                        if (l4Ok && pendingSpawn && ui.copyPendingSpawn
                            && !(ui.campaignMode && ui.campaignLevel == 4))
                        {
                            // Копирование через ПКМ-меню (только не L4)
                            sf::Vector2f worldPos = ui.spawnDragOrigin;
                            float halfX = ui.boxSizeX / 2.0f;
                            float halfY = ui.boxSizeY / 2.0f;

                            for (size_t i = 0; i < ui.copyTemplates.size(); ++i) {
                                Atom na = ui.copyTemplates[i];
                                sf::Vector2f rotated = rotateVec(ui.copyOffsets[i], ui.copyRotation);
                                na.pos = worldPos + rotated;
                                na.pos.x = std::clamp(na.pos.x, -halfX + na.radius, halfX - na.radius);
                                na.pos.y = std::clamp(na.pos.y, -halfY + na.radius, halfY - na.radius);
                                na.vel = vel;
                                na.bonds = { -1, -1, -1, -1 };
                                na.hbonds = { -1, -1, -1, -1 };
                                na.selected = false;
                                na.age = 0.0f;
                                atoms.push_back(na);
                            }
                            for (size_t i = 0; i < ui.copyWallA.size(); ++i) {
                                sf::Vector2f ra = rotateVec(ui.copyWallA[i], ui.copyRotation);
                                sf::Vector2f rb = rotateVec(ui.copyWallB[i], ui.copyRotation);
                                Wall w;
                                w.a = worldPos + ra;
                                w.b = worldPos + rb;
                                w.a.x = std::clamp(w.a.x, -halfX, halfX);
                                w.a.y = std::clamp(w.a.y, -halfY, halfY);
                                w.b.x = std::clamp(w.b.x, -halfX, halfX);
                                w.b.y = std::clamp(w.b.y, -halfY, halfY);
                                walls.push_back(w);
                            }
                        }
                        else if (l4Ok && pendingSpawn && ui.spawnWaterSelected) {
                            // Спавн готовой молекулы H2O
                            float halfX = ui.boxSizeX / 2.0f;
                            float halfY = ui.boxSizeY / 2.0f;

                            const float halfAng = WATER_ANGLE_DEG * 0.5f
                                * 3.14159265f / 180.0f;
                            const float bLen = getMorsePair(0, 1).re;
                            float maxR = bLen + ATOM_TYPES[1].radius;

                            sf::Vector2f c = ui.spawnDragOrigin;
                            c.x = std::clamp(c.x, -halfX + maxR, halfX - maxR);
                            c.y = std::clamp(c.y, -halfY + maxR, halfY - maxR);

                            int base = (int)atoms.size();
                            auto mol = makeWaterMolecule(base, c,
                                ui.waterSpawnRotation, vel);
                            atoms.push_back(mol[0]);
                            atoms.push_back(mol[1]);
                            atoms.push_back(mol[2]);

                            if (ui.campaignMode && ui.campaignLevel == 4) {
                                ui.stockH -= 2;
                                ui.stockO -= 1;
                                ui.spawnCooldownTimer = L4_SPAWN_COOLDOWN;
                                // Обновим счётчик в Spawn Menu (визуально)
                                if (ui.batchCounts.size() >= 3)
                                    ui.batchCounts[2] = std::max(0, ui.batchCounts[2] - 1);
                            }
                        }
                        else if (l4Ok && pendingSpawn && ui.selectedSpawnType >= 0) {
                            // Обычный spawn из Spawn Menu.
                            if (ui.selectedSpawnType == (int)ATOM_TYPES.size() - 1) {
                                Neutron n;
                                n.pos = ui.spawnDragOrigin;
                                if (speed > 0.0f) {
                                    n.dir = dirUnit;
                                    float sp = std::min(speed, 20.0f);
                                    if (ui.campaignMode && ui.campaignLevel == 6)
                                        sp *= L6_SPAWN_SPEED_MUL;
                                    if (sp < 0.1f) sp = 0.1f;
                                    n.energy_eV = 0.0253f * (sp / 5.0f) * (sp / 5.0f);
                                }
                                else {
                                    n.dir = { 1.0f, 0.0f };
                                    n.energy_eV = 1e-4f;
                                }
                                n.age = 0.0f;
                                n.alive = true;
                                n.parentU = -1;
                                n.delayed = false;
                                neutrons.push_back(n);
                            }
                            else {
                                atoms.push_back(makeAtom(ui.selectedSpawnType,
                                    ui.spawnDragOrigin, vel));

                                if (ui.campaignMode && ui.campaignLevel == 4) {
                                    if (ui.selectedSpawnType == 0) ui.stockH -= 1;
                                    else if (ui.selectedSpawnType == 1) ui.stockO -= 1;
                                    ui.spawnCooldownTimer = L4_SPAWN_COOLDOWN;
                                    if ((int)ui.batchCounts.size() > ui.selectedSpawnType)
                                        ui.batchCounts[ui.selectedSpawnType] =
                                        std::max(0, ui.batchCounts[ui.selectedSpawnType] - 1);
                                }
                            }
                        }
                        else if (l4Ok) {
                            // Drag по уже созданным сущностям
                            for (int idx : ui.spawnDragAtomIndices) {
                                if (idx >= 0 && idx < (int)atoms.size())
                                    atoms[idx].vel = vel;
                            }
                            for (int idx : ui.spawnDragNeutronIndices) {
                                if (idx >= 0 && idx < (int)neutrons.size()) {
                                    if (speed > 0.0f) {
                                        neutrons[idx].dir = dirUnit;
                                        float sp = std::min(speed, 20.0f);
                                        if (sp < 0.1f) sp = 0.1f;
                                        neutrons[idx].energy_eV =
                                            0.0253f * (sp / 5.0f) * (sp / 5.0f);
                                    }
                                }
                            }
                        }

                        ui.spawnDragActive = false;
                        ui.spawnDragAtomIndices.clear();
                        ui.spawnDragNeutronIndices.clear();
                        ui.copyPendingSpawn = false;
                    }
                    if (ui.isDrawingWall) {
                        ui.isDrawingWall = false;
                        sf::Vector2f d = ui.wallDrawCurrent - ui.wallDrawStart;
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len >= WALL_MIN_LENGTH) {
                            Wall w;
                            w.a = ui.wallDrawStart;
                            w.b = ui.wallDrawCurrent;
                            walls.push_back(w);
                        }
                    }
                    if (ui.lmbIsDown) {
                        // На L6 клик по урану НЕ должен его фокусировать:
                        // камера мгновенно «улетает» на мишень и целиться
                        // по кольцам становится невозможно. Уран здесь —
                        // просто цель, манипулировать им не нужно.
                        const bool l6NoSelect =
                            ui.campaignMode && ui.campaignLevel == 6;

                        if (!ui.lmbIsDragging && !l6NoSelect) {
                            sf::Vector2f worldPos = window.mapPixelToCoords(ui.lmbDownPixel, camera);

                            // Сначала ищем атом
                            int hit = -1;
                            for (size_t i = 0; i < atoms.size(); ++i) {
                                float ddx = atoms[i].pos.x - worldPos.x;
                                float ddy = atoms[i].pos.y - worldPos.y;
                                float hitR = atoms[i].radius * 2.0f;
                                if (ddx * ddx + ddy * ddy < hitR * hitR) {
                                    hit = (int)i;
                                    break;
                                }
                            }

                            if (hit >= 0) {
                                ui.focusedAtomIndex = hit;
                                for (auto& w : walls) w.selected = false;
                            }
                            else {
                                ui.focusedAtomIndex = -1;
                                for (auto& a : atoms) a.selected = false;

                                // Ищем стену
                                bool wallHit = false;
                                for (auto& w : walls) {
                                    sf::Vector2f ab = w.b - w.a;
                                    float ab2 = ab.x * ab.x + ab.y * ab.y;
                                    if (ab2 < 1e-8f) continue;
                                    sf::Vector2f ap = worldPos - w.a;
                                    float t = (ap.x * ab.x + ap.y * ab.y) / ab2;
                                    t = std::clamp(t, 0.0f, 1.0f);
                                    sf::Vector2f closest = w.a + t * ab;
                                    float dx = worldPos.x - closest.x;
                                    float dy = worldPos.y - closest.y;
                                    if (dx * dx + dy * dy < WALL_HIT_RADIUS * WALL_HIT_RADIUS) {
                                        w.selected = !w.selected;
                                        wallHit = true;
                                    }
                                    else {
                                        w.selected = false;
                                    }
                                }
                                (void)wallHit;
                            }
                        }
                        ui.lmbIsDown = false;
                        ui.lmbIsDragging = false;
                    }
                    ui.speedSlider.isDragging = false;
                    ui.tempSliderDragging = false;
                    ui.boxSizeXDragging = false;
                    ui.boxSizeYDragging = false;
                    ui.gravityDirDragging = false;
                    ui.gravityMagDragging = false;
                }

                if (mb->button == sf::Mouse::Button::Right) {
                    isPanning = false;

                    if (ui.rmbIsDown && !ui.rmbWasDragged) {
                        if (ui.copyMode) {
                            ui.copyMode = false;
                            ui.copyOffsets.clear();
                            ui.copyTemplates.clear();
                            ui.copyRotation = 0.0f;
                        }
                        else if (ui.rmbMenuOpen) {
                            ui.rmbMenuOpen = false;
                        }
                        else {
                            int selCount = 0;
                            for (const auto& a : atoms) if (a.selected) selCount++;
                            for (const auto& w : walls) if (w.selected) selCount++;

                            // ПКМ-меню доступно в песочнице и в кампаниях-уровнях-2/3.
                            // В уровне-1 его нет — там идёт обучение, инструменты
                            // «на лету» только запутали бы.
                            const bool rmbMenuAllowed = !ui.campaignMode
                                || (ui.campaignMode && ui.campaignLevel >= 2);

                            if (selCount > 0 && rmbMenuAllowed) {
                                ui.rmbMenuOpen = true;
                                ui.rmbMenuPos = ui.rmbDownPixel;
                            }
                            else {
                                ui.focusedAtomIndex = -1;
                            }
                        }
                    }
                    ui.rmbIsDown = false;
                    ui.rmbWasDragged = false;
                }
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (ui.editingCountIndex >= 0) {
                    if (keyPressed->code == sf::Keyboard::Key::Enter) commitEdit();
                    else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        ui.editingCountIndex = -1;
                        ui.editBuffer.clear();
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Backspace) {
                        if (!ui.editBuffer.empty()) ui.editBuffer.pop_back();
                    }
                }
                else if (ui.tempEditing) {
                    if (keyPressed->code == sf::Keyboard::Key::Enter) commitTempEdit();
                    else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        ui.tempEditing = false;
                        ui.tempEditBuffer.clear();
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Backspace) {
                        if (!ui.tempEditBuffer.empty()) ui.tempEditBuffer.pop_back();
                    }
                }
                else {
                    if (keyPressed->code == sf::Keyboard::Key::R) {
                        walls.clear();
                        atoms.clear();
                        neutrons.clear();
                        gammas.clear();
                        atoms.push_back(makeHydrogen({ -2.0f, 0.0f }, { 0.5f, 0.0f }));
                        atoms.push_back(makeHydrogen({ 2.0f, 0.0f }, { -0.5f, 0.0f }));
                        physStepAccumulator = 0.0f;
                        ui.focusedAtomIndex = -1;
                        ui.rmbMenuOpen = false;
                        ui.copyMode = false;
                        ui.copyRotation = 0.0f;
                    }
                    if (keyPressed->code == sf::Keyboard::Key::N) {
                        Neutron n;
                        n.pos = camera.getCenter();
                        n.dir = randomUnitVector2D(rng);
                        n.energy_eV = 1e6f;
                        n.age = 0.0f;
                        n.alive = true;
                        n.parentU = -1;
                        n.delayed = false;
                        neutrons.push_back(n);
                    }
                    if (keyPressed->code == sf::Keyboard::Key::W) {
                        if (!ui.campaignMode) ui.wallDrawMode = !ui.wallDrawMode;
                    }
                    if (keyPressed->code == sf::Keyboard::Key::C) {
                        ui.showCharges = !ui.showCharges;
                    }
                    if (keyPressed->code == sf::Keyboard::Key::Space) {
                        // На L4 пауза пробелом отключена — иначе можно
                        // расставить молекулы «на паузе» и выиграть.
                        const bool spacePauseBlocked =
                            ui.campaignMode && ui.campaignLevel == 4;
                        if (!spacePauseBlocked) {
                            if (ui.speedSlider.value > 0.0f) {
                                ui.savedSpeedValue = ui.speedSlider.value;
                                ui.speedSlider.value = 0.0f;
                            }
                            else {
                                ui.speedSlider.value = (ui.savedSpeedValue > 0.0f) ? ui.savedSpeedValue : 1.0f;
                            }
                        }
                    }
                    if (keyPressed->code == sf::Keyboard::Key::Tab) {
                        // На L6 меню спавна отключено — спавн только drag'ом
                        // в активной зоне.
                        bool l6 = ui.campaignMode && ui.campaignLevel == 6;
                        if (!l6) ui.spawnMenuOpen = !ui.spawnMenuOpen;
                    }
                    if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        if (ui.isDrawingWall) {
                            ui.isDrawingWall = false;
                        }
                        else if (ui.wallDrawMode) ui.wallDrawMode = false;
                        else if (ui.copyMode) {
                            ui.copyMode = false;
                            ui.copyOffsets.clear();
                            ui.copyTemplates.clear();
                            ui.copyRotation = 0.0f;
                        }
                        else if (ui.rmbMenuOpen) ui.rmbMenuOpen = false;
                        else if (ui.selectedSpawnType >= 0) {
                            ui.selectedSpawnType = -1;
                            ui.spawnWaterSelected = false;
                        }
                        else {
                            // Ничего не открыто — открываем pause-меню.
                            // Снимаем выделение, чтобы картинка под
                            // оверлеем была «чистой».
                            for (auto& a : atoms) a.selected = false;
                            for (auto& w : walls) w.selected = false;
                            ui.pauseMenuOpen = true;

                            // Сброс «водного» превью — иначе можно на паузе
                            // (перед паузой) подготовить спавн H2O и получить
                            // преимущество. Также полностью очищаем pending-drag.
                            ui.spawnWaterSelected = false;
                            ui.waterSpawnRotation = 0.0f;
                            ui.selectedSpawnType = -1;
                            ui.spawnDragActive = false;
                            ui.spawnDragAtomIndices.clear();
                            ui.spawnDragNeutronIndices.clear();
                            ui.copyPendingSpawn = false;
                        }
                    }
                }
            }
        }

        // ============================================================
        // Непрерывные апдейты
        // ============================================================
        if (ui.speedSlider.isDragging) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            ui.speedSlider.value = posToValue(((float)mp.x - SLIDER_LEFT) / SLIDER_WIDTH);
            ui.savedSpeedValue = ui.speedSlider.value;
        }
        if (ui.tempSliderDragging) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            ui.targetTempCelsius = tempSliderPosToCelsius(((float)mp.x - TEMP_SLIDER_LEFT) / TEMP_SLIDER_WIDTH);
        }
        if (ui.boxSizeXDragging || ui.boxSizeYDragging) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            float pos = std::clamp(((float)mp.x - bTrackX1) / (bTrackX2 - bTrackX1), 0.0f, 1.0f);
            float size = boxSizePosToSize(pos);
            if (ui.boxSizeXDragging) ui.boxSizeX = size;
            if (ui.boxSizeYDragging) ui.boxSizeY = size;
        }

        // Стена в процессе рисования
        if (ui.isDrawingWall) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            ui.wallDrawCurrent = window.mapPixelToCoords(mp, camera);
        }

        // Гравитация — слайдеры
        if (ui.gravityDirDragging) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            float t = std::clamp(((float)mp.x - gpSliderX1) / (gpSliderX2 - gpSliderX1), 0.0f, 1.0f);
            ui.gravityDirDeg = t * 360.0f;
        }
        if (ui.gravityMagDragging) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            float t = std::clamp(((float)mp.x - gpSliderX1) / (gpSliderX2 - gpSliderX1), 0.0f, 1.0f);
            ui.gravityMagnitude = t * GRAVITY_MAG_MAX;
        }

        if (ui.rmbIsDown) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            float dx = (float)(mp.x - ui.rmbDownPixel.x);
            float dy = (float)(mp.y - ui.rmbDownPixel.y);
            if (dx * dx + dy * dy > DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX) {
                ui.rmbWasDragged = true;
            }
        }

        if (ui.lmbIsDown) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            ui.lmbCurrentPixel = mp;
            float dx = (float)(mp.x - ui.lmbDownPixel.x);
            float dy = (float)(mp.y - ui.lmbDownPixel.y);
            if (dx * dx + dy * dy > DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX) {
                ui.lmbIsDragging = true;
            }
            if (ui.lmbIsDragging) {
                sf::Vector2f w1 = window.mapPixelToCoords(ui.lmbDownPixel, camera);
                sf::Vector2f w2 = window.mapPixelToCoords(mp, camera);
                float minX = std::min(w1.x, w2.x);
                float maxX = std::max(w1.x, w2.x);
                float minY = std::min(w1.y, w2.y);
                float maxY = std::max(w1.y, w2.y);
                for (auto& a : atoms) {
                    a.selected = (a.pos.x >= minX && a.pos.x <= maxX &&
                        a.pos.y >= minY && a.pos.y <= maxY);
                }
                for (auto& w : walls) {
                    sf::Vector2f mid = (w.a + w.b) * 0.5f;
                    w.selected = (mid.x >= minX && mid.x <= maxX &&
                        mid.y >= minY && mid.y <= maxY);
                }
            }
        }

        if (isPanning) {
            sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);
            sf::Vector2i delta = lastMousePos - currentMousePos;
            lastMousePos = currentMousePos;
            sf::Vector2f viewSize = camera.getSize();
            float wppX = viewSize.x / (float)winSize.x;
            float wppY = viewSize.y / (float)winSize.y;
            camera.move({ delta.x * wppX, delta.y * wppY });
        }

        // На L6 камера управляется скриптом перелёта между кольцами;
        // фокус на уране только мешает.
        const bool l6CamLock =
            ui.campaignMode && ui.campaignLevel == 6;
        if (!l6CamLock
            && ui.focusedAtomIndex >= 0
            && ui.focusedAtomIndex < (int)atoms.size()) {
            sf::Vector2f target = atoms[ui.focusedAtomIndex].pos;
            sf::Vector2f current = camera.getCenter();
            float k = 0.15f;
            camera.setCenter(current + (target - current) * k);
        }

        // ============================================================
        // Кадровый апдейт + выбор состояния
        // ============================================================
        float dt = frameClock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;

        // Настройки из меню применяем всегда — они нужны и в меню, и в симуляции.
        ui.showDetailedAtoms = mainMenu.getSettings().detailedAtoms;
        ui.fxaaEnabled = mainMenu.getSettings().fxaaEnabled;
        ui.language = mainMenu.getSettings().language;

        sf::Vector2i mp = sf::Mouse::getPosition(window);

        // Подсчёт прогресса задач кампании.
// H2 — это пара атомов H, связанных ковалентной связью.
// Идём по всем атомам H и считаем связи с «напарниками»
// с индексом j > i (чтобы одну связь не посчитать дважды).
        if (ui.campaignMode && ui.campaignLevel == 1) {
            int h2 = 0;
            for (size_t i = 0; i < atoms.size(); ++i) {
                if (atoms[i].elementId != 0) continue;
                for (int j : atoms[i].bonds) {
                    if (j < 0 || j <= (int)i) continue;
                    if (j >= (int)atoms.size()) continue;
                    if (atoms[j].elementId == 0) h2++;
                }
            }
            ui.taskH2Count = h2;
            if (h2 > ui.taskH2MaxSeen) ui.taskH2MaxSeen = h2;

            // Фазовые переходы (однократные, по максимуму достигнутого)
            if (ui.taskPhase == 1 && ui.taskH2MaxSeen >= 5) {
                ui.taskPhase = 2;
                ui.taskH2Target = 25;
                ui.taskTextAlpha = 0.0f;   // запускаем fade-in
            }
            if (ui.taskPhase == 2 && ui.taskH2MaxSeen >= 25) {
                ui.taskPhase = 3;
                ui.taskH2Complete = true;
                ui.taskTextAlpha = 0.0f;   // запускаем fade-in
            }

            // Плавное появление текста задачи после смены фазы
            if (ui.taskTextAlpha < 1.0f) {
                ui.taskTextAlpha = std::min(1.0f, ui.taskTextAlpha + dt * 3.0f);
            }

            // Hover для кнопки "Next Level" (видна только в фазе 3)
            ui.nextLevelButtonHovered = false;
            if (ui.taskPhase == 3) {
                sf::FloatRect nr = nextLevelButtonRect(winSize, 400.0f);
                ui.nextLevelButtonHovered = nr.contains(
                    { (float)mp.x, (float)mp.y });
            }
        }

        // ============================================================
// Прогресс уровня 2 (Многоликая вода)
// ============================================================
        if (ui.campaignMode && ui.campaignLevel == 2) {
            ui.taskH2OCount = countWaterMolecules(atoms);
            ui.taskH2O2Count = countH2O2Molecules(atoms);
            ui.taskHBondCount = countActiveHBonds(atoms);

            // Фазовые переходы: по достижению цели текущей фазы
            if (ui.taskPhase == 1 && ui.taskH2OCount >= 5) {
                ui.taskPhase = 2;
                ui.taskTextAlpha = 0.0f;
            }
            else if (ui.taskPhase == 2 && ui.taskH2O2Count >= 5) {
                ui.taskPhase = 3;
                ui.taskTextAlpha = 0.0f;
            }
            else if (ui.taskPhase == 3
                && ui.taskH2OCount >= 5
                && ui.taskH2O2Count >= 5) {
                ui.taskPhase = 4;
                ui.taskTextAlpha = 0.0f;
                ui.hintButtonOpen = false;   // входим в новую hint-фазу
            }
            else if (ui.taskPhase == 4 && ui.taskHBondCount >= 8) {
                ui.taskPhase = 5;
                ui.taskTextAlpha = 0.0f;
            }

            if (ui.taskTextAlpha < 1.0f) {
                ui.taskTextAlpha = std::min(1.0f,
                    ui.taskTextAlpha + dt * 3.0f);
            }

            // Hover для Next Level (только в финальной фазе)
            ui.nextLevelButtonHovered = false;
            if (ui.taskPhase == 5) {
                sf::FloatRect nr = nextLevelButtonRect(winSize, 480.0f);
                ui.nextLevelButtonHovered = nr.contains(
                    { (float)mp.x, (float)mp.y });
            }
        }

        // ============================================================
        // Прогресс уровня 3 (Кислоты и ионы)
        // ============================================================
        if (ui.campaignMode && ui.campaignLevel == 3) {
            ui.taskHClCount = countHClMolecules(atoms);
            ui.taskH3OCount = countH3OMolecules(atoms);
            ui.taskSingleClCount = countSingleCl(atoms);

            if (ui.taskSingleClCount > ui.taskSingleClBest)
                ui.taskSingleClBest = ui.taskSingleClCount;

            // Таймер идёт только при 10+ одиночных Cl.
            // Учитываем ускорение времени: dt * timeScale.
            const float timeScaleNow =
                ui.pauseMenuOpen ? 0.0f : ui.speedSlider.value;

            if (ui.taskPhase == 0 && ui.taskHClCount >= 3) {
                ui.taskPhase = 1;
                ui.taskTextAlpha = 0.0f;
            }
            else if (ui.taskPhase == 1 && ui.taskH3OCount >= 3) {
                ui.taskPhase = 2;
                ui.taskTextAlpha = 0.0f;
                ui.taskClHoldTimer = 0.0f;
                ui.taskClTimerRunning = false;
            }
            else if (ui.taskPhase == 2) {
                if (ui.taskSingleClCount >= 10) {
                    ui.taskClTimerRunning = true;
                    ui.taskClHoldTimer += dt * timeScaleNow;
                    if (ui.taskClHoldTimer >= 30.0f) {
                        ui.taskPhase = 3;
                        ui.taskTextAlpha = 0.0f;
                        ui.hintButtonOpen = false;   // входим в новую hint-фазу
                    }
                }
                else {
                    if (ui.taskClTimerRunning) {
                        // только что просели — короткая красная вспышка
                        ui.taskClFlashTimer = 0.8f;
                    }
                    ui.taskClTimerRunning = false;
                    ui.taskClHoldTimer = 0.0f;
                }
            }

            if (ui.taskClFlashTimer > 0.0f)
                ui.taskClFlashTimer -= dt;

            if (ui.taskTextAlpha < 1.0f) {
                ui.taskTextAlpha = std::min(1.0f,
                    ui.taskTextAlpha + dt * 3.0f);
            }

            // Hover для Next Level (только в финальной фазе)
            ui.nextLevelButtonHovered = false;
            if (ui.taskPhase == 3) {
                sf::FloatRect nr = nextLevelButtonRect(winSize, 460.0f);
                ui.nextLevelButtonHovered = nr.contains(
                    { (float)mp.x, (float)mp.y });
            }
        }

        // ============================================================
        // Прогресс уровня 4 (Молекулярный мост)
        // ============================================================
        if (ui.campaignMode && ui.campaignLevel == 4) {
            // Cooldown
            if (ui.spawnCooldownTimer > 0.0f) {
                ui.spawnCooldownTimer -= dt;
                if (ui.spawnCooldownTimer < 0.0f) ui.spawnCooldownTimer = 0.0f;
            }

            // --- Построение графа воды по H-связям ---
            const int n = (int)atoms.size();

            std::vector<int> atomToWater(n, -1);
            std::vector<int> waterO;   // индексы O-атомов воды
            for (int i = 0; i < n; ++i) {
                if (atoms[i].elementId != 1) continue;
                int hCount = 0;
                bool hasO = false;
                for (int k : atoms[i].bonds) {
                    if (k < 0) continue;
                    if (atoms[k].elementId == 0) hCount++;
                    else if (atoms[k].elementId == 1) hasO = true;
                }
                if (hCount == 2 && !hasO) {
                    atomToWater[i] = (int)waterO.size();
                    waterO.push_back(i);
                    for (int k : atoms[i].bonds) {
                        if (k >= 0 && atoms[k].elementId == 0) {
                            atomToWater[k] = atomToWater[i];
                        }
                    }
                }
            }
            const int m = (int)waterO.size();

            // Union-Find по H-связям
            std::vector<int> parent(m);
            for (int i = 0; i < m; ++i) parent[i] = i;
            auto findRoot = [&](int x) {
                while (parent[x] != x) {
                    parent[x] = parent[parent[x]];
                    x = parent[x];
                }
                return x;
                };
            auto unite = [&](int a, int b) {
                a = findRoot(a); b = findRoot(b);
                if (a != b) parent[a] = b;
                };

            for (int i = 0; i < n; ++i) {
                int wi = atomToWater[i];
                if (wi < 0) continue;
                for (int j : atoms[i].hbonds) {
                    if (j < 0 || j >= n) continue;
                    int wj = atomToWater[j];
                    if (wj < 0 || wj == wi) continue;
                    unite(wi, wj);
                }
            }

            // Какая компонента касается каких колонн
            std::vector<int> compPillars(m, 0);  // bitmask
            for (int w = 0; w < m; ++w) {
                int oIdx = waterO[w];
                int root = findRoot(w);
                for (size_t p = 0; p < ui.pillars.size() && p < 3; ++p) {
                    float dx = atoms[oIdx].pos.x - ui.pillars[p].pos.x;
                    float dy = atoms[oIdx].pos.y - ui.pillars[p].pos.y;
                    float ext = ui.pillars[p].halfSize + L4_PILLAR_TOUCH_DIST;
                    if (std::abs(dx) <= ext && std::abs(dy) <= ext) {
                        compPillars[root] |= (1 << p);
                    }
                }
            }

            // Максимум соединённых колонн в одной компоненте
            int best = 0;
            for (int w = 0; w < m; ++w) {
                if (findRoot(w) != w) continue;
                int mask = compPillars[w];
                int cnt = 0;
                for (int p = 0; p < 3; ++p) if (mask & (1 << p)) cnt++;
                if (cnt > best) best = cnt;
            }
            ui.taskPillarsConnected = best;

            // Таймер удержания
            if (best >= 3 && ui.taskPhase == 1) {
                ui.taskBridgeHoldTimer += dt;
                ui.taskBridgeTimerRunning = true;
                if (ui.taskBridgeHoldTimer >= L4_BRIDGE_HOLD_TIME) {
                    ui.taskPhase = 2;
                    ui.taskTextAlpha = 0.0f;
                }
            }
            else if (ui.taskPhase == 1) {
                ui.taskBridgeHoldTimer = 0.0f;
                ui.taskBridgeTimerRunning = false;
            }

            if (ui.taskTextAlpha < 1.0f) {
                ui.taskTextAlpha = std::min(1.0f, ui.taskTextAlpha + dt * 3.0f);
            }

            ui.nextLevelButtonHovered = false;
            if (ui.taskPhase == 2) {
                sf::FloatRect nr = nextLevelButtonRect(winSize, L4_TASK_PANEL_H);
                ui.nextLevelButtonHovered = nr.contains(
                    { (float)mp.x, (float)mp.y });
            }
        }

        // ============================================================
// Прогресс уровня 6 (Взрывные кольца)
// ============================================================
        if (ui.campaignMode && ui.campaignLevel == 6) {
            // Считаем живой уран
            int aliveU = 0;
            for (const auto& a : atoms) if (a.elementId == 3) aliveU++;
            ui.ringUraniumRemaining = aliveU;

            // Спавн блокируем, если идёт пауза между фазами
            if (ui.phaseDelayTimer > 0.0f) {
                ui.phaseDelayTimer -= dt;
            }

            // Детонация текущего кольца
            if (ui.taskPhase >= 1 && ui.taskPhase <= L6_RING_COUNT
                && ui.ringUraniumRemaining == 0
                && ui.phaseDelayTimer <= 0.0f)
            {
                ui.phaseDelayTimer = L6_PHASE_DELAY;
                ui.ringIndex++;

                if (ui.ringIndex < L6_RING_COUNT) {
                    // Очищаем все «старые» нейтроны и гаммы, оставшиеся
                    // от предыдущего кольца. Иначе они долетают до свежего
                    // урана и мгновенно детонируют его — уровень сам
                    // «перескакивает» через кольца.
                    neutrons.clear();
                    gammas.clear();

                    // Спавн урана на следующем кольце
                    int idx = ui.ringIndex;
                    sf::Vector2f c{ L6_RING_X[idx], L6_RING_Y[idx] };
                    float baseY = c.y - L6_URANIUM_Y_OFFSET;
                    float totalW = (L6_URANIUM_PER_RING - 1)
                        * L6_URANIUM_SPACING;
                    float x0 = c.x - totalW * 0.5f;
                    for (int k = 0; k < L6_URANIUM_PER_RING; ++k) {
                        sf::Vector2f p(x0 + k * L6_URANIUM_SPACING, baseY);
                        atoms.push_back(makeAtom(3, p, { 0.0f, 0.0f }));
                    }
                    ui.ringUraniumRemaining = L6_URANIUM_PER_RING;

                    // Перенос spawn-зоны
                    // Сдвигаем зону влево от кольца, чтобы не спавнить
                    // нейтроны прямо внутри урана (иначе мгновенная детонация).
                    ui.activeSpawnZone.pos = { c.x - (L6_PILLAR_HALF_SIZE
                        + L6_SPAWN_ZONE_RADIUS + 3.0f), c.y };
                    ui.activeSpawnZone.active = true;

                    // Перелёт камеры
                    ui.cameraTransitionActive = true;
                    ui.cameraTransitionTarget = c;

                    ui.taskPhase++;
                    ui.taskTextAlpha = 0.0f;
                }
                else {
                    // Все кольца пройдены
                    ui.taskPhase = L6_RING_COUNT + 1;
                    ui.taskTextAlpha = 0.0f;
                    ui.activeSpawnZone.active = false;
                }
            }

            // Перелёт камеры
            if (ui.cameraTransitionActive) {
                sf::Vector2f cur = camera.getCenter();
                sf::Vector2f tgt = ui.cameraTransitionTarget;
                sf::Vector2f delta = tgt - cur;
                float len2 = delta.x * delta.x + delta.y * delta.y;
                if (len2 < 0.5f) {
                    camera.setCenter(tgt);
                    ui.cameraTransitionActive = false;
                }
                else {
                    camera.setCenter(cur + delta * 0.08f);
                }
            }

            if (ui.taskTextAlpha < 1.0f) {
                ui.taskTextAlpha = std::min(1.0f,
                    ui.taskTextAlpha + dt * 3.0f);
            }

            ui.nextLevelButtonHovered = false;
            if (ui.taskPhase == L6_RING_COUNT + 1) {
                sf::FloatRect nr = nextLevelButtonRect(winSize, L6_TASK_PANEL_H);
                ui.nextLevelButtonHovered = nr.contains(
                    { (float)mp.x, (float)mp.y });
            }
        }

        // В главном меню: работаем только с демо, основная
        // симуляция НЕ считается (экономия CPU и корректность).
        if (state == GameState::MainMenu) {
            mainMenu.updateHover(mp, winSize);
            mainMenu.update(dt, rng);
            mainMenu.render(window, font, fontLoaded);
            window.display();
            continue;
        }

        // ============================================================
        // Физика основной симуляции (только в state == Simulation)
        // ============================================================
        // Пауза: скорость = 0, пока открыто Esc-меню.
        // Настройка ui.speedSlider.value сохраняется — при закрытии
        // меню симуляция продолжит идти с той же скоростью.
        float timeScale =
            (ui.pauseMenuOpen || ui.sandboxAskDialog) ? 0.0f
            : ui.speedSlider.value;
        float stepsNeeded = SUBSTEPS * timeScale;
        physStepAccumulator += stepsNeeded;
        int stepsThisFrame = static_cast<int>(physStepAccumulator);
        if (stepsThisFrame > 2000) stepsThisFrame = 2000;
        physStepAccumulator -= stepsThisFrame;

        float physTemp = tempCelsiusToPhysics(ui.targetTempCelsius);

        for (int step = 0; step < stepsThisFrame; ++step) {
            // Перестраиваем сетку под текущие позиции
            grid.clear();
            for (size_t k = 0; k < atoms.size(); ++k)
                grid.insert((int)k, atoms[k].pos);

            updateBonds(atoms, grid);
            updateHBonds(atoms, grid, physTemp, rng);

            // Реакция Na + вода может удалить Na и компактизировать
            // atoms (erase уменьшает размер). После этого старый grid
            // хранит индексы, которых уже нет в atoms — при обращении
            // к ним computeInteraction упадёт с «vector subscript out
            // of range». Поэтому, если размер изменился, перестраиваем
            // grid под новое содержимое atoms.
            const size_t atomsSizeBeforeNa = atoms.size();
            applySodiumWaterReaction(atoms);
            if (atoms.size() != atomsSizeBeforeNa) {
                grid.clear();
                for (size_t k = 0; k < atoms.size(); ++k)
                    grid.insert((int)k, atoms[k].pos);
            }

            std::vector<sf::Vector2f> forces(atoms.size(), { 0.0f, 0.0f });

            for (size_t i = 0; i < atoms.size(); ++i) {
                int cx = grid.cellX(atoms[i].pos.x);
                int cy = grid.cellY(atoms[i].pos.y);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = cx + dx, ny = cy + dy;
                        if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows()) continue;
                        for (int j : grid.at(nx, ny)) {
                            if (j <= (int)i) continue;
                            float fx, fy;
                            computeInteraction(atoms, (int)i, j, fx, fy);
                            forces[i].x -= fx; forces[i].y -= fy;
                            forces[j].x += fx; forces[j].y += fy;
                        }
                    }
                }
            }

            // Угловые 3-частичные силы
            applyHBondAngularForces(atoms, forces);   // H-связь O_d—H···O_a
            applyWaterAngleForces(atoms, forces);     // угол H—O—H в воде
            applyPeroxideAngleForces(atoms, forces);  // угол H—O—O в H2O2

            // Гравитация
            if (ui.gravityEnabled) {
                float rad = ui.gravityDirDeg * 3.14159265f / 180.0f;
                float gx = std::cos(rad);
                float gy = std::sin(rad);
                float g = ui.gravityMagnitude * GRAVITY_ACCEL_SCALE;
                for (size_t i = 0; i < atoms.size(); ++i) {
                    forces[i].x += atoms[i].mass * g * gx;
                    forces[i].y += atoms[i].mass * g * gy;
                }
            }

            for (size_t i = 0; i < atoms.size(); ++i) {
                sf::Vector2f accel = forces[i] / atoms[i].mass;
                atoms[i].vel += accel * PHYS_DT;

                float noiseAmp = std::sqrt(2.0f * GAMMA * physTemp / atoms[i].mass * PHYS_DT);
                float vxNoise = noiseAmp * normalDist(rng);
                float vyNoise = noiseAmp * normalDist(rng);

                atoms[i].vel.x += -GAMMA * atoms[i].vel.x * PHYS_DT + vxNoise;
                atoms[i].vel.y += -GAMMA * atoms[i].vel.y * PHYS_DT + vyNoise;

                atoms[i].vel *= GLOBAL_DAMPING;
                atoms[i].pos += atoms[i].vel * PHYS_DT;
                atoms[i].age += PHYS_DT * 0.1f;
            }
            applyWallsToAtoms(atoms, walls);
            applyPillarsToAtoms(atoms, ui.pillars);
            applyBarriersToAtoms(atoms, ui.barriers);   // ← НОВОЕ: держит уран в корзине
            float halfX = ui.boxSizeX * 0.5f;
            float halfY = ui.boxSizeY * 0.5f;
            for (auto& a : atoms) {
                if (a.pos.x < -halfX + a.radius) { a.pos.x = -halfX + a.radius; a.vel.x = -a.vel.x * BOUNDARY_REST; }
                if (a.pos.x > halfX - a.radius) { a.pos.x = halfX - a.radius; a.vel.x = -a.vel.x * BOUNDARY_REST; }
                if (a.pos.y < -halfY + a.radius) { a.pos.y = -halfY + a.radius; a.vel.y = -a.vel.y * BOUNDARY_REST; }
                if (a.pos.y > halfY - a.radius) { a.pos.y = halfY - a.radius; a.vel.y = -a.vel.y * BOUNDARY_REST; }
            }
            // --- Нейтронный транспорт ---
            NeutronStepConfig nsCfg;
            if (ui.campaignMode && ui.campaignLevel == 6) {
                nsCfg.neutronGravity = true;
                nsCfg.gravityDirDeg = ui.gravityDirDeg;
                // ← НОВОЕ: нейтроны падают в 3 раза слабее атомов,
                // чтобы можно было добросить до дальних колец.
                nsCfg.gravityMag = ui.gravityMagnitude * GRAVITY_ACCEL_SCALE
                    * L6_NEUTRON_GRAVITY_MUL;
                nsCfg.dragScale = L6_NEUTRON_DRAG_SCALE;
                nsCfg.bouncePillars = true;
                nsCfg.pillarRestitution = L6_PILLAR_RESTITUTION;
                nsCfg.pillars = &ui.pillars;
                nsCfg.skipFissionFragments = true;
                nsCfg.recordTrail = true;
                nsCfg.barriers = &ui.barriers;
                nsCfg.minSpeed = L6_NEUTRON_MIN_SPEED;
            }

            stepNeutrons(neutrons, gammas, atoms, delayedPool, grid,
                sf::Vector2f{ ui.boxSizeX, ui.boxSizeY },
                PHYS_DT, rng, nsCfg);
            updateDelayedNeutrons(neutrons, delayedPool, PHYS_DT, rng);
            stepGammas(gammas, sf::Vector2f{ ui.boxSizeX, ui.boxSizeY }, PHYS_DT);
        }

        // Иначе — симуляция
        if (ui.fxaaEnabled && rtOk && fxaaOk) {
            // 1) Рендерим ТОЛЬКО мир в оффскрин-текстуру.
            //    FXAA применяется только к сцене, интерфейс не трогает.
            if (sceneRT.getSize() != window.getSize())
                (void)sceneRT.resize(window.getSize());

            drawEverything(sceneRT, mp, font, fontLoaded,
                atoms, walls, neutrons, gammas,
                camera, ui, sf::Vector2f{ ui.boxSizeX, ui.boxSizeY },
                DrawPass::World);
            sceneRT.display();

            // 2) Применяем FXAA при выкладывании текстуры в окно
            window.setView(sf::View(sf::FloatRect({ 0.0f, 0.0f },
                { (float)winSize.x, (float)winSize.y })));
            window.clear();
            sf::Sprite spr(sceneRT.getTexture());
            fxaaShader.setUniform("texture", sf::Shader::CurrentTexture);
            fxaaShader.setUniform("texelSize",
                sf::Glsl::Vec2(1.0f / (float)winSize.x, 1.0f / (float)winSize.y));
            window.draw(spr, &fxaaShader);

            // 3) Интерфейс поверх — без FXAA.
            drawEverything(window, mp, font, fontLoaded,
                atoms, walls, neutrons, gammas,
                camera, ui, sf::Vector2f{ ui.boxSizeX, ui.boxSizeY },
                DrawPass::UI);
        }
        else {
            drawEverything(window, mp, font, fontLoaded,
                atoms, walls, neutrons, gammas,
                camera, ui, sf::Vector2f{ ui.boxSizeX, ui.boxSizeY });
        }

        window.display();
    }
    return 0;
}