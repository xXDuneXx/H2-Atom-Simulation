#pragma once
#include "Types.hpp"
#include "Localization.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

// Всё UI-состояние, которое читает рендер и меняет обработка событий.
struct UIState {
    // Speed slider
    Slider speedSlider;
    float savedSpeedValue = 1.0f;

    // Показ зарядов атомов (тумблер в верхней панели)
    bool showCharges = false;

    // Temperature
    float targetTempCelsius = TEMP_DEFAULT_C;
    bool tempSliderDragging = false;
    bool tempEditing = false;
    std::string tempEditBuffer = "";

    // Focus & LMB selection
    int focusedAtomIndex = -1;
    bool lmbIsDown = false;
    bool lmbIsDragging = false;
    sf::Vector2i lmbDownPixel;
    sf::Vector2i lmbCurrentPixel;

    // RMB
    bool rmbIsDown = false;
    bool rmbWasDragged = false;
    sf::Vector2i rmbDownPixel;
    bool rmbMenuOpen = false;
    sf::Vector2i rmbMenuPos;

    // Copy mode
    bool copyMode = false;
    std::vector<sf::Vector2f> copyWallA;
    std::vector<sf::Vector2f> copyWallB;
    std::vector<sf::Vector2f> copyOffsets;
    std::vector<Atom> copyTemplates;
    float copyRotation = 0.0f;

    // Spawn menu
    bool spawnMenuOpen = true;
    int selectedSpawnType = -1;
    std::vector<int> batchCounts;
    int editingCountIndex = -1;
    std::string editBuffer = "";
    sf::Clock cursorClock;

    // Спавн с направлением (drag)
    bool             spawnDragActive = false;
    sf::Vector2f     spawnDragOrigin = { 0.0f, 0.0f };
    std::vector<int> spawnDragAtomIndices;
    std::vector<int> spawnDragNeutronIndices;

    // ПКМ-копирование: теперь тоже pending-спавн — атомы и стены
    // появляются только при отпускании ЛКМ, уже со скоростью.
    bool copyPendingSpawn = false;

    // Gravity (bottom-right panel)
    bool  gravityEnabled = false;
    float gravityDirDeg = GRAVITY_DEFAULT_DIR_DEG;
    float gravityMagnitude = GRAVITY_DEFAULT_MAG;
    bool  gravityDirDragging = false;
    bool  gravityMagDragging = false;

    // Wall drawing mode
    bool         wallDrawMode = false;
    bool         isDrawingWall = false;
    sf::Vector2f wallDrawStart;
    sf::Vector2f wallDrawCurrent;

    // Box size (bottom-left panel)
    float boxSizeX = BOX_DEFAULT;
    float boxSizeY = BOX_DEFAULT;
    bool  boxSizeXDragging = false;
    bool  boxSizeYDragging = false;

    // Settings
    bool showDetailedAtoms = true;
    bool fxaaEnabled = false;
    Language language = Language::EN;

    // Esc / pause menu
    bool pauseMenuOpen = false;
    bool pauseSettingsOpen = false;   // ← новое

    // Dropdown языка внутри pause-настроек
    bool pauseLanguageDropdownOpen = false;
    int  pauseLanguageDropdownHovered = -1;

    // Campaign
    bool campaignMode = false;   // включён режим кампании
    int  campaignLevel = 0;      // 1 = Origins of chemistry

    // Прогресс задач кампании (обновляется раз в кадр в AtomSimulation.cpp)
    int  taskPhase = 1;          // 1 = 5 H2, 2 = 25 H2, 3 = level complete
    int  taskH2Count = 0;        // текущее число молекул H2
    int  taskH2MaxSeen = 0;      // максимум H2 за всё время (для фаз)
    int  taskH2Target = 5;       // цель для текущей фазы (5 или 25)
    bool taskH2Complete = false;

    // Плавное появление текста задачи при смене фазы (0..1)
    float taskTextAlpha = 1.0f;

    // Кнопка "Next Level" (видна только в финальной фазе уровня)
    bool  nextLevelButtonHovered = false;

    // --- Счётчики уровня 2 ---
    int   taskH2OCount = 0;         // текущее число H2O
    int   taskH2O2Count = 0;        // текущее число H2O2
    int   taskHBondCount = 0;       // текущее число активных H-связей
    bool  hintButtonOpen = false;   // открыт ли блок подсказки в панели

    // --- Счётчики уровня 3 ---
    int   taskHClCount = 0;         // текущее число HCl
    int   taskH3OCount = 0;         // текущее число H3O+
    int   taskSingleClCount = 0;    // текущее число одиночных Cl
    int   taskSingleClBest = 0;     // максимум за заход (для дебага/UI)
    float taskClHoldTimer = 0.0f;   // сколько секунд держится >= 10 Cl
    bool  taskClTimerRunning = false;
    float taskClFlashTimer = 0.0f;  // короткая красная вспышка при сбросе

    // --- Уровень 4 — «Молекулярный мост» ---
    int  stockH = 0;               // оставшийся запас H
    int  stockO = 0;               // оставшийся запас O
    bool stockLimited = false;     // включён ли лимит (только L4)

    bool  spawnWaterSelected = false;   // выбран пункт Water в Spawn Menu
    float waterSpawnRotation = 0.0f;    // текущий угол поворота превью H2O

    float spawnCooldownTimer = 0.0f;    // 0..L4_SPAWN_COOLDOWN

    int   taskPillarsConnected = 0;     // 0..3
    float taskBridgeHoldTimer = 0.0f;   // сек, сколько держится мостик
    bool  taskBridgeTimerRunning = false;

    // Неподвижные препятствия (только L4)
    std::vector<Pillar> pillars;

    // --- Уровень 6 — «Взрывные кольца» ---
    std::vector<Barrier> barriers;     // неразрушимые борта
    SpawnZone activeSpawnZone;         // активная зона спавна (одна)

    int  ringIndex = 0;                 // 0..3 — текущее кольцо
    int  ringUraniumRemaining = 0;      // живого урана на текущем кольце
    int  ringUraniumTotal = 0;          // сколько было изначально

    // Для плавного перелёта камеры между кольцами
    bool  cameraTransitionActive = false;
    sf::Vector2f cameraTransitionTarget{ 0.0f, 0.0f };

    // Микро-пауза после детонации кольца, до перелёта
    float phaseDelayTimer = 1.0f;

    // Панель задачи L6 (заполняется в UI.cpp)
    sf::FloatRect l6HintButtonRect{ {-1.0f, -1.0f}, {0.0f, 0.0f} };

    // --- Диалог выбора карты при входе в песочницу ---
    bool sandboxAskDialog = false;
    bool sandboxEverEntered = false;   // ← новое

    // Прямоугольник кнопки «Show hint» — заполняется в drawEverything
    // для текущей фазы. Используется в AtomSimulation.cpp для hit-теста:
    // нельзя захардкодить Y, потому что цель бывает в 2 строки (\n),
    // и позиция кнопки тогда сдвигается ниже.
    // Начальное значение — заведомо «вне экрана».
    sf::FloatRect hintButtonRect = { {-1.0f, -1.0f}, {0.0f, 0.0f} };
};

// Фаза рендеринга.
//   All   — мир + интерфейс (обычный путь без FXAA);
//   World — только мир и температурная виньетка (идёт в RT для FXAA);
//   UI    — только интерфейс поверх уже отрисованного мира (без FXAA).
enum class DrawPass {
    All,
    World,
    UI,
};

// ============================================================
// Хелперы Spawn Menu (общие для AtomSimulation.cpp и UI.cpp)
// ============================================================
// На L4 к обычным строкам (H, O) добавляется строка «Water (H2O)».
// В песочнице — тоже добавляется Water в конец списка.
// Индексы:
//   !campaignMode            → 0..N-1 = атомы, N = Water
//   campaignLevel == 1       → [0] = H
//   campaignLevel == 2       → [0] = H, [1] = O
//   campaignLevel == 3       → [0] = H, [1] = O, [2] = Cl
//   campaignLevel == 4       → [0] = H, [1] = O, [2] = Water
inline bool isWaterRow(int row, bool campaignMode, int campaignLevel) {
    if (campaignMode && campaignLevel == 4) return row == 2;
    if (!campaignMode) return row == (int)ATOM_TYPES.size();
    return false;
}

// Строка Spawn Menu → elementId (для иконки/цвета) или -1 для воды
inline int rowElementId(int row, bool campaignMode, int campaignLevel) {
    if (isWaterRow(row, campaignMode, campaignLevel)) return -1;
    return row;
}

// Заблокированы ли регуляторы на L4
inline bool l4ControlsLocked(const UIState& ui) {
    return ui.campaignMode && ui.campaignLevel == 4;
}

// Единая точка входа рендеринга.
// ui передаётся НЕ константным — drawEverything записывает в него
// ui.hintButtonRect для hit-теста кнопки «Show hint».
void drawEverything(sf::RenderTarget& target,
    sf::Vector2i mousePixel,
    const sf::Font& font, bool fontLoaded,
    const std::vector<Atom>& atoms,
    const std::vector<Wall>& walls,
    const std::vector<Neutron>& neutrons,
    const std::vector<Gamma>& gammas,
    const sf::View& camera,
    UIState& ui,
    sf::Vector2f boxSize,
    DrawPass pass = DrawPass::All);

// Прямоугольник i-й кнопки pause-меню:
//   0 = Continue, 1 = Settings, 2 = Main Menu
sf::FloatRect pauseMenuButtonRect(sf::Vector2u winSize, int idx);

// Прямоугольник кнопки "Next Level" (кампания, финал уровня).
// panelH — высота панели задачи: 400 для L1, 480 для L2, 460 для L3.
sf::FloatRect nextLevelButtonRect(sf::Vector2u winSize,
    float panelH = 400.0f);