#pragma once
#include <cmath>
#include <algorithm>
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <array>
#include <random>          // ← новое
#include "Config.hpp"

struct AtomType {
    std::string name;
    std::string symbol;
    float mass;
    float radius;
    sf::Color color;
    int   maxBonds;
    int   Z;    // число протонов
    int   N;    // число нейтронов
};

// ============================================================
// Нуклон — протон или нейтрон внутри ядра
// ============================================================
struct Nucleon {
    sf::Vector2f relPos{};
    int type = 0;
};

struct Neutron {
    sf::Vector2f pos{};
    sf::Vector2f dir{};
    float energy_eV = 0.0f;
    float age = 0.0f;
    bool  alive = false;
    int   parentU = -1;
    bool  delayed = false;

    // L6: визуальный след. На L1–L5 вектор остаётся пустым.
    std::vector<sf::Vector2f> trail;
    float trailTimer = 0.0f;
};

struct Gamma {
    sf::Vector2f pos{};
    sf::Vector2f dir{};
    float energy_MeV = 0.0f;
    float age = 0.0f;
    bool  alive = false;
};

struct Wall {
    sf::Vector2f a, b;
    bool selected = false;
};

// ============================================================
// Неподвижная колонна (уровень 4 «Молекулярный мост»)
// ============================================================
struct Pillar {
    sf::Vector2f pos;
    float halfSize = L4_PILLAR_HALF_SIZE;   // половина стороны квадрата
};

// ============================================================
// Уровень 6: неразрушимый борт (боковая стенка корзины)
// ============================================================
struct Barrier {
    sf::Vector2f pos;       // центр
    sf::Vector2f halfSize;  // половина ширины/высоты AABB
};

// ============================================================
// Уровень 6: зона спавна игрока
// ============================================================
struct SpawnZone {
    sf::Vector2f pos{ 0.0f, 0.0f };
    float radius = L6_SPAWN_ZONE_RADIUS;
    bool  active = false;   // false = зоны нет (например, между фазами)
};

struct Atom {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float mass = 0.0f;
    float radius = 0.0f;
    sf::Color color;
    int   elementId = 0;
    std::array<int, 4> bonds = { -1, -1, -1, -1 };
    std::array<int, 4> hbonds = { -1, -1, -1, -1 };
    std::vector<Nucleon> nucleons;   // НОВОЕ: ядро
    bool  selected = false;
    float age = 0.0f;
};

struct Slider {
    float value = 1.0f;
    bool isDragging = false;
};

inline const std::vector<AtomType> ATOM_TYPES = {
    { "Hydrogen",   "H",  H_MASS,       0.35f, sf::Color(240, 240, 240), 1, 1,  0   },
    { "Oxygen",     "O",  O_MASS,       0.45f, sf::Color(230, 80,  80),  3, 8,  8   },
    { "Chlorine",   "Cl", CL_MASS,      0.55f, sf::Color(110, 200, 90),  1, 17, 18  },
    { "Uranium-235","U",  U235_MASS,    0.85f, sf::Color(90, 130, 90),   0, 92, 143 },
    { "Sodium",     "Na", NA_MASS,      0.60f, sf::Color(200, 180, 240), 1, 11, 12  },
    { "Barium",     "Ba", BA_MASS,      0.70f, sf::Color(210, 180, 120), 0, 56, 82  },
    { "Krypton",    "Kr", KR_MASS,      0.60f, sf::Color(130, 220, 180), 0, 36, 58  },
    { "Neutron",    "n",  NEUTRON_MASS, NUCLEON_RADIUS, sf::Color(240, 240, 245), 0, 0, 1 },
};

// ============================================================
// Генерация нуклонов — спираль Вогеля (phyllotaxis)
// ============================================================
// Позиции нуклонов раскидываются по спирали Вогеля:
//   angle_i = i * goldenAngle (~137.5°)
//   r_i     = R * sqrt((i + 0.5) / total)
// Такое распределение равномерно по площади диска — без сгустков
// и без пустот. Радиус R подобран так, чтобы при гексагональной
// упаковке нуклоны касались (78% площади): буквы P/n не наезжают
// друг на друга, потому что сами нуклоны не перекрываются.
//
// Тип нуклона (P/N) раздаётся независимо от позиции: строим пул
// из Z протонов и N нейтронов, перемешиваем и раскладываем по
// позициям. Так P и N равномерно перемешаны, без корреляции с
// радиусом и без «сгруппированных» по типу областей.
//
// Seed детерминированный (по Z и N), поэтому один и тот же изотоп
// всегда выглядит одинаково.
// ============================================================
inline std::vector<Nucleon> generateNucleons(int Z, int N, float nucleonR,
    float sizeScale = NUCLEUS_SIZE_SCALE) {
    int total = Z + N;
    if (total <= 0) return {};

    // Ограничиваем тяжёлые ядра
    if (total > MAX_NUCLEONS_DISPLAY) {
        float s = (float)MAX_NUCLEONS_DISPLAY / (float)total;
        int newZ = std::max(1, (int)std::round(Z * s));
        int newN = std::max(0, MAX_NUCLEONS_DISPLAY - newZ);
        Z = newZ;
        N = newN;
        total = Z + N;
    }

    const float rn = nucleonR * sizeScale;

    std::vector<Nucleon> result;
    result.reserve(total);

    // Детерминированный сид: одинаковый изотоп → одинаковая картинка
    unsigned seed = (unsigned)Z * 73856093u ^ (unsigned)N * 19349663u ^ 0x9E3779B9u;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> uni(-1.0f, 1.0f);

    // ------------------------------------------------------------
    // Радиус ядра: гексагональная упаковка кружков радиуса rn.
    // R = rn * sqrt(total / 0.78) — ровно тот радиус, при котором
    // N кружков касаются друг друга (78% площади).
    // ------------------------------------------------------------
    float R = rn * std::sqrt((float)total / 0.78f / 2.0f);
    if (R < rn) R = rn;

    // ------------------------------------------------------------
    // Позиции: спираль Вогеля
    // ------------------------------------------------------------
    const float GOLDEN_ANGLE = 2.39996323f;   // ~137.5° в радианах

    std::vector<sf::Vector2f> positions;
    positions.reserve(total);
    for (int i = 0; i < total; ++i) {
        float angle = i * GOLDEN_ANGLE;
        float r = R * std::sqrt((i + 0.5f) / (float)total);
        // Небольшой джиттер, чтобы ядро не выглядело слишком
        // «математически идеальным».
        float jitter = rn * 0.10f;
        positions.push_back({
            std::cos(angle) * r + uni(rng) * jitter,
            std::sin(angle) * r + uni(rng) * jitter
            });
    }

    // Разворачиваем: спираль Вогеля изначально идёт из центра наружу,
    // но нам нужно, чтобы наружные нуклоны рисовались раньше
    // внутренних (тогда внутренние окажутся «поверх» — без эффекта
    // лестницы). Разворот массива меняет только порядок отрисовки,
    // на геометрию и на раздачу типов P/N не влияет.
    std::reverse(positions.begin(), positions.end());

    // ------------------------------------------------------------
    // Типы P/N: случайная раздача из пула Z протонов + N нейтронов
    // ------------------------------------------------------------
    std::vector<int> types;
    types.reserve(total);
    for (int i = 0; i < Z; ++i) types.push_back(0);   // протоны
    for (int i = 0; i < N; ++i) types.push_back(1);   // нейтроны
    std::shuffle(types.begin(), types.end(), rng);

    for (int i = 0; i < total; ++i) {
        Nucleon nuc;
        nuc.relPos = positions[i];
        nuc.type = types[i];
        result.push_back(nuc);
    }
    return result;
}

// ============================================================
// Единая фабрика атома с готовым ядром
// ============================================================
inline Atom makeAtom(int elementId, sf::Vector2f pos, sf::Vector2f vel) {
    const AtomType& t = ATOM_TYPES[elementId];
    Atom a;
    a.pos = pos;
    a.vel = vel;
    a.elementId = elementId;
    a.mass = t.mass;
    a.radius = t.radius;
    a.color = t.color;
    a.bonds = { -1, -1, -1, -1 };
    a.hbonds = { -1, -1, -1, -1 };
    a.selected = false;
    a.age = 0.0f;
    a.nucleons = generateNucleons(t.Z, t.N, NUCLEON_RADIUS, NUCLEUS_SIZE_SCALE);
    return a;
}

// ------------------------------------------------------------
// Хелперы для работы со связями (без изменений)
// ------------------------------------------------------------
inline int countBonds(const Atom& a) {
    int n = 0;
    for (int k : a.bonds) if (k >= 0) n++;
    return n;
}
inline bool isBondedTo(const Atom& a, int j) {
    for (int k : a.bonds) if (k == j) return true;
    return false;
}
inline void addBond(Atom& a, int j) {
    for (auto& k : a.bonds) if (k < 0) { k = j; return; }
}
inline void removeBond(Atom& a, int j) {
    for (auto& k : a.bonds) if (k == j) { k = -1; return; }
}
inline void removeAllBondsTo(Atom& a, int j) {
    for (auto& k : a.bonds) if (k == j) k = -1;
}
inline bool hasFreeBond(const Atom& a) {
    int maxB = ATOM_TYPES[a.elementId].maxBonds;
    return countBonds(a) < maxB;
}
inline int countHBonds(const Atom& a) {
    int n = 0;
    for (int k : a.hbonds) if (k >= 0) n++;
    return n;
}
inline bool isHBondedTo(const Atom& a, int j) {
    for (int k : a.hbonds) if (k == j) return true;
    return false;
}
inline void addHBond(Atom& a, int j) {
    for (auto& k : a.hbonds) if (k < 0) { k = j; return; }
}
inline void removeAllHBondsTo(Atom& a, int j) {
    for (auto& k : a.hbonds) if (k == j) k = -1;
}

// ------------------------------------------------------------
// SpatialGrid — без изменений
// ------------------------------------------------------------
class SpatialGrid {
public:
    void init(float worldMin, float worldSize, float cellSize) {
        m_cellSize = cellSize;
        m_min = worldMin;
        m_cols = std::max(1, (int)std::ceil(worldSize / cellSize));
        m_rows = m_cols;
        m_cells.assign(m_cols * m_rows, {});
    }
    void clear() { for (auto& c : m_cells) c.clear(); }
    void insert(int idx, const sf::Vector2f& pos) {
        int cx = std::clamp((int)((pos.x - m_min) / m_cellSize), 0, m_cols - 1);
        int cy = std::clamp((int)((pos.y - m_min) / m_cellSize), 0, m_rows - 1);
        m_cells[cy * m_cols + cx].push_back(idx);
    }
    int cellX(float x) const { return std::clamp((int)((x - m_min) / m_cellSize), 0, m_cols - 1); }
    int cellY(float y) const { return std::clamp((int)((y - m_min) / m_cellSize), 0, m_rows - 1); }
    const std::vector<int>& at(int cx, int cy) const { return m_cells[cy * m_cols + cx]; }
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
private:
    float m_cellSize = 1.0f;
    float m_min = 0.0f;
    int   m_cols = 1;
    int   m_rows = 1;
    std::vector<std::vector<int>> m_cells;
};

// ------------------------------------------------------------
// TIP4P, tetrahedral order, atomOutlineColor — без изменений
// ------------------------------------------------------------
inline bool computeTip4pSite(const std::vector<Atom>& atoms, int oIdx,
    sf::Vector2f& mPos) {
    if (oIdx < 0 || oIdx >= (int)atoms.size()) return false;
    const Atom& o = atoms[oIdx];
    if (o.elementId != 1) return false;

    int h1 = -1, h2 = -1;
    for (int k : o.bonds) {
        if (k < 0 || k >= (int)atoms.size()) continue;
        if (atoms[k].elementId != 0) continue;
        if (h1 < 0) h1 = k;
        else if (h2 < 0) { h2 = k; break; }
    }
    if (h1 < 0 || h2 < 0) return false;

    sf::Vector2f d1 = atoms[h1].pos - o.pos;
    sf::Vector2f d2 = atoms[h2].pos - o.pos;
    float L1 = std::sqrt(d1.x * d1.x + d1.y * d1.y);
    float L2 = std::sqrt(d2.x * d2.x + d2.y * d2.y);
    if (L1 < 1e-6f || L2 < 1e-6f) return false;

    sf::Vector2f bis = d1 / L1 + d2 / L2;
    float bl = std::sqrt(bis.x * bis.x + bis.y * bis.y);
    if (bl < 1e-6f) return false;

    mPos.x = o.pos.x - bis.x / bl * TIP4P_OM_DIST;
    mPos.y = o.pos.y - bis.y / bl * TIP4P_OM_DIST;
    return true;
}

inline float computeTetrahedralOrder(const std::vector<Atom>& atoms,
    int oIdx, int& neighborCount) {
    neighborCount = 0;
    if (oIdx < 0 || oIdx >= (int)atoms.size()) return 0.0f;
    const Atom& o = atoms[oIdx];
    if (o.elementId != 1) return 0.0f;

    std::vector<sf::Vector2f> dirs;
    for (int k : o.bonds) {
        if (k < 0 || k >= (int)atoms.size()) continue;
        sf::Vector2f d = atoms[k].pos - o.pos;
        float L = std::sqrt(d.x * d.x + d.y * d.y);
        if (L > 1e-6f) dirs.push_back(d / L);
    }
    for (int k : o.hbonds) {
        if (k < 0 || k >= (int)atoms.size()) continue;
        sf::Vector2f d = atoms[k].pos - o.pos;
        float L = std::sqrt(d.x * d.x + d.y * d.y);
        if (L > 1e-6f) dirs.push_back(d / L);
    }
    neighborCount = (int)dirs.size();
    if (neighborCount < 4) return 0.0f;
    if (dirs.size() > 4) dirs.resize(4);

    float sum = 0.0f;
    for (size_t i = 0; i < dirs.size(); ++i) {
        for (size_t j = i + 1; j < dirs.size(); ++j) {
            float c = std::clamp(dirs[i].x * dirs[j].x + dirs[i].y * dirs[j].y,
                -1.0f, 1.0f);
            float t = c + 1.0f / 3.0f;
            sum += t * t;
        }
    }
    return 1.0f - (3.0f / 8.0f) * sum;
}

inline sf::Color atomOutlineColor(int elementId) {
    if (elementId == 1) return sf::Color(196, 68, 68);
    if (elementId == 2) return sf::Color(94, 170, 77);
    if (elementId == 3) return sf::Color(80, 110, 80);
    if (elementId == 4) return sf::Color(160, 144, 192);
    if (elementId == 5) return sf::Color(168, 144, 96);   // Ba
    if (elementId == 6) return sf::Color(104, 176, 144);  // Kr
    return sf::Color(204, 204, 204);
}

// ------------------------------------------------------------
// Отображаемые заряды (для тумблера Charges)
// ------------------------------------------------------------
// Показываем упрощённые ионные заряды. Натрий и хлор — реальные
// ионы (+1 / -1); H и O — полярные атомы в воде с частичными
// зарядами, показываем знак как «+» / «-».
inline std::string atomChargeLabel(int elementId) {
    if (elementId == 0) return "+";   // H (частично +)
    if (elementId == 1) return "-";   // O (частично -)
    if (elementId == 2) return "-";   // Cl-
    if (elementId == 4) return "+";   // Na+
    return "";                        // U, n — без заряда
}

inline sf::Color atomChargeColor(int elementId) {
    if (elementId == 0) return sf::Color(255, 200, 200);  // H+
    if (elementId == 1) return sf::Color(255, 130, 130);  // O-
    if (elementId == 2) return sf::Color(150, 230, 150);  // Cl-
    if (elementId == 4) return sf::Color(160, 180, 255);  // Na+
    return sf::Color::White;
}