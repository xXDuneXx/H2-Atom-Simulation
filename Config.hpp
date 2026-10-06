#pragma once
#include <SFML/Graphics.hpp>

// ============================================================
// Физические параметры
// ============================================================
const float H_MASS = 1.008f;
const float O_MASS = 15.999f;
const float CL_MASS = 35.45f;
const float NEUTRON_MASS = 1.0f;
const float U235_MASS = 235.0f;
const float NA_MASS = 22.99f;
const float BA_MASS = 138.0f;   // типичный осколок Ba-138 (Z=56, N=82)
const float KR_MASS = 94.0f;   // типичный осколок Kr-94  (Z=36, N=58)

const float F_MAX = 50.0f;

const float VDW_De = 0.02f;
const float VDW_re = 3.5f;
const float VDW_a = 0.8f;
const float VDW_CUTOFF = 5.0f;

const float BOND_FORM_DIST = 1.4f;   // увеличено под O-O (re=1.21)
const float BOND_BREAK_DIST = 2.5f;
const float HARD_CORE_K = 30.0f;   // было 40 — при формировании воды H не так резко отлетает

const float PHYS_DT = 0.005f;
const int   SUBSTEPS = 20;
const float BOUNDARY_REST = 0.9f;

const float GAMMA = 0.5f;
const float GLOBAL_DAMPING = 1.0f;

const float TEMP_MIN_C = -273.15f;
const float TEMP_MAX_C = 110.0f;
const float TEMP_DEFAULT_C = 0.0f;
const float TEMP_PHYS_MAX = 0.3f;

const float DRAG_THRESHOLD_PX = 6.0f;

// ============================================================
// Нуклоны в ядрах
// ============================================================
const int   MAX_NUCLEONS_DISPLAY = 45;    // предел для тяжёлых ядер

// Базовый радиус нуклона. При NUCLEUS_SIZE_SCALE = 1.0 ядро выглядит
// «реалистично» — очень маленьким по сравнению с электронным облаком.
const float NUCLEON_RADIUS = 0.024f;      // Å

// Множитель размера ядра (только рендер, на физику не влияет):
//   1.0  — реалистично (очень маленькое ядро);
//   >1.0 — ядро крупнее, лучше видны нуклоны и подписи P/n.
// Попробуй, например, 3.0f, чтобы разглядеть детали.
const float NUCLEUS_SIZE_SCALE = 0.5f;

// ============================================================
// Спавн с направлением/скоростью (drag)
// ============================================================
const float SPAWN_DRAG_MIN_PX = 8.0f;    // порог в пикселях
const float SPAWN_SPEED_K = 2.0f;    // (у.е./с) на 1 Å длины стрелки
const float SPAWN_SPEED_MAX = 15.0f;   // максимальная скорость при drag

// ============================================================
// Реакция натрия с водой: 2Na + 2H2O → 2NaOH + H2 + тепло
// ============================================================
// В 2D-модели: как только Na оказывается ближе этого расстояния
// к кислороду молекулы воды (O с двумя H), срабатывает «взрыв»:
// обе O–H связи рвутся, атомы H разлетаются с большой скоростью,
// все соседние атомы получают радиальный импульс, а сам Na
// исчезает (превратился в NaOH, который мы не моделируем).
const float NA_WATER_REACT_DIST = 1.8f;   // Å, дистанция срабатывания
const float NA_EXPLOSION_KICK = 18.0f;  // у.е./с — скорость осколков
const float NA_EXPLOSION_RADIUS = 4.0f;   // Å — радиус ударной волны

// ============================================================
// Ионная связь Na+Cl- (NaCl)
// ============================================================
// Натрий и хлор образуют ионную пару. В 2D-модели это
// Morse-яма с большой глубиной и длинным re — она визуально
// даёт устойчивую пару Na-Cl и не даёт им разлететься.
// Na образует ТОЛЬКО эту связь (со всем остальным — только
// vdW), поэтому существующие соединения (вода, HCl, Cl2,
// H2O2) не затрагиваются.
const float NACL_De = 3.0f;
const float NACL_re = 2.20f;
const float NACL_a = 1.5f;
const float NACL_cutoff = 3.5f;

// ============================================================
// HCl-лёд: усиленная водородная связь H···Cl
// ============================================================
const float HCL_HB_De = 0.7f;          // было 0.4 — слишком слабо
const float HCL_HB_re = 2.20f;         // как было
const float HCL_HB_a = 1.5f;
const float HCL_HB_cutoff = 3.5f;

// Регистрация hbond H···Cl (как для воды, но своё окно)
const float HCL_HB_FORM_DIST = 2.60f;   // дистанция регистрации
const float HCL_HB_BREAK_DIST = 3.50f;   // дистанция разрыва
const float HCL_HB_ANGLE_COS_MIN = 0.5f;   // мягче, чем у воды (0.7)
const int   HCL_HB_SLOTS_CL = 2;       // Cl принимает до 2 H-связей

// ============================================================
// Испарение воды / плавление HCl-льда
// ============================================================
// physTemp: 0°C → 0.07, 15°C → 0.24, 110°C → 0.30
//           −85°C → 0.0482 (точка плавления HCl-льда)
// Формирование новых H-связей запрещено выше порога.
// Существующие H-связи рвутся тепловым шумом между MIN и MAX.
const float HBOND_FORM_TEMP_MAX = 0.28f;   // ~80°C  — вода испаряется
const float HBOND_BREAK_TEMP_MIN = 0.26f;   // ~50°C  — начало разрыва
const float HBOND_BREAK_TEMP_MAX = 0.30f;   // ~110°C — полный разрыв

// HCl-лёд: плавление/сублимация около −85°C (как в реальности).
// physTemp(−85°C) ≈ 0.0482, physTemp(−78°C) ≈ 0.050.
//   Formation: разрешено только при physTemp < 0.0485 (≈ −84°C).
//   Breaking :  начинается сразу выше порога и полностью завершается
//               к physTemp ≈ 0.055 (≈ −56°C).
const float HCL_HBOND_FORM_TEMP_MAX = 0.0485f;   // ~-84°C
const float HCL_HB_BREAK_TEMP_MIN = 0.0485f;   // ~-84°C
const float HCL_HB_BREAK_TEMP_MAX = 0.055f;    // ~-56°C

// ============================================================
// Morse-параметры для каждой пары элементов
// ============================================================
struct MorsePair {
    float De, re, a, cutoff;
};

// elementId: 0 = H, 1 = O, 2 = Cl
inline MorsePair getMorsePair(int e1, int e2) {
    // Канонизируем порядок для асимметричных пар
    int lo = std::min(e1, e2);
    int hi = std::max(e1, e2);

    if (lo == 0 && hi == 0) return { 5.0f, 0.74f, 1.94f, 3.0f }; // H-H
    if (lo == 1 && hi == 1) return { 5.5f, 1.21f, 2.00f, 3.2f }; // O-O
    if (lo == 2 && hi == 2) return { 2.5f, 1.99f, 1.50f, 3.5f }; // Cl-Cl
    if (lo == 0 && hi == 1) return { 5.0f, 0.97f, 1.80f, 3.0f }; // H-O
    if (lo == 0 && hi == 2) return { 4.5f, 1.27f, 1.80f, 3.0f }; // H-Cl
    if (lo == 1 && hi == 2) return { 0.5f, 3.00f, 1.50f, 3.5f }; // O-Cl (слабое vdW)
    if (lo == 2 && hi == 4) return { NACL_De, NACL_re, NACL_a, NACL_cutoff }; // Na-Cl
    return { 5.0f, 0.97f, 1.80f, 3.0f };                          // fallback
}

// ============================================================
// Панели
// ============================================================
const float STATS_W = 210.0f;
const float STATS_MARGIN = 12.0f;

const float PANEL_HEIGHT = 44.0f;
const float SLIDER_LEFT = 90.0f;
const float SLIDER_WIDTH = 300.0f;
const float SLIDER_KNOB_R = 8.0f;
const float SPEED_MIN = 0.01f;
const float SPEED_MAX = 50.0f;
const float RESET_BTN_W = 46.0f;
const float RESET_BTN_H = 26.0f;
const float RESET_BTN_GAP = 70.0f;

const float TEMP_LABEL_X = 515.0f;
const float TEMP_SLIDER_LEFT = 570.0f;
const float TEMP_SLIDER_WIDTH = 140.0f;
const float TEMP_FIELD_LEFT = 720.0f;
const float TEMP_FIELD_W = 90.0f;
const float TEMP_FIELD_H = 26.0f;
const float TEMP_RESET_LEFT = 830.0f;
const float TEMP_RESET_W = 40.0f;
const float TEMP_RESET_H = 26.0f;

// Кнопка "Charges" (справа от temp-сброса, x=890)
const float CHARGE_BTN_LEFT = 890.0f;
const float CHARGE_BTN_W = 100.0f;
const float CHARGE_BTN_H = 26.0f;

const float MENU_LEFT = 12.0f;
const float MENU_TOP = PANEL_HEIGHT + 10.0f;
const float MENU_WIDTH = 300.0f;
const float MENU_HEADER_H = 26.0f;
const float MENU_ROW_H = 34.0f;
const float MENU_FOOTER_H = 108.0f;

const float ROW_ICON_X = 22.0f;
const float ROW_ICON_R = 9.0f;
const float ROW_NAME_X = 42.0f;
const float ROW_MINUS_X = 180.0f;
const float ROW_FIELD_X = 206.0f;
const float ROW_FIELD_W = 50.0f;
const float ROW_PLUS_X = 258.0f;
const float ROW_BTN_W = 24.0f;
const float ROW_BTN_H = 22.0f;
const float ROW_SELECT_W = 170.0f;

// ============================================================
// Панель размера коробки (bottom-left)
// ============================================================
const float BOX_PANEL_LEFT = 12.0f;
const float BOX_PANEL_WIDTH = 280.0f;
const float BOX_PANEL_HEIGHT = 76.0f;
const float BOX_PANEL_BOTTOM_OFFSET = 32.0f;   // расстояние от низа окна до низа панели

const float BOX_LABEL_X = 12.0f;   // внутри панели
const float BOX_LABEL_W = 50.0f;
const float BOX_TRACK_X = 64.0f;   // внутри панели
const float BOX_TRACK_W = 155.0f;
const float BOX_VALUE_X = 226.0f;  // левый край текста значения
const float BOX_VALUE_W = 46.0f;

const float BOX_ROW1_Y = 22.0f;   // центр строки "Box X" (относительно верха панели)
const float BOX_ROW2_Y = 52.0f;   // центр строки "Box Y"

const float BOX_KNOB_R = 7.0f;
const float BOX_TRACK_THICKNESS = 4.0f;

const float BOX_MIN = 4.0f;    // минимальный размер (больше 2×max радиус = 0.9)
const float BOX_MAX = 80.0f;
const float BOX_DEFAULT = 20.0f;

// ============================================================
// TIP4P-стиль: виртуальный сайт M на кислороде
// ============================================================
// В модели TIP4P отрицательный «центр» кислорода вынесен из ядра O
// в виртуальный сайт M, лежащий на биссектрисе H–O–H со стороны
// неподелённых пар (т.е. в стороне, противоположной обоим H).
// Это устраняет искусственную симметрию точечного заряда на O
// и делает направленность H-связи физически корректной.
const float TIP4P_OM_DIST = 0.15f;   // Å, смещение M от ядра O

// ============================================================
// Водородная связь (для воды/льда)
// ============================================================
// Радиальные параметры H-связи теперь относятся к расстоянию H···M
// (а не H···O), т.к. акцепторный центр перенесён на M-сайт.
const float HBOND_De = 1.0f;
const float HBOND_re = 1.70f;         // H···M равновесное (было 1.85 для H···O)
const float HBOND_a = 1.5f;
const float HBOND_CUTOFF = 2.85f;     // H···M cutoff (было 3.0)
const float HBOND_FORM_DIST = 2.15f;  // (было 2.3)
const float HBOND_BREAK_DIST = 2.85f; // (было 3.0)
const float HBOND_ANGLE_COS_MIN = 0.7f;  // усилили линейность (было 0.5)
const int   HBOND_SLOTS_O = 2;
const float HBOND_K_ANG = 10.0f;     // было 15 — тоже подрежем на всякий случай
const float ANGLE_FORCE_MAX = 20.0f; // НОВОЕ: потолок силы от углов

// НОВОЕ: жёсткость угла H-O-H у молекулы воды
const float WATER_ANGLE_DEG = 104.5f;
const float WATER_ANGLE_K = 11.0f; // было 40 — мягче сводит угол H-O-H, H не вылетает

// ============================================================
// Диссоциация HCl в воде (кислотное поведение)
// ============================================================
// Если H из HCl оказывается ближе этого расстояния к атому O,
// у которого есть свободный слот, H-Cl рвётся: H отдаётся O.
// Это и есть имитация реакции HCl + H2O → H3O+ + Cl-.
const float HCL_DISSOC_DIST = 2.3f;

// ============================================================
// Пероксид водорода H2O2
// ============================================================
// Одинарная O-O связь в H2O2 — заметно слабее и длиннее, чем двойная
// O=O в O2. Реальные значения:
//   O=O (O2):    De ≈ 5.5, re ≈ 1.21 Å
//   O-O (H2O2):  De ≈ 1.5, re ≈ 1.48 Å   (в ~3.5 раза слабее)
const float OO_SINGLE_De = 2.5f;
const float OO_SINGLE_re = 1.48f;
const float OO_SINGLE_a = 2.0f;
const float OO_SINGLE_cutoff = 3.2f;

// Угол H-O-O в H2O2. Реально ~97°, по запросу — 110°.
const float PEROXIDE_ANGLE_DEG = 115.0f;
const float PEROXIDE_ANGLE_K = 11.0f;

// ============================================================
// Гравитация
// ============================================================
const float GRAVITY_DEFAULT_DIR_DEG = 90.0f;    // 90° = вниз (экран +Y)
const float GRAVITY_DEFAULT_MAG = 1.0f;
const float GRAVITY_MAG_MAX = 3.0f;
const float GRAVITY_ACCEL_SCALE = 2.0f;     // g_max = 6.0 (в единицах F/m)

// Панель гравитации (bottom-right)
const float GRAV_PANEL_W = 300.0f;
const float GRAV_PANEL_H = 100.0f;
const float GRAV_PANEL_RIGHT_MARGIN = 12.0f;
const float GRAV_PANEL_BOTTOM_OFFSET = 32.0f;
const float GRAV_LABEL_X = 15.0f;
const float GRAV_CHECKBOX_X = 85.0f;
const float GRAV_CHECKBOX_SIZE = 14.0f;
const float GRAV_SLIDER_X = 85.0f;
const float GRAV_SLIDER_W = 195.0f;
const float GRAV_ROW1_Y = 25.0f;
const float GRAV_ROW2_Y = 55.0f;
const float GRAV_ROW3_Y = 85.0f;
const float GRAV_KNOB_R = 7.0f;

// ============================================================
// Стены
// ============================================================
const float WALL_THICKNESS = 0.10f;    // для рендера и столкновений
const float WALL_HIT_RADIUS = 0.30f;    // клик попадания
const float WALL_MIN_LENGTH = 0.5f;     // мин. длина, чтобы создалась
const float WALL_RESTITUTION = 0.5f;     // отскок

// ============================================================
// Esc / pause menu
// ============================================================
const float PAUSE_MENU_BTN_W = 320.0f;
const float PAUSE_MENU_BTN_H = 60.0f;
const float PAUSE_MENU_BTN_GAP = 18.0f;
const float PAUSE_MENU_CX_FRAC = 0.5f;    // центр по горизонтали
const float PAUSE_MENU_START_Y_FRAC = 0.42f;   // верх первой кнопки
const float PAUSE_MENU_TITLE_Y_FRAC = 0.28f;   // центр заголовка "Paused"

// ============================================================
// Morse-параметры для каждой пары элементов
// ============================================================

struct MorseParams {
    float De;
    float re;
    float a;
};

// Функция для получения параметров в зависимости от ID элементов (0 = H, 1 = O)
inline MorseParams getMorseParams(int e1, int e2) {
    // H-H (Водород)
    if (e1 == 0 && e2 == 0) return { 1.5f, 0.74f, 2.0f };
    // O-O (Кислород, двойная связь)
    if (e1 == 1 && e2 == 1) return { 3.0f, 1.21f, 2.5f };
    // H-O (Вода или гидроксил)
    return { 2.0f, 0.96f, 2.0f };
}

// Параметры с учётом порядка связи: одинарная O-O (H2O2) — своя,
// всё остальное — стандартные из getMorsePair.
// order = 1 для O-O в H2O2; order >= 2 для O=O в O2.
inline MorsePair getMorsePairByOrder(int e1, int e2, int order) {
    if (e1 == 1 && e2 == 1 && order <= 1) {
        return { OO_SINGLE_De, OO_SINGLE_re, OO_SINGLE_a, OO_SINGLE_cutoff };
    }
    return getMorsePair(e1, e2);
}

// ============================================================
// Парные дистанции образования/разрыва связей
// ============================================================
// Для большинства пар достаточно стандартных BOND_FORM_DIST/BOND_BREAK_DIST,
// но у Cl-Cl равновесная длина 1.99 Å БОЛЬШЕ стандартного порога 1.4 Å.
// Из-за этого связь Cl-Cl образуется на отталкивающей стенке Morse-
// потенциала (V > 0), и пара мгновенно разлетается, разрывая связь.
// Для Cl-Cl (и H-Cl на всякий случай) задаём свои, более широкие окна.
struct BondDists { float form; float brk; };

inline BondDists getBondDists(int e1, int e2) {
    int lo = std::min(e1, e2);
    int hi = std::max(e1, e2);
    if (lo == 2 && hi == 2) return { 2.4f, 3.5f };   // Cl-Cl
    if (lo == 0 && hi == 2) return { 1.7f, 2.9f };   // H-Cl
    if (lo == 2 && hi == 4) return { 2.6f, 3.8f };   // Na-Cl (ионная)
    return { BOND_FORM_DIST, BOND_BREAK_DIST };      // всё остальное — как было
}

// ============================================================
// Уровень 4 — «Молекулярный мост»
// ============================================================
const float L4_BOX_W = 60.0f;
const float L4_BOX_H = 36.0f;

const float L4_TEMP_C = -273.15f;   // абсолютный ноль
const float L4_GRAVITY_MAG = 0.01f;
const float L4_GRAVITY_DIR_DEG = 90.0f;      // вниз

const int   L4_START_H = 100;
const int   L4_START_O = 50;

const float L4_SPAWN_COOLDOWN = 0.2f;       // сек, минимум между спавнами
const float L4_BRIDGE_HOLD_TIME = 30.0f;      // сек, сколько должен продержаться мостик

// Колонны — квадраты. halfSize — половина стороны.
// Было 2.5 (радиус) → стало 3.75 (половина стороны): +50% к размеру.
const float L4_PILLAR_HALF_SIZE = 3.75f;
const float L4_PILLAR_X = 18.0f;          // |x| боковых колонн (центр — 0)
const float L4_PILLAR_TOUCH_DIST = 1.5f;  // доп. отступ касания колонны

const float L4_TASK_PANEL_H = 420.0f;     // высота панели задания L4

// ============================================================
// Уровень 6 — «Взрывные кольца» (Neutron Basketball)
// ============================================================
const float L6_BOX_W = 310.0f;
const float L6_BOX_H = 100.0f;

const float L6_TEMP_C = 30.0f;            // температура среды

const float L6_GRAVITY_MAG = 0.04f;         // фиксированная гравитация
const float L6_GRAVITY_DIR_DEG = 90.0f;

// Нейтроны падают в 3 раза слабее атомов — легче целиться по дальним кольцам.
const float L6_NEUTRON_GRAVITY_MUL = 1.0f / 3.0f;

const int   L6_URANIUM_PER_RING = 4;       // шаров урана на кольцо
const int   L6_RING_COUNT = 4;          // всего колец (= фаз)

const float L6_PILLAR_HALF_SIZE = 3.0f;  // половина стороны колонны-корзины

// Стартовая площадка — колонна игрока
const float L6_START_X = -22.0f;
const float L6_START_Y = 6.0f;

// Кольца (центр колонны). Интервалы между кольцами ПРОГРЕССИВНО
// растут: 26 → 34 → 43. Первое кольцо близко (легко попасть),
// последнее — самое сложное. Высоты чередуются для разнообразия
// траекторий.
const float L6_RING_X[4] = { 12.0f,  38.0f,  72.0f, 115.0f };
const float L6_RING_Y[4] = { -4.0f, -14.0f,  -8.0f, -20.0f };

// Уран на кольце: сдвигаем вверх над колонной, чтобы визуально
// «лежал на площадке».
const float L6_URANIUM_Y_OFFSET = 3.5f;   // над центром колонны
const float L6_URANIUM_SPACING = 0.95f;   // горизонтальный шаг между шарами

// Борта (неразрушимые)
const float L6_BARRIER_HALF_W = 0.4f;   // тонкие стенки
const float L6_BARRIER_HALF_H = 1.4f;   // по высоте

// Зона спавна игрока
const float L6_SPAWN_ZONE_RADIUS = 4.0f;

// Нейтроны
const float L6_NEUTRON_DRAG_SCALE = 0.0f;    // почти нет трения
const float L6_PILLAR_RESTITUTION = 0.4f;     // отскок от колонн
const float L6_SPAWN_SPEED_MUL = 1.2f;        // добросить до дальних колец
const float L6_NEUTRON_MIN_SPEED = 0.15f;     // ниже — нейтрон «умер»
const float L6_NEUTRON_TRAIL_MAX = 1.5f;      // сек — длина следа
const float L6_NEUTRON_TRAIL_STEP = 0.04f;    // сек — шаг между точками следа

// Пауза между фазовым переходом (сек) — короткая, чтобы игрок
// заметил «вспышку» и переезд камеры.
const float L6_PHASE_DELAY = 0.6f;

// Панель задачи
const float L6_TASK_PANEL_H = 360.0f;