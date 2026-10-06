#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <cstring>

enum class Language { EN = 0, RU = 1, BROKEN = 2 };

struct LocString {
    const char* en;
    const char* ru;
};

// ============================================================
// Основная функция перевода.
// Возвращает sf::String, готовую для sf::Text:
//   EN     — ASCII, байты совпадают.
//   RU     — UTF-8 декодируется через fromUtf8().
//   BROKEN — байты UTF-8 как есть, но SFML видит их как ANSI
//            (однобайтовые Latin-1) → mojibake. Именно это
//            и даёт «сломанный» вид.
// ============================================================
inline sf::String tr(const LocString& s, Language lang) {
    const char* str = (lang == Language::EN) ? s.en : s.ru;
    if (lang == Language::RU) {
        return sf::String::fromUtf8(str, str + std::strlen(str));
    }
    // EN (чистый ASCII) и BROKEN (мусор) — байты как ANSI.
    return sf::String(str);
}

// Сырые байты — для snprintf и т.п.
inline const char* trRaw(const LocString& s, Language lang) {
    return (lang == Language::EN) ? s.en : s.ru;
}

// Совместимость со старым кодом (возвращает сырые байты).
inline std::string trs(const LocString& s, Language lang) {
    return std::string((lang == Language::EN) ? s.en : s.ru);
}

namespace Loc {
    // --- Main menu ---
    constexpr LocString BtnSimulation = { "Simulation",  "Симуляция" };
    constexpr LocString BtnSettings = { "Settings",    "Настройки" };
    constexpr LocString BtnExit = { "Exit",        "Выход" };
    constexpr LocString BtnCampaign = { "Campaign",    "Кампания" };
    constexpr LocString BtnSandbox = { "Sandbox",     "Песочница" };
    constexpr LocString BtnBack = { "Back",        "Назад" };
    constexpr LocString SelectMode = { "Select Mode",  "Выбор режима" };
    constexpr LocString SelectLevel = { "Select Level", "Выбор уровня" };
    constexpr LocString Level1Title = { "Origins of chemistry", "Истоки химии" };
    constexpr LocString Level1Subtitle = {
        "Tutorial - assemble the first molecules",
        "Обучение — соберите первые молекулы" };

    // --- Settings ---
    constexpr LocString DetailedAtoms = {
        "Detailed atoms (nucleus on zoom)",
        "Детальные атомы (ядро при зуме)" };
    constexpr LocString Fxaa = { "FXAA anti-aliasing", "FXAA сглаживание" };
    constexpr LocString LanguageLabel = { "Language", "Язык" };
    constexpr LocString EscBack = { "[Esc] back", "[Esc] назад" };

    // --- Top panel ---
    constexpr LocString Speed = { "Speed",   "Скорость" };
    constexpr LocString Temp = { "Temp",    "Темп." };
    constexpr LocString Charges = { "Charges", "Заряды" };

    // --- Gravity ---
    constexpr LocString Gravity = { "Gravity", "Гравитация" };
    constexpr LocString GravityAngle = { "Angle",   "Угол" };
    constexpr LocString GravityForce = { "Force",   "Сила" };

    // --- Box ---
    constexpr LocString BoxX = { "Box X", "Коробка X" };
    constexpr LocString BoxY = { "Box Y", "Коробка Y" };

    // --- Spawn menu ---
    constexpr LocString SpawnMenu = { "Spawn Menu", "Меню спавна" };
    constexpr LocString BatchSpawnBox = { "Batch spawn in box:", "Пакетный спавн в коробке:" };
    constexpr LocString SpawnBatch = { "Spawn Batch", "Спавн пакета" };
    constexpr LocString ClearAll = { "Clear All",   "Очистить всё" };
    constexpr LocString SpawnHint = {
        "[Esc] close  [LMB] place one  [Drag] select",
        "[Esc] закрыть  [ЛКМ] один  [Drag] выделение" };

    // --- Element names (spawn menu) ---
    constexpr LocString AtomHydrogen = { "Hydrogen",    "Водород" };
    constexpr LocString AtomOxygen = { "Oxygen",      "Кислород" };
    constexpr LocString AtomChlorine = { "Chlorine",    "Хлор" };
    constexpr LocString AtomUranium = { "Uranium-235", "Уран-235" };
    constexpr LocString AtomSodium = { "Sodium",      "Натрий" };
    constexpr LocString AtomBarium = { "Barium",      "Барий" };
    constexpr LocString AtomKrypton = { "Krypton",     "Криптон" };
    constexpr LocString AtomNeutron = { "Neutron",     "Нейтрон" };

    // --- Stats panel ---
    constexpr LocString StatsAtom = { "Atom",         "Атом" };
    constexpr LocString StatsMass = { "Mass",         "Масса" };
    constexpr LocString StatsRadius = { "Radius",       "Радиус" };
    constexpr LocString StatsAge = { "Age",          "Возраст" };
    constexpr LocString StatsSpeed = { "Speed",        "Скорость" };
    constexpr LocString StatsPosition = { "Position",     "Позиция" };
    constexpr LocString StatsBonds = { "Bonds",        "Связи" };
    constexpr LocString StatsHBonds = { "H-bonds",      "H-связи" };
    constexpr LocString StatsTetra = { "Tetra order",  "Тетра-пор." };
    constexpr LocString StatsPartners = { "Partners",     "Партнёры" };
    constexpr LocString StatsSelected = { "Selected",     "Выделен" };
    constexpr LocString StatsSelGroup = { "Sel. group",   "В группе" };
    constexpr LocString StatsYes = { "Yes", "Да" };
    constexpr LocString StatsNo = { "No",  "Нет" };

    // --- RMB menu ---
    constexpr LocString RmbCopy = { "Copy",     "Копировать" };
    constexpr LocString RmbDelete = { "Delete",   "Удалить" };
    constexpr LocString RmbDeselect = { "Deselect", "Снять" };

    // --- Wall mode ---
    constexpr LocString WallMode = {
        "WALL MODE  [W] off  [Esc] cancel",
        "РЕЖИМ СТЕН  [W] выкл  [Esc] отмена" };

    // --- Bottom hint ---
    constexpr LocString HintCampaign = {
        "[Tab] Spawn menu  [C] Charges  [Space] Pause  [R] Reset  "
        "[F11] Fullscreen  [LMB] Focus/Select  [RMB] Pan  [Wheel] Zoom",
        "[Tab] Меню спавна  [C] Заряды  [Space] Пауза  [R] Сброс  "
        "[F11] Полный экран  [ЛКМ] Выбор  [ПКМ] Панорама  [Колесо] Зум" };
    constexpr LocString HintSandbox = {
        "[Tab] Spawn menu  [W] Wall mode  [C] Charges  [Space] Pause  "
        "[R] Reset  [F11] Fullscreen  [LMB] Focus/Select  "
        "[RMB] Pan/Context  [Ctrl+Wheel] Rotate copy",
        "[Tab] Меню спавна  [W] Режим стен  [C] Заряды  [Space] Пауза  "
        "[R] Сброс  [F11] Полный экран  [ЛКМ] Выбор  "
        "[ПКМ] Панорама/Меню  [Ctrl+Колесо] Поворот копии" };

    // --- Pause menu ---
    constexpr LocString Paused = { "Paused",     "Пауза" };
    constexpr LocString PauseContinue = { "Continue",   "Продолжить" };
    constexpr LocString PauseSettings = { "Settings",   "Настройки" };
    constexpr LocString PauseMainMenu = { "Main Menu",  "В меню" };

    // --- Task / campaign ---
    constexpr LocString TaskHeader = { "Task", "Задание" };
    constexpr LocString GoalPhase1 = {
        "Goal: create 5 hydrogen molecules (H2).",
        "Цель: создайте 5 молекул водорода (H2)." };
    constexpr LocString GoalPhase2 = {
        "Goal: create 25 hydrogen molecules (H2).",
        "Цель: создайте 25 молекул водорода (H2)." };
    constexpr LocString GoalComplete = { "Goal complete!", "Цель достигнута!" };
    constexpr LocString TaskH2Prefix = { "H2 molecules:", "Молекул H2:" };
    constexpr LocString LevelComplete = { "Level complete!", "Уровень пройден!" };
    constexpr LocString CompressQuote = {
        "If you compress hydrogen, it will turn into metal.",
        "Если сжать водород, он превратится в металл." };
    constexpr LocString NextLevel = {
        "Next Level", "Следующий уровень" };

    constexpr LocString HowToPlay = { "How to play:", "Как играть:" };
    constexpr LocString Hint1_1 = {
        "1. Press [Tab] to open the Spawn Menu.",
        "1. Нажмите [Tab] чтобы открыть меню спавна." };
    constexpr LocString Hint1_2 = {
        "2. Select \"Hydrogen\".", "2. Выберите \"Водород\"." };
    constexpr LocString Hint1_3 = {
        "3. Click on the canvas to place an H atom.",
        "3. Кликните по полю, чтобы поставить атом H." };
    constexpr LocString Hint1_4 = {
        "Two H atoms close together bond into H2.",
        "Два близких атома H образуют H2." };
    constexpr LocString Hint1_5 = {
        "You can also throw them around - just hold LMB",
        "Их можно бросать — удерживайте ЛКМ" };
    constexpr LocString Hint1_6 = {
        "and drag to set direction and speed, then release.",
        "и тяните, чтобы задать направление и скорость." };

    constexpr LocString Hint2_1 = {
        "Spawn Batch is now unlocked!", "Пакетный спавн теперь доступен!" };
    constexpr LocString Hint2_2 = {
        "Open [Tab] Spawn Menu and press \"Spawn Batch\"",
        "Откройте [Tab] меню спавна и нажмите \"Спавн пакета\"" };
    constexpr LocString Hint2_3 = {
        "to drop a whole batch of atoms at once - much faster",
        "чтобы сразу высыпать партию атомов — гораздо быстрее," };
    constexpr LocString Hint2_4 = {
        "than clicking them one by one.", "чем ставить их по одному." };
    constexpr LocString Hint2_5 = {
        "Keep 25 H2 molecules present at the same time.",
        "Держите 25 молекул H2 одновременно." };
    constexpr LocString Hint2_6 = {
        "Throw them around with [LMB] drag to set speed.",
        "Бросайте их [ЛКМ] drag, чтобы задать скорость." };

    constexpr LocString Hint3_1 = { "You did it!", "У вас получилось!" };
    constexpr LocString Hint3_2 = {
        "25 hydrogen molecules were present at once.",
        "Одновременно было 25 молекул водорода." };
    constexpr LocString LangEnglish = { "English",  "English" };
    constexpr LocString LangRussian = { "Russian",  "Русский" };
    constexpr LocString LangBroken = { "Ð ÑƒÑÑÐºÐ¸Ð¹",   "Ð ÑƒÑÑÐºÐ¸Ð¹" };

    // ============================================================
    // Уровень 2 — «Многоликая вода»
    // ============================================================
    constexpr LocString Level2Title = {
        "Hybrid ice", "Гибридный лёд" };
    constexpr LocString Level2Subtitle = {
        "Oxygen unlocked - build water and peroxide",
        "Кислород открыт — соберите воду и пероксид" };

    constexpr LocString Goal2Phase1 = {
        "Goal: hold 5 water molecules (H2O)\nat once.",
        "Цель: удерживайте 5 молекул воды (H2O)\nодновременно." };
    constexpr LocString Goal2Phase2 = {
        "Goal: create 5 hydrogen peroxide (H2O2).",
        "Цель: создайте 5 молекул пероксида водорода (H2O2)." };
    constexpr LocString Goal2Phase3 = {
        "Goal: hold 5 H2O and 5 H2O2\nat once.",
        "Цель: удерживайте 5 H2O и 5 H2O2\nодновременно." };
    constexpr LocString Goal2Phase4 = {
        "Goal: freeze water - keep 8 H-bonds\nat once.",
        "Цель: заморозьте воду — держите 8 H-связей\nодновременно." };
    constexpr LocString Goal2Complete = { "Goal complete!", "Цель достигнута!" };

    constexpr LocString TaskH2OPrefix = { "H2O:",  "H2O:" };
    constexpr LocString TaskH2O2Prefix = { "H2O2:", "H2O2:" };
    constexpr LocString TaskHBondPrefix = { "H-bonds:", "H-связи:" };

    // --- подсказки уровня 2, фаза 1 (собрать воду) ---
    constexpr LocString HintL2_1_1 = {
        "1. [Tab] Spawn Menu -> Oxygen.",
        "1. [Tab] меню спавна → Кислород." };
    constexpr LocString HintL2_1_2 = {
        "2. One O + two H bond into water (H2O).",
        "2. Один O + два H образуют воду (H2O)." };
    constexpr LocString HintL2_1_3 = {
        "3. Keep 5 H2O molecules at the same time.",
        "3. Держите 5 молекул H2O одновременно." };
    constexpr LocString HintL2_1_4 = {
        "Tip: drag-throw H atoms toward O.",
        "Совет: бросайте атомы H в сторону O (drag)." };

    // --- подсказки уровня 2, фаза 2 (пероксид) ---
    constexpr LocString HintL2_2_1 = {
        "H2O2 needs two HO groups (each O has 1 H).",
        "H2O2 получается из двух групп HO (у каждой O — 1 H)." };
    constexpr LocString HintL2_2_2 = {
        "First make a free HO: 1 O + 1 H.",
        "Сначала получите свободный HO: 1 O + 1 H." };
    constexpr LocString HintL2_2_3 = {
        "Then bring two HO groups close together.",
        "Затем сведите две группы HO вместе." };
    constexpr LocString HintL2_2_4 = {
        "Important: a FULL water (O+2H) will NOT",
        "Важно: ПОЛНАЯ вода (O+2H) НЕ даст" };
    constexpr LocString HintL2_2_5 = {
        "form peroxide - leave O with only 1 H.",
        "пероксид — оставляйте O только с 1 H." };

    // --- подсказки уровня 2, фаза 4 (лёд) ---
    constexpr LocString HintL2_4_2 = {
        "Cool the box below 0 C to freeze water.",
        "Охладите коробку ниже 0 °C — вода замёрзнет." };
    constexpr LocString HintL2_4_3 = {
        "Water molecules will link via H-bonds.",
        "Молекулы воды соединятся H-связями." };
    constexpr LocString HintL2_4_4 = {
        "H2O2 can also H-bond with water.",
        "H2O2 тоже образует H-связи с водой." };

    constexpr LocString ShowHint = { "Show hint", "Показать подсказку" };
    constexpr LocString HideHint = { "Hide hint", "Скрыть подсказку" };
    constexpr LocString FinishLevel = { "Back to menu", "В меню" };

    // --- Диалог при входе в песочницу ---
    constexpr LocString SandboxAskTitle = {
        "Choose a map", "Выберите карту" };
    constexpr LocString SandboxDefault = {
        "Standard sandbox", "Стандартная" };
    constexpr LocString SandboxKeepLast = {
        "Keep last layout", "Оставить прошлую" };

    // --- Уровень 2: дополнительный хинт фазы 1 (ПКМ-меню) ---
    // Важно: без выделения ПКМ-меню не откроется — это надо
    // объяснить прямым текстом.
    constexpr LocString HintL2_1_5 = {
        "Tip: LMB-drag selects atoms; RMB then opens",
        "Совет: ЛКМ-рамкой выделите атомы, затем ПКМ откроет" };
    constexpr LocString HintL2_1_6 = {
        "the copy / delete / deselect menu.",
        "меню копировать / удалить / снять выделение." };

    // --- Уровень 2: хинт фазы 3 (разблокировка температуры) ---
    constexpr LocString HintL2_3_1 = {
        "Temperature control is now unlocked!",
        "Регулировка температуры теперь доступна!" };

    // --- Финальные описания для L2 и L3 ---
    constexpr LocString Level2CompleteDesc = {
        "5 H2O and 5 H2O2 with 8 H-bonds were present.",
        "Одновременно было 5 H2O, 5 H2O2 и 8 H-связей." };
    constexpr LocString Level3CompleteDesc = {
        "10+ single Cl atoms were held for 30 seconds.",
        "Одновременно держались 10+ одиночных Cl 30 секунд." };

    // --- Жёлтый курсив для L2 ---
    constexpr LocString PeroxideQuote = {
        "Hydrogen peroxide bonds with water via H-bonds.",
        "Перекись водорода присоединяется к воде водородными\nсвязями." };

    // ============================================================
    // Уровень 3 — «Кислоты и ионы»
    // ============================================================
    constexpr LocString Level3Title = {
        "Acids and ions", "Кислоты и ионы" };
    constexpr LocString Level3Subtitle = {
        "Chlorine unlocked - split the acid, hold the ions",
        "Хлор открыт — расщепите кислоту, удержите ионы" };

    constexpr LocString Goal3Phase0 = {
        "Goal: hold 3 HCl molecules\nat once.",
        "Цель: удерживайте 3 молекулы HCl\nодновременно." };
    constexpr LocString Goal3Phase1 = {
        "Goal: hold 3 hydronium ions (H3O+)\nat once.",
        "Цель: удерживайте 3 иона гидроксония (H3O+)\nодновременно." };
    constexpr LocString Goal3Phase2 = {
        "Goal: keep 10+ single Cl atoms for 30 s.",
        "Цель: удерживайте 10+ одиночных Cl в течение 30 с." };
    constexpr LocString Goal3Complete = { "Goal complete!", "Цель достигнута!" };

    constexpr LocString TaskHClPrefix = { "HCl:",  "HCl:" };
    constexpr LocString TaskH3OPrefix = { "H3O+:", "H3O+:" };
    constexpr LocString TaskClPrefix = { "Cl:",   "Cl:" };

    // --- подсказки уровня 3, фаза 1 (получить H3O+) ---
    constexpr LocString Hint3_1_1 = {
        "HCl in water splits: HCl + H2O -> H3O+ + Cl-.",
        "HCl в воде распадается: HCl + H2O → H3O+ + Cl−." };
    constexpr LocString Hint3_1_2 = {
        "Make HCl first: spawn Cl and H away from water.",
        "Сначала получите HCl: спавньте Cl и H вдали от воды." };
    constexpr LocString Hint3_1_3 = {
        "Then drag the HCl molecule into water -",
        "Затем перетащите молекулу HCl в воду —" };
    constexpr LocString Hint3_1_4 = {
        "H jumps to O, leaving Cl- behind.",
        "H перескочит на O, оставив Cl− позади." };
    constexpr LocString Hint3_1_5 = {
        "Keep 3 H3O+ ions at the same time.",
        "Держите 3 иона H3O+ одновременно." };

    // --- подсказки уровня 3, фаза 2 (одиночные Cl) ---
    constexpr LocString Hint3_2_1 = {
        "Cl does NOT bond with another Cl near water.",
        "Рядом с водой Cl НЕ связывается с другим Cl." };
    constexpr LocString Hint3_2_2 = {
        "Flood the box with water and spawn Cl atoms.",
        "Заполните коробку водой и спавньте атомы Cl." };
    constexpr LocString Hint3_2_3 = {
        "Keep 10+ single Cl atoms for 30 seconds.",
        "Держите 10+ одиночных Cl в течение 30 секунд." };

    // ============================================================
    // Уровень 4 — «Молекулярный мост»
    // ============================================================
    constexpr LocString Level4Title = {
        "Molecular bridge", "Молекулярный мост" };
    constexpr LocString Level4Subtitle = {
        "Build a continuous water chain between the pillars",
        "Постройте непрерывную водную цепь между колоннами" };

    constexpr LocString Goal4Phase1 = {
        "Goal: connect all three pillars with one\ncontinuous chain of water molecules.",
        "Цель: соедините все три колонны единой\nнепрерывной цепью молекул воды." };
    constexpr LocString Goal4Complete = { "Goal complete!", "Цель достигнута!" };

    constexpr LocString Hint4_1 = {
        "1. Spawn H2O via Spawn Menu.",
        "1. Спавньте H2O через меню спавна." };
    constexpr LocString Hint4_2 = {
        "2. Hold Ctrl + wheel to rotate the molecule.",
        "2. Удерживайте Ctrl + колесо, чтобы повернуть молекулу." };
    constexpr LocString Hint4_3 = {
        "3. Connect all three pillars in one chain.",
        "3. Соедините все три колонны одной цепью." };
    constexpr LocString Hint4_4 = {
        "4. The bridge must hold for 10 seconds.",
        "4. Мостик должен держаться 10 секунд." };
    constexpr LocString Hint4Gravity = {
        "Note: weak gravity pulls everything down.",
        "Внимание: слабая гравитация тянет всё вниз." };
    constexpr LocString Hint4NoPause = {
        "[Space] pause is disabled on this level.",
        "[Space] пауза отключена на этом уровне." };

    constexpr LocString TaskPillarsPrefix = { "Pillars:", "Колонны:" };
    constexpr LocString TaskBridgePrefix = { "Hold:",    "Держится:" };
    constexpr LocString TaskStockPrefix = { "Stock",    "Запас" };

    constexpr LocString AtomWater = { "Water (H2O)", "Вода (H2O)" };

    constexpr LocString Level4CompleteDesc = {
        "A stable water bridge spans all three pillars.",
        "Устойчивый водный мостик соединяет все три колонны." };

    // ============================================================
    // Уровень 6 — «Взрывные кольца»
    // ============================================================
    constexpr LocString Level6Title = {
        "Explosive rings", "Взрывные кольца" };
    constexpr LocString Level6Subtitle = {
        "Throw neutrons into uranium rings(In development)",
        "Забросьте нейтроны в урановые кольца(В разработке)" };

    constexpr LocString Goal6Phase1 = {
        "Goal: detonate all uranium on the current ring.",
        "Цель: детонируйте весь уран на текущем кольце." };
    constexpr LocString Goal6Complete = { "Goal complete!", "Цель достигнута!" };

    constexpr LocString TaskRingsPrefix = { "Rings:", "Кольца:" };
    constexpr LocString TaskUraniumPrefix = { "Uranium:", "Уран:" };

    constexpr LocString HintL6_1 = {
        "Hold LMB inside the glowing zone, drag to aim,",
        "Удерживайте ЛКМ в светящейся зоне, тяните для прицела," };
    constexpr LocString HintL6_2 = {
        "release to throw the neutron.",
        "отпустите — нейтрон полетит." };

    constexpr LocString Level6CompleteDesc = {
        "All uranium rings detonated.",
        "Все урановые кольца детонированы." };
}