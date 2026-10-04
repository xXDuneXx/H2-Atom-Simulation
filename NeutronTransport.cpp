// ============================================================
// NeutronTransport.cpp
// ============================================================
// Транспорт нейтронов методом Монте-Карло.
//
// Философия: нейтроны — отдельная подсистема. Они не входят в
// ATOM_TYPES, не имеют валентных связей, не влияют на H-связи.
// Единственное, что они делают — взаимодействуют с ядрами (в первую
// очередь с U-235), вызывая деление или радиационный захват.
//
// Масштаб величин:
//   - Расстояние: Å (совпадает с симуляционными единицами атомов)
//   - Скорость теплового нейтрона (0.0253 эВ): 5 у.е./с
//   - Ограничение скорости: 20 у.е./с (чтобы не «проскакивать» ядра)
// ============================================================

#include "NuclearData.hpp"
#include "NeutronTransport.hpp"
#include "Types.hpp"
#include "Config.hpp"

#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

// ============================================================
// SECTION 1. Утилиты
// ============================================================

float uniform01(std::mt19937& rng) {
    std::uniform_real_distribution<float> d(0.0f, 1.0f);
    return d(rng);
}

sf::Vector2f randomUnitVector2D(std::mt19937& rng) {
    float a = uniform01(rng) * 2.0f * 3.14159265f;
    return { std::cos(a), std::sin(a) };
}

// Скорость нейтрона в симуляционных единицах.
// Тепловой нейтрон (0.0253 эВ) — 5 у.е./с.
// Быстрые нейтроны ограничены 20 у.е./с.
inline float neutronSpeed(float E_eV) {
    const float THERMAL_EV = 0.0253f;
    const float THERMAL_SPEED = 5.0f;
    const float MAX_SPEED = 20.0f;
    if (E_eV <= 0.0f) return 0.0f;
    float v = THERMAL_SPEED * std::sqrt(E_eV / THERMAL_EV);
    if (v > MAX_SPEED) v = MAX_SPEED;
    return v;
}

// Сэмплирование Пуассона (алгоритм Кнута, для λ < 30)
inline int poissonSample(float lambda, std::mt19937& rng) {
    if (lambda <= 0.0f) return 0;
    float L = std::exp(-lambda);
    float p = 1.0f;
    int k = 0;
    do {
        ++k;
        p *= uniform01(rng);
    } while (p > L && k < 30);
    return k - 1;
}

// ============================================================
// SECTION 2. Сечения
// ============================================================

// Лог-линейная интерполяция по таблице U235_XS.
// type: 0=σ_f, 1=σ_c, 2=σ_s
inline float lookupSigma(float E_eV, int type) {
    const int N = sizeof(U235_XS) / sizeof(U235_XS[0]);
    auto pick = [type](const CrossSection& r) -> float {
        return (type == 0) ? r.sigma_f
            : (type == 1) ? r.sigma_c
            : r.sigma_s;
        };
    if (E_eV <= U235_XS[0].E_eV)   return pick(U235_XS[0]);
    if (E_eV >= U235_XS[N - 1].E_eV) return pick(U235_XS[N - 1]);

    int lo = 0;
    for (int i = 1; i < N; ++i) {
        if (U235_XS[i].E_eV > E_eV) { lo = i - 1; break; }
    }
    const auto& a = U235_XS[lo];
    const auto& b = U235_XS[lo + 1];
    float t = (std::log(E_eV) - std::log(a.E_eV))
        / (std::log(b.E_eV) - std::log(a.E_eV));
    float sa = pick(a), sb = pick(b);
    return sa + (sb - sa) * t;
}

inline float lookupSigmaFission(float, float E_eV) { return lookupSigma(E_eV, 0); }
inline float lookupSigmaCapture(float, float E_eV) { return lookupSigma(E_eV, 1); }
inline float lookupSigmaScatter(float, float E_eV) { return lookupSigma(E_eV, 2); }

// Резонансный вклад в σ_f по формуле Брейта-Вигнера.
// Для каждого резонанса:
//   σ_f(E) = π·λ̄²(E_r)·g_J·Γ_n·Γ_f / ((E−E_r)² + Γ²/4)
// где λ̄²(E_r) = ħ²/(2m·E_r) = 2.07·10⁵ барн·эВ / E_r.
inline float resonanceFissionSigma(float E_eV) {
    const float HBAR2_OVER_2M = 2.07e5f;   // барн·эВ
    const float PI = 3.14159265f;
    const int N = sizeof(U235_RESONANCES) / sizeof(U235_RESONANCES[0]);
    float sum = 0.0f;
    for (int i = 0; i < N; ++i) {
        const auto& r = U235_RESONANCES[i];
        if (r.E_eV <= 0.0f) continue;   // пропускаем связанный уровень
        float G = r.Gamma_n + r.Gamma_g + r.Gamma_f;
        if (G < 1e-10f) continue;
        float dE = E_eV - r.E_eV;
        float denom = dE * dE + 0.25f * G * G;
        float lambda2 = HBAR2_OVER_2M / r.E_eV;
        sum += PI * lambda2 * r.g_J * r.Gamma_n * r.Gamma_f / denom;
    }
    return sum;
}

inline float totalFissionSigma(float E_eV) {
    return lookupSigmaFission(0, E_eV) + resonanceFissionSigma(E_eV);
}

// Полное сечение взаимодействия для атома любого типа
inline float lookupSigmaTotal(int elementId, float E_eV) {
    if (elementId == 3) {   // U-235
        return totalFissionSigma(E_eV)
            + lookupSigmaCapture(0, E_eV)
            + lookupSigmaScatter(0, E_eV);
    }
    // H, O, Cl — грубые оценки (не зависят от E)
    if (elementId == 0) return 20.0f;   // H:  σ_s ≈ 20 барн
    if (elementId == 1) return  4.0f;   // O:  σ_s ≈ 4 барн
    if (elementId == 2) return 50.0f;   // Cl: σ_total ≈ 50 барн
    return 1.0f;
}

// Радиус «взаимодействия» в 2D. Масштабируется как √σ.
// R_0 подобран так, чтобы в типичной коробке 20×20 у.е.
// нейтрон за время жизни встречал несколько ядер.
inline float interactionRadius(int elementId, float E_eV) {
    const float R_0 = 5.0f;    // базовый радиус в Å
    const float SIGMA_REF = 700.0f;  // реперное сечение (барн)
    float sigma = lookupSigmaTotal(elementId, E_eV);
    return R_0 * std::sqrt(sigma / SIGMA_REF);
}

// ============================================================
// SECTION 3. Сэмплирование
// ============================================================

// Массовое число A лёгкого осколка по таблице U235_YIELDS.
// Тяжёлый = 236 − A.
inline int sampleYield(std::mt19937& rng) {
    const int N = sizeof(U235_YIELDS) / sizeof(U235_YIELDS[0]);
    float total = 0.0f;
    for (int i = 0; i < N; ++i) total += U235_YIELDS[i].yield_percent;
    float r = uniform01(rng) * total;
    float acc = 0.0f;
    for (int i = 0; i < N; ++i) {
        acc += U235_YIELDS[i].yield_percent;
        if (r <= acc) return U235_YIELDS[i].A;
    }
    return U235_YIELDS[N - 1].A;
}

// Спектр Уатта для промпт-нейтронов деления (аппроксимация).
// Средняя энергия ≈ 2 МэВ, максимум ≈ 0.7 МэВ, хвост до ~12 МэВ.
inline float sampleWattSpectrum(std::mt19937& rng) {
    float u1 = uniform01(rng);
    float u2 = uniform01(rng);
    if (u1 < 1e-10f) u1 = 1e-10f;
    if (u2 < 1e-10f) u2 = 1e-10f;
    // Сумма двух экспонент: одна с T=1.0 МэВ, другая с T=0.8 МэВ
    float E_MeV = -std::log(u1) - 0.8f * std::log(u2);
    if (E_MeV < 1e-4f) E_MeV = 1e-4f;
    if (E_MeV > 12.0f) E_MeV = 12.0f;
    return E_MeV * 1e6f;   // в эВ
}

// Множественность нейтронов. U-235 тепловой: ν ≈ 2.43.
inline int sampleNeutronMultiplicity(float /*E_eV*/, std::mt19937& rng) {
    return poissonSample(2.43f, rng);
}

// Группа запаздывающих нейтронов (6 групп, Keepin).
inline int sampleDelayedGroup(std::mt19937& rng) {
    const int N = 6;
    float totalBeta = 0.0f;
    for (int i = 0; i < N; ++i) totalBeta += DELAYED_GROUPS[i].beta_frac;
    float r = uniform01(rng) * totalBeta;
    float acc = 0.0f;
    for (int i = 0; i < N; ++i) {
        acc += DELAYED_GROUPS[i].beta_frac;
        if (r <= acc) return i;
    }
    return N - 1;
}

// ============================================================
// SECTION 4. Рассеяние и гамма-каскады
// ============================================================

// Упругое рассеяние нейтрона на ядре (точная кинематика).
// Формула для изотропного рассеяния в системе центра масс.
inline void scatterNeutron(Neutron& n, const Atom& target, std::mt19937& rng) {
    float A = std::max(1.0f, target.mass);   // в единицах массы нейтрона
    float alpha = ((A - 1.0f) / (A + 1.0f));
    alpha = alpha * alpha;

    // Новая энергия
    float E_new = n.energy_eV * (alpha + (1.0f - alpha) * uniform01(rng));
    if (E_new < 1e-4f) E_new = 1e-4f;

    // Угол отклонения в лабораторной системе
    float cosCom = 2.0f * uniform01(rng) - 1.0f;
    float denom = std::sqrt(1.0f + A * A + 2.0f * A * cosCom);
    float cosLab = (1.0f + A * cosCom) / denom;
    cosLab = std::clamp(cosLab, -1.0f, 1.0f);
    float deviation = std::acos(cosLab);

    float sign = (uniform01(rng) < 0.5f) ? -1.0f : 1.0f;
    float currentAngle = std::atan2(n.dir.y, n.dir.x);
    float newAngle = currentAngle + sign * deviation;
    n.dir = { std::cos(newAngle), std::sin(newAngle) };
    n.energy_eV = E_new;
}

// Каскад гамма-квантов (3–5 фотонов, суммарно ≈ 7 МэВ).
inline void spawnGammaCascade(const sf::Vector2f& pos,
    std::vector<Gamma>& gammas,
    std::mt19937& rng) {
    int count = 3 + (int)(uniform01(rng) * 3.0f);   // 3..5
    const float totalE = 7.0f;   // МэВ
    for (int i = 0; i < count; ++i) {
        Gamma g;
        g.pos = pos;
        g.dir = randomUnitVector2D(rng);
        g.energy_MeV = totalE / (float)count * (0.7f + 0.6f * uniform01(rng));
        g.age = 0.0f;
        g.alive = true;
        gammas.push_back(g);
    }
}

// ============================================================
// SECTION 5. Деление
// ============================================================

// Индексы атомов, помеченных к удалению (elementId = -1).
// После stepNeutrons вызывается compactAtomsAfterFission().
void compactAtomsAfterFission(std::vector<Atom>& atoms) {
    bool anyDead = false;
    for (const auto& a : atoms) if (a.elementId < 0) { anyDead = true; break; }
    if (!anyDead) return;

    // Ремап old→new
    std::vector<int> remap(atoms.size(), -1);
    int newIdx = 0;
    for (size_t k = 0; k < atoms.size(); ++k) {
        if (atoms[k].elementId >= 0) remap[k] = newIdx++;
    }
    // Обновляем ссылки
    for (auto& a : atoms) {
        if (a.elementId < 0) continue;
        for (auto& b : a.bonds)
            if (b >= 0) b = (b < (int)remap.size()) ? remap[b] : -1;
        for (auto& b : a.hbonds)
            if (b >= 0) b = (b < (int)remap.size()) ? remap[b] : -1;
    }
    // Удаляем
    atoms.erase(
        std::remove_if(atoms.begin(), atoms.end(),
            [](const Atom& a) { return a.elementId < 0; }),
        atoms.end());
}

// Обработка деления U-235. Возвращает через newNeutrons новые
// нейтроны, через delayedPool — запаздывающие, через gammas — гамма.
// Помечает U-235 как удалённый (elementId = -1).
void handleFission(int hitIdx, const Neutron& n,
    std::vector<Atom>& atoms,
    std::vector<Neutron>& newNeutrons,
    std::vector<Gamma>& gammas,
    std::vector<Neutron>& delayedPool,
    std::mt19937& rng)
{
    // Запоминаем позицию заранее — vector может реаллоцироваться
    const sf::Vector2f uPos = atoms[hitIdx].pos;

    // 1) Массы осколков
    int A1 = sampleYield(rng);
    int A2 = 236 - A1;

    // 2) Кинематика осколков
    const float KE_FRAG_MeV = 169.0f;
    const float SPEED_SCALE = 0.5f;
    float E1 = KE_FRAG_MeV * (A2 / (float)(A1 + A2));
    float E2 = KE_FRAG_MeV * (A1 / (float)(A1 + A2));
    float v1 = SPEED_SCALE * std::sqrt(E1 / (float)A1);
    float v2 = SPEED_SCALE * std::sqrt(E2 / (float)A2);

    float baseAngle = uniform01(rng) * 2.0f * 3.14159265f;
    sf::Vector2f dir1 = { std::cos(baseAngle), std::sin(baseAngle) };
    sf::Vector2f dir2 = { -dir1.x, -dir1.y };

    // 3) Создаём два осколка.
    //    Типичное деление U-235: лёгкий осколок — Kr (Z=36), тяжёлый — Ba (Z=56).
    const int KR_ID = 6;
    const int BA_ID = 5;

    int A_kr, A_ba;
    sf::Vector2f pos_kr, pos_ba, vel_kr, vel_ba;
    if (A1 < A2) {
        A_kr = A1;   A_ba = A2;
        pos_kr = uPos + dir1 * 0.3f;   vel_kr = dir1 * v1;
        pos_ba = uPos + dir2 * 0.3f;   vel_ba = dir2 * v2;
    }
    else {
        A_kr = A2;   A_ba = A1;
        pos_kr = uPos + dir2 * 0.3f;   vel_kr = dir2 * v2;
        pos_ba = uPos + dir1 * 0.3f;   vel_ba = dir1 * v1;
    }

    // Криптон: лёгкий осколок. makeAtom() сама генерирует ядро (36 P + 58 n,
    // масштабируется до MAX_NUCLEONS_DISPLAY).
    Atom kr = makeAtom(KR_ID, pos_kr, vel_kr);
    kr.mass = (float)A_kr;
    kr.radius = 0.45f + 0.025f * std::sqrt((float)A_kr);
    atoms.push_back(kr);

    // Барий: тяжёлый осколок. 56 P + 82 n.
    Atom ba = makeAtom(BA_ID, pos_ba, vel_ba);
    ba.mass = (float)A_ba;
    ba.radius = 0.45f + 0.025f * std::sqrt((float)A_ba);
    atoms.push_back(ba);

    // 4) Промпт-нейтроны (спектр Уатта)
    int np = sampleNeutronMultiplicity(n.energy_eV, rng);
    for (int k = 0; k < np; ++k) {
        Neutron nn;
        nn.pos = uPos;
        nn.dir = randomUnitVector2D(rng);
        nn.energy_eV = sampleWattSpectrum(rng);
        nn.age = 0.0f;
        nn.alive = true;
        nn.parentU = -1;
        nn.delayed = false;
        newNeutrons.push_back(nn);
    }

    // 5) Запаздывающие нейтроны (β_total ≈ 0.65%)
    if (uniform01(rng) < 0.0065f) {
        int g = sampleDelayedGroup(rng);
        float lambda = std::max(DELAYED_GROUPS[g].lambda, 1e-3f);
        float u01 = uniform01(rng);
        if (u01 < 1e-6f) u01 = 1e-6f;
        float t_emit = -std::log(u01) / lambda;

        Neutron dn;
        dn.pos = uPos;
        dn.dir = { 0.0f, 0.0f };
        dn.energy_eV = 0.5e6f;    // ~0.5 МэВ для запаздывающего
        dn.age = t_emit;    // здесь age = время до испускания
        dn.alive = true;
        dn.parentU = g;         // индекс группы
        dn.delayed = true;
        delayedPool.push_back(dn);
    }

    // 6) Гамма-каскад (~7 МэВ)
    spawnGammaCascade(uPos, gammas, rng);

    // 7) Помечаем U-235 как удалённый
    atoms[hitIdx].elementId = -1;
}

// ============================================================
// SECTION 6. Запаздывающие нейтроны
// ============================================================

void updateDelayedNeutrons(std::vector<Neutron>& activeNeutrons,
    std::vector<Neutron>& delayedPool,
    float dt,
    std::mt19937& rng)
{
    for (auto& dn : delayedPool) {
        if (!dn.alive) continue;
        dn.age -= dt;
        if (dn.age <= 0.0f) {
            // Испускаем как активный нейтрон
            Neutron n;
            n.pos = dn.pos;
            n.dir = randomUnitVector2D(rng);
            n.energy_eV = 0.5e6f;
            n.age = 0.0f;
            n.alive = true;
            n.parentU = -1;
            n.delayed = false;
            activeNeutrons.push_back(n);
            dn.alive = false;
        }
    }
    delayedPool.erase(
        std::remove_if(delayedPool.begin(), delayedPool.end(),
            [](const Neutron& n) { return !n.alive; }),
        delayedPool.end());
}

// ============================================================
// SECTION 7. Основной шаг транспорта
// ============================================================

void stepNeutrons(
    std::vector<Neutron>& neutrons,
    std::vector<Gamma>& gammas,
    std::vector<Atom>& atoms,
    std::vector<Neutron>& delayedPool,
    const SpatialGrid& grid,
    sf::Vector2f boxSize,
    float dt,
    std::mt19937& rng)
{
    const float halfX = boxSize.x * 0.5f;
    const float halfY = boxSize.y * 0.5f;

    std::vector<Neutron> newNeutrons;
    newNeutrons.reserve(8);

    for (size_t ni = 0; ni < neutrons.size(); ++ni) {
        Neutron& n = neutrons[ni];
        if (!n.alive) continue;

        float speed = neutronSpeed(n.energy_eV);
        float move = speed * dt;

        // Разбиваем движение на подшаги, чтобы нейтрон не «проскакивал»
        // мимо ядер с малым радиусом взаимодействия.
        const float MAX_CHUNK = 0.3f;   // Å на подшаг
        int nChunks = std::max(1, (int)std::ceil(move / MAX_CHUNK));
        nChunks = std::min(nChunks, 200);
        float chunkSize = move / (float)nChunks;

        for (int c = 0; c < nChunks && n.alive; ++c) {
            n.pos += n.dir * chunkSize;

            // Границы коробки
            if (std::abs(n.pos.x) > halfX || std::abs(n.pos.y) > halfY) {
                n.alive = false;
                break;
            }

            // Ищем ядра в 3×3
            int cx = grid.cellX(n.pos.x);
            int cy = grid.cellY(n.pos.y);

            for (int dy = -1; dy <= 1 && n.alive; ++dy) {
                for (int dx = -1; dx <= 1 && n.alive; ++dx) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || nx >= grid.cols() ||
                        ny < 0 || ny >= grid.rows()) continue;

                    for (int idx : grid.at(nx, ny)) {
                        if (idx < 0 || idx >= (int)atoms.size()) continue;
                        const Atom& a = atoms[idx];
                        if (a.elementId < 0) continue;

                        // Осколки деления не участвуют в нейтронном транспорте.
                        // Реальные U-235 имеют elementId == 3; осколки — elementId 1 или 2
                        // с большой массой (A > 60). Пропускаем их.
                        if (a.elementId != 3 && a.mass > 60.0f) continue;

                        float ddx = a.pos.x - n.pos.x;
                        float ddy = a.pos.y - n.pos.y;
                        float r2 = ddx * ddx + ddy * ddy;

                        float R_int = interactionRadius(a.elementId, n.energy_eV);
                        if (r2 > R_int * R_int) continue;

                        // === ВЗАИМОДЕЙСТВИЕ ===
                        // Сечения
                        float sigma_f = (a.elementId == 3)
                            ? totalFissionSigma(n.energy_eV) : 0.0f;
                        float sigma_c = (a.elementId == 3)
                            ? lookupSigmaCapture(0, n.energy_eV) : 0.0f;
                        float sigma_s;
                        if (a.elementId == 3)
                            sigma_s = lookupSigmaScatter(0, n.energy_eV);
                        else if (a.elementId == 0) sigma_s = 20.0f;
                        else if (a.elementId == 1) sigma_s = 4.0f;
                        else if (a.elementId == 2) sigma_s = 15.0f;
                        else                       sigma_s = 5.0f;

                        float sigma_t = sigma_f + sigma_c + sigma_s;
                        if (sigma_t <= 1e-10f) continue;

                        float r = uniform01(rng) * sigma_t;
                        if (r < sigma_f) {
                            // Деление
                            handleFission(idx, n, atoms, newNeutrons,
                                gammas, delayedPool, rng);
                            n.alive = false;
                        }
                        else if (r < sigma_f + sigma_c) {
                            // Радиационный захват
                            spawnGammaCascade(a.pos, gammas, rng);
                            n.alive = false;
                        }
                        else {
                            // Упругое рассеяние
                            scatterNeutron(n, a, rng);
                            // Продолжаем движение в новом направлении
                        }
                        break;   // одно взаимодействие на chunk
                    }
                }
            }
        }

        if (n.alive) n.age += dt;
    }

    // Добавляем новые нейтроны от делений
    for (auto& nn : newNeutrons) neutrons.push_back(nn);

    // Чистим мёртвые нейтроны
    neutrons.erase(
        std::remove_if(neutrons.begin(), neutrons.end(),
            [](const Neutron& n) { return !n.alive; }),
        neutrons.end());

    // Чистим помеченные к удалению U-235
    compactAtomsAfterFission(atoms);
}

// ============================================================
// SECTION 8. Гамма-кванты (упрощённый транспорт)
// ============================================================

void stepGammas(std::vector<Gamma>& gammas,
    sf::Vector2f boxSize,
    float dt)
{
    const float halfX = boxSize.x * 0.5f;
    const float halfY = boxSize.y * 0.5f;
    const float GAMMA_SPEED = 60.0f;   // у.е./с, быстрее нейтронов

    for (auto& g : gammas) {
        if (!g.alive) continue;
        g.pos += g.dir * GAMMA_SPEED * dt;
        g.age += dt;
        if (std::abs(g.pos.x) > halfX || std::abs(g.pos.y) > halfY) {
            g.alive = false;
        }
    }
    gammas.erase(
        std::remove_if(gammas.begin(), gammas.end(),
            [](const Gamma& g) { return !g.alive; }),
        gammas.end());
}