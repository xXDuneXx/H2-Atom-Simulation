#include "Physics.hpp"
#include <cmath>
#include <random>   // ← добавить в начало Physics.cpp
#include <cstdio>
#include <algorithm>

// ---------- вспомогательные ----------

sf::Vector2f rotateVec(const sf::Vector2f& v, float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    return { v.x * c - v.y * s, v.x * s + v.y * c };
}

float valueToPos(float value) {
    if (value <= 0.0f) return 0.0f;
    return std::log(value / SPEED_MIN) / std::log(SPEED_MAX / SPEED_MIN);
}

float boxSizeToPos(float size) {
    return std::clamp((size - BOX_MIN) / (BOX_MAX - BOX_MIN), 0.0f, 1.0f);
}

float boxSizePosToSize(float p) {
    p = std::clamp(p, 0.0f, 1.0f);
    return BOX_MIN + p * (BOX_MAX - BOX_MIN);
}

float posToValue(float pos) {
    pos = std::clamp(pos, 0.0f, 1.0f);
    return SPEED_MIN * std::pow(SPEED_MAX / SPEED_MIN, pos);
}

std::string formatSpeed(float v) {
    if (v <= 0.0f) return "0x";
    char buf[32];
    if (v < 1.0f)       std::snprintf(buf, sizeof(buf), "%.2fx", v);
    else if (v < 10.0f) std::snprintf(buf, sizeof(buf), "%.1fx", v);
    else                std::snprintf(buf, sizeof(buf), "%.0fx", v);
    return buf;
}

float tempCelsiusToPhysics(float c) {
    // Трёхзонная карта:
    //   c <= 0°C        : линейно от 0 (при -273.15°C) до ICE_BASE (при 0°C)
    //   0°C .. MELT_C   : быстрый рост — зона плавления льда
    //   MELT_C .. 110°C : плавный рост — жидкость/пар
    const float ICE_BASE = 0.07f;   // physTemp при 0°C   → лёд прочный
    const float MELT_C = 15.0f;   // конец зоны плавления, °C
    const float MELT_TOP = 0.24f;   // physTemp при MELT_C → лёд расплавлен

    if (c <= 0.0f) {
        float s = (c - TEMP_MIN_C) / (0.0f - TEMP_MIN_C); // [-273..0] → [0..1]
        if (s < 0.0f) s = 0.0f;
        return s * ICE_BASE;
    }
    if (c <= MELT_C) {
        float s = c / MELT_C;                             // [0..MELT_C] → [0..1]
        return ICE_BASE + s * (MELT_TOP - ICE_BASE);
    }
    float s = (c - MELT_C) / (TEMP_MAX_C - MELT_C);       // [MELT_C..110] → [0..1]
    if (s > 1.0f) s = 1.0f;
    return MELT_TOP + s * (TEMP_PHYS_MAX - MELT_TOP);
}

float tempCelsiusToSliderPos(float c) {
    return std::clamp((c - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C), 0.0f, 1.0f);
}

float tempSliderPosToCelsius(float p) {
    p = std::clamp(p, 0.0f, 1.0f);
    return TEMP_MIN_C + p * (TEMP_MAX_C - TEMP_MIN_C);
}

std::string formatTempCelsius(float c) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", c);
    return buf;
}

// ---------- взаимодействие ----------

void computeInteraction(const std::vector<Atom>& atoms, int i, int j,
    float& fx, float& fy)
{
    const Atom& a1 = atoms[i];
    const Atom& a2 = atoms[j];

    float dx = a2.pos.x - a1.pos.x;   // a1 → a2
    float dy = a2.pos.y - a1.pos.y;
    float r2 = dx * dx + dy * dy;
    float r = std::sqrt(r2);
    if (r < 1e-6f) { fx = 0.0f; fy = 0.0f; return; }

    const int max1 = ATOM_TYPES[a1.elementId].maxBonds;
    const int max2 = ATOM_TYPES[a2.elementId].maxBonds;
    const int n1 = countBonds(a1);
    const int n2 = countBonds(a2);
    const bool sat1 = (n1 >= max1);
    const bool sat2 = (n2 >= max2);

    const bool mutuallyBonded = isBondedTo(a1, j) && isBondedTo(a2, i);
    const bool mutuallyHBonded = isHBondedTo(a1, j) && isHBondedTo(a2, i);

    float F_total = 0.0f;

    // --- Жёсткое ядро ---
    // Для пары H–O ядро специально ослаблено. Иначе при сближении
    // двух HO· (сборка H2O2) чужой кислород отбрасывает H сильнее,
    // чем O–O-связь может удержать, и «пероксидный» комплекс
    // мгновенно разваливается: H улетает, O–O рвётся.
    // Для всех остальных пар — как раньше (HARD_CORE_K).
    float hcK = HARD_CORE_K;
    if ((a1.elementId == 0 && a2.elementId == 1) ||
        (a1.elementId == 1 && a2.elementId == 0)) {
        hcK = 4.0f;
    }
    float minDist = a1.radius + a2.radius;
    if (r < minDist) F_total += hcK * (minDist - r);

    if (mutuallyBonded) {
        // Ковалентная связь.
        // Считаем порядок: сколько раз j записан в слотах i.
        // Для O-O: order=1 → одинарная (H2O2), order=2 → двойная (O2).
        int order = 0;
        for (int k : a1.bonds) if (k == j) order++;
        const MorsePair mp = getMorsePairByOrder(a1.elementId, a2.elementId, order);
        float delta = r - mp.re;
        float expTerm = std::exp(-mp.a * delta);
        float F = -2.0f * mp.De * mp.a * (1.0f - expTerm) * expTerm;
        if (r > mp.cutoff) F = 0.0f;
        else if (r > mp.cutoff - 0.5f) F *= (mp.cutoff - r) / 0.5f;
        F_total += F;
    }
    else if (mutuallyHBonded) {
        // H-связь. Донор — H, акцептор — O (через M-сайт) или Cl (напрямую).
        int hIdx = (a1.elementId == 0) ? i : j;
        int accIdx = (hIdx == i) ? j : i;
        int accElem = atoms[accIdx].elementId;

        if (accElem == 1) {
            // === Кислород-акцептор (TIP4P, через M-сайт) ===
            sf::Vector2f mPos;
            if (!computeTip4pSite(atoms, accIdx, mPos)) { fx = 0.0f; fy = 0.0f; return; }

            float rMx = mPos.x - atoms[hIdx].pos.x;
            float rMy = mPos.y - atoms[hIdx].pos.y;
            float rM = std::sqrt(rMx * rMx + rMy * rMy);
            if (rM < 1e-6f) { fx = 0.0f; fy = 0.0f; return; }

            int oDonor = -1;
            for (int k : atoms[hIdx].bonds) if (k >= 0) { oDonor = k; break; }

            float angleFactor = 1.0f;
            if (oDonor >= 0 && oDonor < (int)atoms.size()) {
                sf::Vector2f u = atoms[hIdx].pos - atoms[oDonor].pos;
                float uLen = std::sqrt(u.x * u.x + u.y * u.y);
                if (uLen > 1e-6f) {
                    float cosT = (u.x * rMx + u.y * rMy) / (uLen * rM);
                    cosT = std::clamp(cosT, -1.0f, 1.0f);
                    angleFactor = 0.5f * (1.0f + cosT);
                }
            }

            float delta = rM - HBOND_re;
            float expTerm = std::exp(-HBOND_a * delta);
            float F = -2.0f * HBOND_De * HBOND_a * (1.0f - expTerm) * expTerm;
            if (rM > HBOND_CUTOFF) F = 0.0f;
            else if (rM > HBOND_CUTOFF - 0.5f) F *= (HBOND_CUTOFF - rM) / 0.5f;
            F *= angleFactor;

            float FH_x = -F * rMx / rM;
            float FH_y = -F * rMy / rM;
            float FO_x = F * rMx / rM;
            float FO_y = F * rMy / rM;

            float minDist = a1.radius + a2.radius;
            if (r < minDist) {
                float F_hc = HARD_CORE_K * (minDist - r);
                FH_x += F_hc * dx / r * (hIdx == i ? 1.0f : -1.0f);
                FH_y += F_hc * dy / r * (hIdx == i ? 1.0f : -1.0f);
                FO_x -= F_hc * dx / r * (hIdx == i ? 1.0f : -1.0f);
                FO_y -= F_hc * dy / r * (hIdx == i ? 1.0f : -1.0f);
            }

            if (j == accIdx) { fx = FO_x; fy = FO_y; }
            else { fx = FH_x; fy = FH_y; }

            float mag = std::sqrt(fx * fx + fy * fy);
            if (mag > F_MAX) { float s = F_MAX / mag; fx *= s; fy *= s; }
            return;
        }
        else if (accElem == 2) {
            // === Хлор-акцептор (H···Cl, HCl-лёд) ===
            float rHx = atoms[accIdx].pos.x - atoms[hIdx].pos.x;
            float rHy = atoms[accIdx].pos.y - atoms[hIdx].pos.y;
            float rH = std::sqrt(rHx * rHx + rHy * rHy);
            if (rH < 1e-6f) { fx = 0.0f; fy = 0.0f; return; }

            // Линейность Cl_d — H ··· Cl_a
            int donor = -1;
            for (int k : atoms[hIdx].bonds) if (k >= 0) { donor = k; break; }
            float angleFactor = 1.0f;
            if (donor >= 0 && donor < (int)atoms.size()) {
                sf::Vector2f u = atoms[hIdx].pos - atoms[donor].pos;
                float uLen = std::sqrt(u.x * u.x + u.y * u.y);
                if (uLen > 1e-6f) {
                    float cosT = (u.x * rHx + u.y * rHy) / (uLen * rH);
                    cosT = std::clamp(cosT, -1.0f, 1.0f);
                    angleFactor = 0.5f * (1.0f + cosT);
                }
            }

            float delta = rH - HCL_HB_re;
            float expTerm = std::exp(-HCL_HB_a * delta);
            float F = -2.0f * HCL_HB_De * HCL_HB_a * (1.0f - expTerm) * expTerm;
            if (rH > HCL_HB_cutoff) F = 0.0f;
            else if (rH > HCL_HB_cutoff - 0.5f) F *= (HCL_HB_cutoff - rH) / 0.5f;
            F *= angleFactor;

            float FH_x = -F * rHx / rH;
            float FH_y = -F * rHy / rH;
            float FA_x = F * rHx / rH;
            float FA_y = F * rHy / rH;

            float minDist = a1.radius + a2.radius;
            if (r < minDist) {
                float F_hc = HARD_CORE_K * (minDist - r);
                FH_x += F_hc * dx / r * (hIdx == i ? 1.0f : -1.0f);
                FH_y += F_hc * dy / r * (hIdx == i ? 1.0f : -1.0f);
                FA_x -= F_hc * dx / r * (hIdx == i ? 1.0f : -1.0f);
                FA_y -= F_hc * dy / r * (hIdx == i ? 1.0f : -1.0f);
            }

            if (j == accIdx) { fx = FA_x; fy = FA_y; }
            else { fx = FH_x; fy = FH_y; }

            float mag = std::sqrt(fx * fx + fy * fy);
            if (mag > F_MAX) { float s = F_MAX / mag; fx *= s; fy *= s; }
            return;
        }
        // Неизвестный акцептор — не должно случаться
        fx = 0.0f; fy = 0.0f; return;
    }
    else if ((a1.elementId == 0 && a2.elementId == 1) ||
        (a1.elementId == 1 && a2.elementId == 0))
    {
        // === H-O special case ===
        // Притяжение H к O включается ТОЛЬКО когда H реально «хочет» уйти к O:
        //   - H свободен (0 связей), у O < 2 H и у O нет O-партнёра
        //     (иначе H «прилипал» бы к пероксиду H2O2)
        //   - H из HCl (1 связь, партнёр — Cl) и у O < 3 H
        //     → тянем H к O, чтобы сработал триггер диссоциации
        // Все прочие случаи (lone H + вода, H3O+ + H, ...) — только жёсткое ядро.
        int hIdx = (a1.elementId == 0) ? i : j;
        int oIdx = (a1.elementId == 1) ? i : j;

        int hBonds = countBonds(atoms[hIdx]);
        int oHBonds = 0;
        for (int k : atoms[oIdx].bonds) {
            if (k >= 0 && atoms[k].elementId == 0) oHBonds++;
        }

        bool allowAttract = false;
        if (hBonds == 0 && oHBonds < 2) {
            // Свободный H + O с местом → H2O / HO.
            // НО: не тянем H к O, если тот уже сидит на O-O-связи (пероксид).
            bool oHasO = false;
            for (int k : atoms[oIdx].bonds) {
                if (k >= 0 && atoms[k].elementId == 1) { oHasO = true; break; }
            }
            if (!oHasO) allowAttract = true;
        }
        else if (hBonds == 1) {
            // H связан ровно с одним атомом — проверяем, что это Cl
            int partner = -1;
            for (int k : atoms[hIdx].bonds) if (k >= 0) { partner = k; break; }
            if (partner >= 0 && atoms[partner].elementId == 2 && oHBonds < 3) {
                // H из HCl → O ещё не полон (не H3O+) → притягиваем
                allowAttract = true;
            }
        }

        if (allowAttract) {
            const MorsePair mp = getMorsePair(0, 1);
            float delta = r - mp.re;
            float expTerm = std::exp(-mp.a * delta);
            float F = -2.0f * mp.De * mp.a * (1.0f - expTerm) * expTerm;
            if (r > mp.cutoff) F = 0.0f;
            else if (r > mp.cutoff - 0.5f) F *= (mp.cutoff - r) / 0.5f;
            F_total += F;
        }
        // Если allowAttract == false — остаётся только жёсткое ядро из начала функции
    }
    else if ((a1.elementId == 0 && a2.elementId == 2) ||
        (a1.elementId == 2 && a2.elementId == 0))
        {
            // === H-Cl, не связанные ковалентно ===
            int hIdx = (a1.elementId == 0) ? i : j;
            int clIdx = (a1.elementId == 2) ? i : j;

            // Ковалентные партнёры H и Cl
            int hPartner = -1;
            for (int k : atoms[hIdx].bonds) if (k >= 0) { hPartner = k; break; }
            int clPartner = -1;
            for (int k : atoms[clIdx].bonds) if (k >= 0) { clPartner = k; break; }

            bool hInHCl = (hPartner >= 0 && atoms[hPartner].elementId == 2);
            bool clInHCl = (clPartner >= 0 && atoms[clPartner].elementId == 0);
            bool differentMolecules = (hPartner != clIdx) && (clPartner != hIdx);

            if (hInHCl && clInHCl && differentMolecules) {
                // === Водородная связь H···Cl между двумя РАЗНЫМИ молекулами HCl ===
                // Именно она держит HCl-лёд.
                float dirX = atoms[clIdx].pos.x - atoms[hIdx].pos.x;
                float dirY = atoms[clIdx].pos.y - atoms[hIdx].pos.y;
                float dLen = std::sqrt(dirX * dirX + dirY * dirY);
                if (dLen < 1e-6f) { fx = 0.0f; fy = 0.0f; return; }

                // Направленность Cl_a — H ··· Cl_b
                float angleFactor = 1.0f;
                {
                    sf::Vector2f u = atoms[hIdx].pos - atoms[hPartner].pos;
                    float uLen = std::sqrt(u.x * u.x + u.y * u.y);
                    if (uLen > 1e-6f) {
                        float cosT = (u.x * dirX + u.y * dirY) / (uLen * dLen);
                        cosT = std::clamp(cosT, -1.0f, 1.0f);
                        angleFactor = 0.5f * (1.0f + cosT);
                    }
                }

                float delta = r - HCL_HB_re;
                float expTerm = std::exp(-HCL_HB_a * delta);
                float F = -2.0f * HCL_HB_De * HCL_HB_a * (1.0f - expTerm) * expTerm;
                if (r > HCL_HB_cutoff) F = 0.0f;
                else if (r > HCL_HB_cutoff - 0.5f) F *= (HCL_HB_cutoff - r) / 0.5f;
                F_total += F * angleFactor;
            }
            // Иначе — только жёсткое ядро (уже добавлено в начале функции)
    }
    else if (a1.elementId == 4 || a2.elementId == 4) {
        // === Натрий (несвязанный) ===
        // Na–O: мягкое притяжение, чтобы Na «тянулся» к воде.
        // Na–Cl: притяжение по той же Morse-яме, что и у связи —
        //        чтобы пара успела «схватиться», пока updateBonds
        //        не зарегистрирует формальную связь.
        // Na–всё остальное: слабое vdW.
        int otherElem = (a1.elementId == 4) ? a2.elementId : a1.elementId;

        if (otherElem == 1) {
            // Притяжение Na···O (металл тянется к кислороду воды)
            const float naRe = 2.0f;
            const float naDe = 1.5f;
            const float naA = 1.5f;
            const float naCut = 3.5f;
            float delta = r - naRe;
            float expTerm = std::exp(-naA * delta);
            float F = -2.0f * naDe * naA * (1.0f - expTerm) * expTerm;
            if (r > naCut) F = 0.0f;
            else if (r > naCut - 0.5f) F *= (naCut - r) / 0.5f;
            F_total += F;
        }
        else if (otherElem == 2) {
            // Притяжение Na···Cl (до регистрации формальной связи)
            const MorsePair mp = { NACL_De, NACL_re, NACL_a, NACL_cutoff };
            float delta = r - mp.re;
            float expTerm = std::exp(-mp.a * delta);
            float F = -2.0f * mp.De * mp.a * (1.0f - expTerm) * expTerm;
            if (r > mp.cutoff) F = 0.0f;
            else if (r > mp.cutoff - 0.5f) F *= (mp.cutoff - r) / 0.5f;
            F_total += F;
        }
        else {
            // Слабое vdW со всеми остальными элементами
            if (r > minDist && r < VDW_CUTOFF) {
                float delta = r - VDW_re;
                float expTerm = std::exp(-VDW_a * delta);
                float F_vdw = -2.0f * VDW_De * VDW_a * (1.0f - expTerm) * expTerm;
                if (r > VDW_CUTOFF - 1.0f) F_vdw *= (VDW_CUTOFF - r) / 1.0f;
                F_total += F_vdw;
            }
        }
    }
    else if (!sat1 && !sat2) {
        const int e1 = a1.elementId;
        const int e2 = a2.elementId;

        if (e1 == 1 && e2 == 1) {
            // === O-O пара ===
            // Morse-потенциал подбирается так, чтобы он СОВПАДАЛ с тем,
            // который будет действовать ПОСЛЕ образования связи. Иначе
            // при 1+1 (HO· + HO· → H2O2) пара набирала бы энергию
            // глубокой ямы O=O (De=5.5), потом связь образуется, яма
            // мгновенно становится одинарной (De=2.5) — а кинетическая
            // энергия пары уже 5.5 > 2.5, и комплекс разлетается,
            // отрывая один H (получается HO2·).
            int nb1 = countBonds(a1);
            int nb2 = countBonds(a2);
            bool canFormOO = (nb1 == 0 && nb2 == 0) ||
                (nb1 == 1 && nb2 == 1);

            if (canFormOO) {
                MorsePair mp;
                if (nb1 == 0 && nb2 == 0) {
                    // Свободные O: будет двойная O=O — глубокая яма.
                    mp = getMorsePair(1, 1);
                }
                else {
                    // Два HO·: будет одинарная O–O. Притяжение делаем
                    // заметно мягче финальной связи, чтобы к моменту
                    // образования пары у неё было мало кинетической
                    // энергии и одинарная связь её удержала.
                    //   De=1.0 (в 2.5 раза слабее конечной De=2.5),
                    //   re=1.48 (совпадает с конечной),
                    //   a=2.0  (совпадает с конечной),
                    //   cutoff=4.0 (длиннее, чтобы захват шёл плавно).
                    mp = { 1.0f, 1.48f, 2.0f, 4.0f };
                }
                float delta = r - mp.re;
                float expTerm = std::exp(-mp.a * delta);
                float F = -2.0f * mp.De * mp.a * (1.0f - expTerm) * expTerm;
                if (r > mp.cutoff) F = 0.0f;
                else if (r > mp.cutoff - 0.5f) F *= (mp.cutoff - r) / 0.5f;
                F_total += F;
            }
            else {
                // Слабое ван-дер-ваальсово притяжение O···O
                if (r > minDist && r < VDW_CUTOFF) {
                    float delta = r - VDW_re;
                    float expTerm = std::exp(-VDW_a * delta);
                    float F_vdw = -2.0f * VDW_De * VDW_a * (1.0f - expTerm) * expTerm;
                    if (r > VDW_CUTOFF - 1.0f) F_vdw *= (VDW_CUTOFF - r) / 1.0f;
                    F_total += F_vdw;
                }
            }
        }
        else {
            // H-H, H-O, H-Cl, Cl-Cl — как раньше
            const MorsePair mp = getMorsePair(e1, e2);
            float delta = r - mp.re;
            float expTerm = std::exp(-mp.a * delta);
            float F = -2.0f * mp.De * mp.a * (1.0f - expTerm) * expTerm;
            if (r > mp.cutoff) F = 0.0f;
            else if (r > mp.cutoff - 0.5f) F *= (mp.cutoff - r) / 0.5f;
            F_total += F;
        }
    }
    else if (sat1 && sat2) {
        // === Спец-случай: H···Cl между двумя разными молекулами HCl ===
        // HCl-лёд существует за счёт таких связей.
        bool isH = (a1.elementId == 0);
        bool isCl = (a1.elementId == 2);
        bool pairIsHCl = (isH && a2.elementId == 2) || (isCl && a2.elementId == 0);

        if (pairIsHCl) {
            int hIdx = (a1.elementId == 0) ? i : j;
            int clIdx = (a1.elementId == 2) ? i : j;

            // Находим ковалентного партнёра H — это должен быть Cl
            int hPartner = -1;
            for (int k : atoms[hIdx].bonds) if (k >= 0) { hPartner = k; break; }

            // Находим ковалентного партнёра Cl — это должен быть H
            int clPartner = -1;
            for (int k : atoms[clIdx].bonds) if (k >= 0) { clPartner = k; break; }

            bool hIsFromHCl = (hPartner >= 0 && atoms[hPartner].elementId == 2);
            bool clIsFromHCl = (clPartner >= 0 && atoms[clPartner].elementId == 0);

            // Проверка, что это РАЗНЫЕ молекулы
            bool differentMolecules = (hPartner != clIdx) && (clPartner != hIdx);

            if (hIsFromHCl && clIsFromHCl && differentMolecules) {
                // Направленность: угол Cl_a—H···Cl_b близок к 180°
                float angleFactor = 1.0f;
                if (hPartner >= 0) {
                    sf::Vector2f u = atoms[hIdx].pos - atoms[hPartner].pos;
                    float uLen = std::sqrt(u.x * u.x + u.y * u.y);
                    if (uLen > 1e-6f) {
                        float cosT = (u.x * dx + u.y * dy) / (uLen * r);
                        cosT = std::clamp(cosT, -1.0f, 1.0f);
                        angleFactor = 0.5f * (1.0f + cosT);
                    }
                }

                float delta = r - HCL_HB_re;
                float expTerm = std::exp(-HCL_HB_a * delta);
                float F = -2.0f * HCL_HB_De * HCL_HB_a * (1.0f - expTerm) * expTerm;
                if (r > HCL_HB_cutoff) F = 0.0f;
                else if (r > HCL_HB_cutoff - 0.5f) F *= (HCL_HB_cutoff - r) / 0.5f;

                F_total += F * angleFactor;
            }
            else {
                // Между теми же молекулами или странные конфигурации — общий vdW
                if (r > minDist && r < VDW_CUTOFF) {
                    float delta = r - VDW_re;
                    float expTerm = std::exp(-VDW_a * delta);
                    float F_vdw = -2.0f * VDW_De * VDW_a * (1.0f - expTerm) * expTerm;
                    if (r > VDW_CUTOFF - 1.0f) F_vdw *= (VDW_CUTOFF - r) / 1.0f;
                    F_total += F_vdw;
                }
            }
        }
        else {
            // Обычный vdW между насыщенными атомами (H-H, Cl-Cl и т.п.)
            if (r > minDist && r < VDW_CUTOFF) {
                float delta = r - VDW_re;
                float expTerm = std::exp(-VDW_a * delta);
                float F_vdw = -2.0f * VDW_De * VDW_a * (1.0f - expTerm) * expTerm;
                if (r > VDW_CUTOFF - 1.0f) F_vdw *= (VDW_CUTOFF - r) / 1.0f;
                F_total += F_vdw;
            }
        }
    }

    if (std::abs(F_total) > F_MAX) F_total = std::copysign(F_MAX, F_total);
    fx = F_total * dx / r;
    fy = F_total * dy / r;
}

// ---------- связи ----------

void updateBonds(std::vector<Atom>& atoms, const SpatialGrid& grid) {
    const int n = (int)atoms.size();

    // ------------------------------------------------------------
    // 1) Разрыв / трансформация связей.
    //    Порог разрыва — парный (getBondDists).
    //    HCl + H2O → H3O+ + Cl-  (атомарный перенос H на O)
    //    Обратной реакции НЕТ: HCl — сильная кислота, равновесие
    //    практически полностью смещено в сторону диссоциации.
    // ------------------------------------------------------------
    for (int i = 0; i < n; ++i) {
        for (int slot = 0; slot < 4; ++slot) {
            int j = atoms[i].bonds[slot];
            if (j < 0 || j <= i) continue;
            if (j >= n || !isBondedTo(atoms[j], i)) {
                atoms[i].bonds[slot] = -1;
                continue;
            }

            const int e1 = atoms[i].elementId;
            const int e2 = atoms[j].elementId;
            bool breakIt = false;
            bool handled = false;

            // a) растянуто
            auto bd = getBondDists(e1, e2);
            float brk2 = bd.brk * bd.brk;
            float dx = atoms[j].pos.x - atoms[i].pos.x;
            float dy = atoms[j].pos.y - atoms[i].pos.y;
            if (dx * dx + dy * dy > brk2) breakIt = true;

            // b) HCl-диссоциация с атомарным переносом H на O.
            //    Только если рядом есть O со свободным слотом.
            //    Сразу делаем H-O, чтобы H3O+ получался строго через кислоту.
            if (!breakIt && ((e1 == 0 && e2 == 2) || (e1 == 2 && e2 == 0))) {
                int hIdx = (e1 == 0) ? i : j;
                int clIdx = (e1 == 2) ? i : j;
                const Atom& h = atoms[hIdx];
                int cx = grid.cellX(h.pos.x);
                int cy = grid.cellY(h.pos.y);
                int oTarget = -1;
                for (int ddy = -1; ddy <= 1 && oTarget < 0; ++ddy) {
                    for (int ddx = -1; ddx <= 1 && oTarget < 0; ++ddx) {
                        int nx = cx + ddx, ny = cy + ddy;
                        if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows()) continue;
                        for (int k : grid.at(nx, ny)) {
                            if (k == hIdx || k == clIdx) continue;
                            if (atoms[k].elementId != 1) continue;   // только O
                            if (countBonds(atoms[k]) >= 3) continue; // нет слота
                            float dxo = atoms[k].pos.x - h.pos.x;
                            float dyo = atoms[k].pos.y - h.pos.y;
                            if (dxo * dxo + dyo * dyo < HCL_DISSOC_DIST * HCL_DISSOC_DIST) {
                                oTarget = k;
                                break;
                            }
                        }
                    }
                }
                if (oTarget >= 0) {
                    // Атомарный перенос H-Cl → H-O
                    removeAllBondsTo(atoms[hIdx], clIdx);
                    removeAllBondsTo(atoms[clIdx], hIdx);
                    addBond(atoms[hIdx], oTarget);
                    addBond(atoms[oTarget], hIdx);
                    handled = true;
                }
            }

            // (Блок c) — обратная диссоциация H3O+ + Cl- → HCl + H2O —
            //  удалён. HCl в воде диссоциирует необратимо.)

            if (!handled && breakIt) {
                removeAllBondsTo(atoms[i], j);
                removeAllBondsTo(atoms[j], i);
            }
        }
    }

    // ------------------------------------------------------------
    // 2) Образование новых связей.
    //    H-H      : одинарная                              → H2
    //    H-O      : только если у O меньше 2 H и нет O-O    → H2O
    //               (H3O+ получается исключительно в фазе 1
    //                через кислотную диссоциацию)
    //    O=O      : двойная, только между двумя свободными → O2
    //    O-O      : одинарная, только между двумя HO·      → H2O2
    //    H-Cl     : одинарная (подавляется рядом с O)      → HCl
    //    Cl-Cl    : одинарная, но НЕ образуется, если рядом
    //               есть вода или H3O (иначе система уходит
    //               в Cl2 + H2)
    //    O-Cl     : запрещено
    // ------------------------------------------------------------
    for (size_t i = 0; i < atoms.size(); ++i) {
        if (!hasFreeBond(atoms[i])) continue;

        int cx = grid.cellX(atoms[i].pos.x);
        int cy = grid.cellY(atoms[i].pos.y);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows()) continue;

                for (int j : grid.at(nx, ny)) {
                    if (j <= (int)i) continue;
                    if (!hasFreeBond(atoms[i]) || !hasFreeBond(atoms[j])) continue;
                    if (isBondedTo(atoms[i], j)) continue;

                    const int e1 = atoms[i].elementId;
                    const int e2 = atoms[j].elementId;

                    auto bd = getBondDists(e1, e2);
                    float form2 = bd.form * bd.form;

                    float ddx = atoms[j].pos.x - atoms[i].pos.x;
                    float ddy = atoms[j].pos.y - atoms[i].pos.y;
                    if (ddx * ddx + ddy * ddy >= form2) continue;

                    if (e1 == 1 && e2 == 1) {
                        int nb1 = countBonds(atoms[i]);
                        int nb2 = countBonds(atoms[j]);

                        if (nb1 == 0 && nb2 == 0) {
                            // O=O — двойная
                            addBond(atoms[i], j);
                            addBond(atoms[i], j);
                            addBond(atoms[j], (int)i);
                            addBond(atoms[j], (int)i);
                        }
                        else if (nb1 == 1 && nb2 == 1) {
                            // O-O — одинарная (H2O2)
                            addBond(atoms[i], j);
                            addBond(atoms[j], (int)i);
                        }
                    }
                    else if ((e1 == 1 && e2 == 2) || (e1 == 2 && e2 == 1)) {
                        // O-Cl: запрещено
                        continue;
                    }
                    else if ((e1 == 0 && e2 == 2) || (e1 == 2 && e2 == 0)) {
                        // H-Cl: не создаём рядом с O со свободным слотом
                        int hIdx = (e1 == 0) ? (int)i : j;
                        const Atom& h = atoms[hIdx];
                        bool suppress = false;
                        int hcx = grid.cellX(h.pos.x);
                        int hcy = grid.cellY(h.pos.y);
                        for (int ddy = -1; ddy <= 1 && !suppress; ++ddy) {
                            for (int ddx = -1; ddx <= 1 && !suppress; ++ddx) {
                                int hnx = hcx + ddx, hny = hcy + ddy;
                                if (hnx < 0 || hnx >= grid.cols() || hny < 0 || hny >= grid.rows()) continue;
                                for (int k : grid.at(hnx, hny)) {
                                    if (k == hIdx) continue;
                                    if (atoms[k].elementId != 1) continue;
                                    if (!hasFreeBond(atoms[k])) continue;
                                    float dxo = atoms[k].pos.x - h.pos.x;
                                    float dyo = atoms[k].pos.y - h.pos.y;
                                    if (dxo * dxo + dyo * dyo < HCL_DISSOC_DIST * HCL_DISSOC_DIST) {
                                        suppress = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (suppress) continue;
                        addBond(atoms[i], j);
                        addBond(atoms[j], (int)i);
                    }
                    else if (e1 == 4 || e2 == 4) {
                        // Na: разрешаем ТОЛЬКО Na-Cl (ионная связь).
                        // Все прочие пары с участием Na отклоняются,
                        // чтобы не сломать уже работающие H-O, H-H,
                        // H-Cl, Cl-Cl, O-O.
                        if ((e1 == 4 && e2 == 2) || (e1 == 2 && e2 == 4)) {
                            addBond(atoms[i], j);
                            addBond(atoms[j], (int)i);
                        }
                        // иначе — связь не создаётся
                    }
                    else if (e1 == 2 && e2 == 2) {
                        // Cl-Cl: не образуем, если рядом есть O (вода или H3O)
                        // — иначе система уходит в Cl2 + H2.
                        bool oNearby = false;
                        int ccx = grid.cellX(atoms[i].pos.x);
                        int ccy = grid.cellY(atoms[i].pos.y);
                        for (int ddy = -1; ddy <= 1 && !oNearby; ++ddy) {
                            for (int ddx = -1; ddx <= 1 && !oNearby; ++ddx) {
                                int hnx = ccx + ddx, hny = ccy + ddy;
                                if (hnx < 0 || hnx >= grid.cols() || hny < 0 || hny >= grid.rows()) continue;
                                for (int k : grid.at(hnx, hny)) {
                                    if (k == (int)i || k == j) continue;
                                    if (atoms[k].elementId != 1) continue;
                                    // любой O в радиусе VDW_CUTOFF — блокируем Cl-Cl
                                    float dxo = atoms[k].pos.x - atoms[i].pos.x;
                                    float dyo = atoms[k].pos.y - atoms[i].pos.y;
                                    if (dxo * dxo + dyo * dyo < VDW_CUTOFF * VDW_CUTOFF) {
                                        oNearby = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (oNearby) continue;
                        addBond(atoms[i], j);
                        addBond(atoms[j], (int)i);
                    }
                    else {
                        // H-O и H-H
                        if ((e1 == 0 && e2 == 1) || (e1 == 1 && e2 == 0)) {
                            // Запрет H-O:
                            //   • если у O уже 2 H (нельзя получить H3O+
                            //     из свободного H — только из HCl)
                            //   • если у O есть O-партнёр (нельзя получить
                            //     «воду-на-пероксиде»)
                            int oIdx = (e1 == 1) ? (int)i : j;
                            int hCount = 0;
                            int oCount = 0;
                            for (int k : atoms[oIdx].bonds) {
                                if (k < 0) continue;
                                if (atoms[k].elementId == 0) hCount++;
                                else if (atoms[k].elementId == 1) oCount++;
                            }
                            if (hCount >= 2) continue;
                            if (oCount > 0)  continue;
                        }
                        addBond(atoms[i], j);
                        addBond(atoms[j], (int)i);
                    }
                }
            }
        }
    }
}

void updateHBonds(std::vector<Atom>& atoms, const SpatialGrid& grid,
    float physTemp, std::mt19937& rng)
{
    const int n = (int)atoms.size();
    std::uniform_real_distribution<float> uni01(0.0f, 1.0f);

    // --- Температурные пороги ---
        // --- Температурные пороги ---
    // У воды и HCl-льда разные точки плавления — разные пороги.
    float breakProbWater = 0.0f;
    if (physTemp > HBOND_BREAK_TEMP_MIN) {
        breakProbWater = (physTemp - HBOND_BREAK_TEMP_MIN) /
            (HBOND_BREAK_TEMP_MAX - HBOND_BREAK_TEMP_MIN);
        breakProbWater = std::clamp(breakProbWater, 0.0f, 1.0f);
    }
    float breakProbHCl = 0.0f;
    if (physTemp > HCL_HB_BREAK_TEMP_MIN) {
        breakProbHCl = (physTemp - HCL_HB_BREAK_TEMP_MIN) /
            (HCL_HB_BREAK_TEMP_MAX - HCL_HB_BREAK_TEMP_MIN);
        breakProbHCl = std::clamp(breakProbHCl, 0.0f, 1.0f);
    }
    const bool canFormWater = (physTemp < HBOND_FORM_TEMP_MAX);
    const bool canFormHCl = (physTemp < HCL_HBOND_FORM_TEMP_MAX);

    // ==========================================================
    // 1) Разрыв H-связей (геометрия + термический шум)
    // ==========================================================
    for (int i = 0; i < n; ++i) {
        for (int slot = 0; slot < 4; ++slot) {
            int j = atoms[i].hbonds[slot];
            if (j < 0 || j <= i) continue;
            if (j >= n || !isHBondedTo(atoms[j], i)) {
                atoms[i].hbonds[slot] = -1;
                continue;
            }

            auto isHDonor = [&](const Atom& a) {
                return a.elementId == 0 && countBonds(a) == 1;
                };
            auto isAcceptor = [&](const Atom& a) {
                if (a.elementId == 1) {                // O: 2 или 3 H
                    int nb = countBonds(a);
                    return nb >= 2 && nb <= 3;
                }
                if (a.elementId == 2) return true;     // Cl (HCl или Cl-)
                return false;
                };

            bool pairOk =
                (isHDonor(atoms[i]) && isAcceptor(atoms[j])) ||
                (isAcceptor(atoms[i]) && isHDonor(atoms[j]));
            if (!pairOk) {
                removeAllHBondsTo(atoms[i], j);
                removeAllHBondsTo(atoms[j], i);
                continue;
            }

            // Разные дистанции разрыва для H···O и H···Cl
            bool isHClPair =
                (atoms[i].elementId == 0 && atoms[j].elementId == 2) ||
                (atoms[i].elementId == 2 && atoms[j].elementId == 0);
            float brkDist = isHClPair ? HCL_HB_BREAK_DIST
                : HBOND_BREAK_DIST;

            float dx = atoms[j].pos.x - atoms[i].pos.x;
            float dy = atoms[j].pos.y - atoms[i].pos.y;
            if (dx * dx + dy * dy > brkDist * brkDist) {
                removeAllHBondsTo(atoms[i], j);
                removeAllHBondsTo(atoms[j], i);
                continue;
            }

            // Термический разрыв: у HCl-льда своя температура плавления
            float breakProb = isHClPair ? breakProbHCl : breakProbWater;
            if (breakProb > 0.0f && uni01(rng) < breakProb) {
                removeAllHBondsTo(atoms[i], j);
                removeAllHBondsTo(atoms[j], i);
            }
        }
    }

    // ==========================================================
    // 2) Образование H-связей воды (акцептор — O, через M-сайт)
    // ==========================================================
    if (canFormWater) {
        for (int i = 0; i < n; ++i) {
            if (atoms[i].elementId != 0) continue;
            if (countBonds(atoms[i]) != 1) continue;
            if (countHBonds(atoms[i]) > 0) continue;

            int oDonor = -1;
            for (int k : atoms[i].bonds) if (k >= 0) { oDonor = k; break; }
            if (oDonor < 0 || oDonor >= n) continue;
            if (atoms[oDonor].elementId != 1) continue;
            if (countBonds(atoms[oDonor]) < 2) continue;

            const int cx = grid.cellX(atoms[i].pos.x);
            const int cy = grid.cellY(atoms[i].pos.y);

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || nx >= grid.cols() ||
                        ny < 0 || ny >= grid.rows()) continue;

                    for (int j : grid.at(nx, ny)) {
                        if (j == i || j == oDonor) continue;
                        if (atoms[j].elementId != 1) continue;
                        if (countBonds(atoms[j]) < 2) continue;
                        if (countHBonds(atoms[j]) >= HBOND_SLOTS_O) continue;

                        bool reverseExists = false;
                        for (int hk : atoms[j].bonds) {
                            if (hk < 0 || hk >= n) continue;
                            if (atoms[hk].elementId != 0) continue;
                            if (isHBondedTo(atoms[hk], oDonor)) {
                                reverseExists = true; break;
                            }
                        }
                        if (reverseExists) continue;

                        sf::Vector2f mPos;
                        if (!computeTip4pSite(atoms, j, mPos)) continue;
                        float ddx = mPos.x - atoms[i].pos.x;
                        float ddy = mPos.y - atoms[i].pos.y;
                        float r2 = ddx * ddx + ddy * ddy;
                        if (r2 > HBOND_FORM_DIST * HBOND_FORM_DIST) continue;
                        float rH = std::sqrt(r2);
                        if (rH < 1e-6f) continue;

                        sf::Vector2f v1 = atoms[i].pos - atoms[oDonor].pos;
                        float l1 = std::sqrt(v1.x * v1.x + v1.y * v1.y);
                        if (l1 < 1e-6f) continue;
                        float cosT = (v1.x * ddx + v1.y * ddy) / (l1 * rH);
                        if (cosT < HBOND_ANGLE_COS_MIN) continue;

                        bool angularConflict = false;
                        for (int existing : atoms[j].hbonds) {
                            if (existing < 0 || existing >= n) continue;
                            sf::Vector2f dE = atoms[existing].pos - atoms[j].pos;
                            float LE = std::sqrt(dE.x * dE.x + dE.y * dE.y);
                            if (LE < 1e-6f) continue;
                            sf::Vector2f eDir = dE / LE;
                            float toHx = -ddx / rH;
                            float toHy = -ddy / rH;
                            float cosBt = toHx * eDir.x + toHy * eDir.y;
                            if (cosBt > 0.5f) { angularConflict = true; break; }
                        }
                        if (angularConflict) continue;

                        addHBond(atoms[i], j);
                        addHBond(atoms[j], i);
                    }
                }
            }
        }
    }

    // ==========================================================
    // 3) Образование H-связей HCl (акцептор — Cl, напрямую)
    // ==========================================================
    if (canFormHCl) {
        for (int i = 0; i < n; ++i) {
            if (atoms[i].elementId != 0) continue;
            if (countBonds(atoms[i]) != 1) continue;
            if (countHBonds(atoms[i]) > 0) continue;

            int donor = -1;
            for (int k : atoms[i].bonds) if (k >= 0) { donor = k; break; }
            if (donor < 0 || donor >= n) continue;
            int donorElem = atoms[donor].elementId;
            if (donorElem != 1 && donorElem != 2) continue;   // O или Cl

            const int cx = grid.cellX(atoms[i].pos.x);
            const int cy = grid.cellY(atoms[i].pos.y);

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || nx >= grid.cols() ||
                        ny < 0 || ny >= grid.rows()) continue;

                    for (int j : grid.at(nx, ny)) {
                        if (j == i || j == donor) continue;
                        if (atoms[j].elementId != 2) continue;   // Cl акцептор
                        if (countHBonds(atoms[j]) >= HCL_HB_SLOTS_CL) continue;

                        // Cl в Cl2 не акцептор
                        bool clClBonded = false;
                        for (int k : atoms[j].bonds) {
                            if (k < 0) continue;
                            if (atoms[k].elementId == 2) { clClBonded = true; break; }
                        }
                        if (clClBonded) continue;

                        float ddx = atoms[j].pos.x - atoms[i].pos.x;
                        float ddy = atoms[j].pos.y - atoms[i].pos.y;
                        float r2 = ddx * ddx + ddy * ddy;
                        if (r2 > HCL_HB_FORM_DIST * HCL_HB_FORM_DIST) continue;
                        float rH = std::sqrt(r2);
                        if (rH < 1e-6f) continue;

                        sf::Vector2f v1 = atoms[i].pos - atoms[donor].pos;
                        float l1 = std::sqrt(v1.x * v1.x + v1.y * v1.y);
                        if (l1 < 1e-6f) continue;
                        float cosT = (v1.x * ddx + v1.y * ddy) / (l1 * rH);
                        if (cosT < HCL_HB_ANGLE_COS_MIN) continue;

                        addHBond(atoms[i], j);
                        addHBond(atoms[j], i);
                    }
                }
            }
        }
    }
}

// ---------- угловые силы ----------

void applyHBondAngularForces(const std::vector<Atom>& atoms,
    std::vector<sf::Vector2f>& forces)
{
    const size_t n = atoms.size();
    for (size_t i = 0; i < n; ++i) {
        if (atoms[i].elementId != 0) continue;

        int donor = -1;
        for (int k : atoms[i].bonds) if (k >= 0) { donor = k; break; }
        if (donor < 0) continue;
        if ((size_t)donor >= n) continue;

        sf::Vector2f u = atoms[i].pos - atoms[donor].pos;
        float uLen2 = u.x * u.x + u.y * u.y;
        if (uLen2 < 1e-8f) continue;
        float uLen = std::sqrt(uLen2);
        sf::Vector2f uHat = u / uLen;

        for (int j : atoms[i].hbonds) {
            if (j < 0) continue;
            if ((size_t)j >= n) continue;
            int accElem = atoms[j].elementId;
            if (accElem != 1 && accElem != 2) continue;   // O или Cl
            if (!isHBondedTo(atoms[j], (int)i)) continue;

            sf::Vector2f w = atoms[j].pos - atoms[i].pos;
            float along = w.x * uHat.x + w.y * uHat.y;
            sf::Vector2f perp = { w.x - uHat.x * along,
                                  w.y - uHat.y * along };

            sf::Vector2f F_ang = { -HBOND_K_ANG * perp.x,
                                   -HBOND_K_ANG * perp.y };
            float m = std::sqrt(F_ang.x * F_ang.x + F_ang.y * F_ang.y);
            if (m > ANGLE_FORCE_MAX) {
                float sc = ANGLE_FORCE_MAX / m;
                F_ang *= sc;
            }
            forces[j].x += F_ang.x;
            forces[j].y += F_ang.y;
            forces[i].x -= F_ang.x;
            forces[i].y -= F_ang.y;
        }
    }
}

void applyWaterAngleForces(const std::vector<Atom>& atoms,
    std::vector<sf::Vector2f>& forces)
{
    const size_t n = atoms.size();
    const float theta0 = WATER_ANGLE_DEG * 3.14159265f / 180.0f;

    for (size_t i = 0; i < n; ++i) {
        if (atoms[i].elementId != 1) continue;

        // Берём ровно двух H. Если у O три H (H3O+), угловая пружина
        // продолжит работать на первых двух — это допустимо для 2D-модели.
        int h1 = -1, h2 = -1;
        for (int k : atoms[i].bonds) {
            if (k < 0) continue;
            if ((size_t)k >= n) continue;
            if (atoms[k].elementId != 0) continue;
            if (h1 < 0) h1 = k;
            else if (h2 < 0) { h2 = k; break; }
        }
        if (h1 < 0 || h2 < 0 || h1 == h2) continue;

        sf::Vector2f u1 = atoms[h1].pos - atoms[i].pos;
        sf::Vector2f u2 = atoms[h2].pos - atoms[i].pos;
        float L1 = std::sqrt(u1.x * u1.x + u1.y * u1.y);
        float L2 = std::sqrt(u2.x * u2.x + u2.y * u2.y);
        if (L1 < 1e-6f || L2 < 1e-6f) continue;

        sf::Vector2f e1 = u1 / L1;
        sf::Vector2f e2 = u2 / L2;

        float c = e1.x * e2.x + e1.y * e2.y;
        if (c > 1.0f) c = 1.0f;
        if (c < -1.0f) c = -1.0f;
        float theta = std::acos(c);
        float s = std::sin(theta);
        if (std::abs(s) < 1e-4f) continue;

        float dTheta = theta - theta0;
        float common = 2.0f * WATER_ANGLE_K * dTheta / s;

        sf::Vector2f F1 = (common / L1) * sf::Vector2f(e2.x - c * e1.x, e2.y - c * e1.y);
        sf::Vector2f F2 = (common / L2) * sf::Vector2f(e1.x - c * e2.x, e1.y - c * e2.y);
        sf::Vector2f F0 = -(F1 + F2);

        // === КЛЭМП: масштабируем ВСЕ три вместе, если превышен потолок ===
        float m1 = std::sqrt(F1.x * F1.x + F1.y * F1.y);
        float m2 = std::sqrt(F2.x * F2.x + F2.y * F2.y);
        float m0 = std::sqrt(F0.x * F0.x + F0.y * F0.y);
        float maxM = std::max({ m1, m2, m0 });
        if (maxM > ANGLE_FORCE_MAX) {
            float scale = ANGLE_FORCE_MAX / maxM;
            F1 *= scale; F2 *= scale; F0 *= scale;
        }
        // ==================================================================

        forces[h1].x += F1.x;  forces[h1].y += F1.y;
        forces[h2].x += F2.x;  forces[h2].y += F2.y;
        forces[i].x += F0.x;   forces[i].y += F0.y;
    }
}

// ============================================================
// Угол H-O-O в пероксиде водорода (H2O2)
// ============================================================
// Для каждого O в пероксидной конфигурации (1 H + 1 O) удерживаем
// плоский угол H-O-O около PEROXIDE_ANGLE_DEG.
void applyPeroxideAngleForces(const std::vector<Atom>& atoms,
    std::vector<sf::Vector2f>& forces)
{
    const size_t n = atoms.size();
    const float theta0 = PEROXIDE_ANGLE_DEG * 3.14159265f / 180.0f;

    for (size_t i = 0; i < n; ++i) {
        if (atoms[i].elementId != 1) continue;   // центр угла — кислород

        // Ищем H и второй O среди ковалентных партнёров
        int hIdx = -1;
        int oIdx = -1;
        int nBonds = 0;
        for (int k : atoms[i].bonds) {
            if (k < 0) continue;
            if ((size_t)k >= n) continue;
            nBonds++;
            if (atoms[k].elementId == 0) hIdx = k;
            else if (atoms[k].elementId == 1) oIdx = k;
        }
        if (nBonds != 2) continue;      // ровно 2 связи
        if (hIdx < 0 || oIdx < 0) continue;

        // Проверяем, что второй O — тоже в пероксидной конфигурации
        // (H + O), чтобы не ловить случайные O-O бонды.
        bool partnerHasH = false;
        bool partnerHasO = false;
        for (int k : atoms[oIdx].bonds) {
            if (k < 0) continue;
            if ((size_t)k >= n) continue;
            if (atoms[k].elementId == 0) partnerHasH = true;
            else if (atoms[k].elementId == 1) partnerHasO = true;
        }
        if (!partnerHasH || !partnerHasO) continue;

        sf::Vector2f u1 = atoms[hIdx].pos - atoms[i].pos;   // O -> H
        sf::Vector2f u2 = atoms[oIdx].pos - atoms[i].pos;   // O -> O
        float L1 = std::sqrt(u1.x * u1.x + u1.y * u1.y);
        float L2 = std::sqrt(u2.x * u2.x + u2.y * u2.y);
        if (L1 < 1e-6f || L2 < 1e-6f) continue;

        sf::Vector2f e1 = u1 / L1;
        sf::Vector2f e2 = u2 / L2;

        float c = e1.x * e2.x + e1.y * e2.y;
        if (c > 1.0f) c = 1.0f;
        if (c < -1.0f) c = -1.0f;
        float theta = std::acos(c);
        float s = std::sin(theta);
        if (std::abs(s) < 1e-4f) continue;

        float dTheta = theta - theta0;
        float common = 2.0f * PEROXIDE_ANGLE_K * dTheta / s;

        sf::Vector2f F1 = (common / L1) * sf::Vector2f(e2.x - c * e1.x, e2.y - c * e1.y);
        sf::Vector2f F2 = (common / L2) * sf::Vector2f(e1.x - c * e2.x, e1.y - c * e2.y);
        sf::Vector2f F0 = -(F1 + F2);

        // Тот же клэмп, что и у воды: масштабируем все три вектора вместе
        float m1 = std::sqrt(F1.x * F1.x + F1.y * F1.y);
        float m2 = std::sqrt(F2.x * F2.x + F2.y * F2.y);
        float m0 = std::sqrt(F0.x * F0.x + F0.y * F0.y);
        float maxM = std::max({ m1, m2, m0 });
        if (maxM > ANGLE_FORCE_MAX) {
            float scale = ANGLE_FORCE_MAX / maxM;
            F1 *= scale; F2 *= scale; F0 *= scale;
        }

        forces[hIdx].x += F1.x;  forces[hIdx].y += F1.y;
        forces[oIdx].x += F2.x;  forces[oIdx].y += F2.y;
        forces[i].x += F0.x;     forces[i].y += F0.y;
    }
}

// ============================================================
// Столкновения атомов со стенами.
// Стена — отрезок с полутолщиной WALL_THICKNESS/2.
// Отражение скорости по нормали с коэффициентом WALL_RESTITUTION.
// ============================================================
void applyWallsToAtoms(std::vector<Atom>& atoms,
    const std::vector<Wall>& walls)
{
    const float halfT = WALL_THICKNESS * 0.5f;

    for (auto& atom : atoms) {
        for (const auto& wall : walls) {
            sf::Vector2f ab = wall.b - wall.a;
            float ab2 = ab.x * ab.x + ab.y * ab.y;
            if (ab2 < 1e-8f) continue;

            sf::Vector2f ap = atom.pos - wall.a;
            float t = (ap.x * ab.x + ap.y * ab.y) / ab2;
            t = std::clamp(t, 0.0f, 1.0f);

            sf::Vector2f closest = wall.a + t * ab;
            sf::Vector2f diff = atom.pos - closest;
            float d2 = diff.x * diff.x + diff.y * diff.y;

            float r = atom.radius + halfT;
            if (d2 >= r * r) continue;
            if (d2 < 1e-8f) {
                // Точно на линии — выталкиваем по нормали сегмента
                float len = std::sqrt(ab2);
                sf::Vector2f n(-ab.y / len, ab.x / len);
                atom.pos = closest + n * r;
                continue;
            }

            float d = std::sqrt(d2);
            sf::Vector2f n = diff / d;

            // Выталкиваем из стены
            atom.pos = closest + n * r;

            // Отражаем нормальную компоненту скорости
            float vn = atom.vel.x * n.x + atom.vel.y * n.y;
            if (vn < 0.0f) {
                atom.vel.x -= (1.0f + WALL_RESTITUTION) * vn * n.x;
                atom.vel.y -= (1.0f + WALL_RESTITUTION) * vn * n.y;
            }
        }
    }
}

// ============================================================
// Реакция натрия с водой: 2Na + 2H2O → 2NaOH + H2 + тепло
// ============================================================
// Как только атом Na оказывается ближе NA_WATER_REACT_DIST к
// кислороду какой-нибудь молекулы воды (O с двумя H), срабатывает
// «взрыв»:
//   1) Обе O–H связи рвутся, атомы H разлетаются радиально;
//   2) Все атомы в радиусе NA_EXPLOSION_RADIUS получают радиальный
//      импульс (ударная волна);
//   3) Na исчезает (превратился в NaOH).
// Реакция одноразовая: один Na + одна вода → один «взрыв».
void applySodiumWaterReaction(std::vector<Atom>& atoms) {
    bool anyDead = false;

    for (size_t i = 0; i < atoms.size(); ++i) {
        if (atoms[i].elementId != 4) continue;   // Na
        // NaCl уже нейтрализован — Na+ не реагирует с водой.
        // Реакция срабатывает только для «металлического» Na.
        if (countBonds(atoms[i]) > 0) continue;

        const sf::Vector2f naPos = atoms[i].pos;

        // Ищем ближайшую воду (O с двумя H) в радиусе реакции
        int waterO = -1;
        float bestD2 = NA_WATER_REACT_DIST * NA_WATER_REACT_DIST;
        for (size_t j = 0; j < atoms.size(); ++j) {
            if (j == i) continue;
            if (atoms[j].elementId != 1) continue;
            int hCount = 0;
            for (int k : atoms[j].bonds)
                if (k >= 0 && atoms[k].elementId == 0) hCount++;
            if (hCount < 2) continue;
            float dx = atoms[j].pos.x - naPos.x;
            float dy = atoms[j].pos.y - naPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestD2) { bestD2 = d2; waterO = (int)j; }
        }
        if (waterO < 0) continue;

        // --- ВЗРЫВ ---

        // 1) Рвём O–H связи воды и разгоняем оба H от центра реакции
        for (int slot = 0; slot < 4; ++slot) {
            int k = atoms[waterO].bonds[slot];
            if (k < 0) continue;
            if (atoms[k].elementId != 0) continue;

            removeAllBondsTo(atoms[waterO], k);
            removeAllBondsTo(atoms[k], waterO);
            // Чистим H-связи, в которых участвовал этот H или вода
            for (auto& a : atoms) {
                removeAllHBondsTo(a, k);
                removeAllHBondsTo(a, waterO);
            }

            float dx = atoms[k].pos.x - naPos.x;
            float dy = atoms[k].pos.y - naPos.y;
            float L = std::sqrt(dx * dx + dy * dy);
            if (L > 1e-6f) {
                atoms[k].vel.x += dx / L * NA_EXPLOSION_KICK;
                atoms[k].vel.y += dy / L * NA_EXPLOSION_KICK;
            }
        }

        // 2) Ударная волна: радиальный импульс всем в радиусе
        const float R2 = NA_EXPLOSION_RADIUS * NA_EXPLOSION_RADIUS;
        for (size_t j = 0; j < atoms.size(); ++j) {
            if (j == i) continue;
            float dx = atoms[j].pos.x - naPos.x;
            float dy = atoms[j].pos.y - naPos.y;
            float r2 = dx * dx + dy * dy;
            if (r2 > R2) continue;
            float L = std::sqrt(r2);
            if (L < 1e-6f) continue;
            float falloff = 1.0f - L / NA_EXPLOSION_RADIUS;   // линейное затухание
            atoms[j].vel.x += dx / L * NA_EXPLOSION_KICK * falloff * 0.6f;
            atoms[j].vel.y += dy / L * NA_EXPLOSION_KICK * falloff * 0.6f;
        }

        // 3) Поглощаем Na
        atoms[i].elementId = -1;
        anyDead = true;
    }

    if (!anyDead) return;

    // Компактизация: удаляем помеченные атомы и перенаправляем ссылки
    // (тот же приём, что и в compactAtomsAfterFission).
    std::vector<int> remap(atoms.size(), -1);
    int newIdx = 0;
    for (size_t k = 0; k < atoms.size(); ++k) {
        if (atoms[k].elementId >= 0) remap[k] = newIdx++;
    }
    for (auto& a : atoms) {
        if (a.elementId < 0) continue;
        for (auto& b : a.bonds)
            if (b >= 0) b = (b < (int)remap.size()) ? remap[b] : -1;
        for (auto& b : a.hbonds)
            if (b >= 0) b = (b < (int)remap.size()) ? remap[b] : -1;
    }
    atoms.erase(std::remove_if(atoms.begin(), atoms.end(),
        [](const Atom& a) { return a.elementId < 0; }), atoms.end());
}

// ---------- фабрики ----------

Atom makeHydrogen(sf::Vector2f pos, sf::Vector2f vel) {
    return makeAtom(0, pos, vel);
}

Atom makeOxygen(sf::Vector2f pos, sf::Vector2f vel) {
    return makeAtom(1, pos, vel);
}

// ============================================================
// Столкновения атомов с неподвижными квадратными колоннами (уровень 4)
// Коллизия: AABB (квадрат) vs. окружность (атом).
// ============================================================
void applyPillarsToAtoms(std::vector<Atom>& atoms,
    const std::vector<Pillar>& pillars)
{
    for (auto& atom : atoms) {
        for (const auto& p : pillars) {
            const float h = p.halfSize;
            const float r = atom.radius;

            // Ближайшая точка AABB к центру атома
            float cX = std::clamp(atom.pos.x, p.pos.x - h, p.pos.x + h);
            float cY = std::clamp(atom.pos.y, p.pos.y - h, p.pos.y + h);
            float dx = atom.pos.x - cX;
            float dy = atom.pos.y - cY;
            float d2 = dx * dx + dy * dy;

            if (d2 >= r * r) continue;   // нет коллизии

            if (d2 > 1e-8f) {
                // Центр атома вне квадрата — выталкиваем по нормали
                float d = std::sqrt(d2);
                float nx = dx / d;
                float ny = dy / d;
                float overlap = r - d;
                atom.pos.x += nx * overlap;
                atom.pos.y += ny * overlap;

                float vn = atom.vel.x * nx + atom.vel.y * ny;
                if (vn < 0.0f) {
                    atom.vel.x -= (1.0f + WALL_RESTITUTION) * vn * nx;
                    atom.vel.y -= (1.0f + WALL_RESTITUTION) * vn * ny;
                }
            }
            else {
                // Центр атома внутри квадрата — выталкиваем через ближайшую грань
                float left = atom.pos.x - (p.pos.x - h);
                float right = (p.pos.x + h) - atom.pos.x;
                float top = atom.pos.y - (p.pos.y - h);
                float bottom = (p.pos.y + h) - atom.pos.y;

                float mn = left;
                int face = 0;   // 0=left, 1=right, 2=top, 3=bottom
                if (right < mn) { mn = right; face = 1; }
                if (top < mn) { mn = top; face = 2; }
                if (bottom < mn) { mn = bottom; face = 3; }

                switch (face) {
                case 0:
                    atom.pos.x = p.pos.x - h - r;
                    if (atom.vel.x > 0.0f) atom.vel.x = -atom.vel.x * WALL_RESTITUTION;
                    break;
                case 1:
                    atom.pos.x = p.pos.x + h + r;
                    if (atom.vel.x < 0.0f) atom.vel.x = -atom.vel.x * WALL_RESTITUTION;
                    break;
                case 2:
                    atom.pos.y = p.pos.y - h - r;
                    if (atom.vel.y > 0.0f) atom.vel.y = -atom.vel.y * WALL_RESTITUTION;
                    break;
                case 3:
                    atom.pos.y = p.pos.y + h + r;
                    if (atom.vel.y < 0.0f) atom.vel.y = -atom.vel.y * WALL_RESTITUTION;
                    break;
                }
            }
        }
    }
}

// ============================================================
// Столкновения атомов с неразрушимыми бортами (уровень 6).
// AABB (борт) vs. окружность (атом) — та же схема, что и у колонн.
// Без этой функции уран свободно сваливался с колонны вбок:
// ui.barriers передавались только в stepNeutrons, а к атомам
// не применялись.
// ============================================================
void applyBarriersToAtoms(std::vector<Atom>& atoms,
    const std::vector<Barrier>& barriers)
{
    for (auto& atom : atoms) {
        for (const auto& b : barriers) {
            const float hx = b.halfSize.x;
            const float hy = b.halfSize.y;
            const float r = atom.radius;

            // Ближайшая точка AABB к центру атома
            float cX = std::clamp(atom.pos.x, b.pos.x - hx, b.pos.x + hx);
            float cY = std::clamp(atom.pos.y, b.pos.y - hy, b.pos.y + hy);
            float dx = atom.pos.x - cX;
            float dy = atom.pos.y - cY;
            float d2 = dx * dx + dy * dy;

            if (d2 >= r * r) continue;

            if (d2 > 1e-8f) {
                // Центр атома вне AABB — выталкиваем по нормали
                float d = std::sqrt(d2);
                float nx = dx / d;
                float ny = dy / d;
                float overlap = r - d;
                atom.pos.x += nx * overlap;
                atom.pos.y += ny * overlap;

                float vn = atom.vel.x * nx + atom.vel.y * ny;
                if (vn < 0.0f) {
                    atom.vel.x -= (1.0f + WALL_RESTITUTION) * vn * nx;
                    atom.vel.y -= (1.0f + WALL_RESTITUTION) * vn * ny;
                }
            }
            else {
                // Центр внутри AABB — выталкиваем через ближайшую грань
                float left = atom.pos.x - (b.pos.x - hx);
                float right = (b.pos.x + hx) - atom.pos.x;
                float top = atom.pos.y - (b.pos.y - hy);
                float bottom = (b.pos.y + hy) - atom.pos.y;

                float mn = left;
                int face = 0;
                if (right < mn) { mn = right; face = 1; }
                if (top < mn) { mn = top; face = 2; }
                if (bottom < mn) { mn = bottom; face = 3; }

                switch (face) {
                case 0:
                    atom.pos.x = b.pos.x - hx - r;
                    if (atom.vel.x > 0.0f) atom.vel.x = -atom.vel.x * WALL_RESTITUTION;
                    break;
                case 1:
                    atom.pos.x = b.pos.x + hx + r;
                    if (atom.vel.x < 0.0f) atom.vel.x = -atom.vel.x * WALL_RESTITUTION;
                    break;
                case 2:
                    atom.pos.y = b.pos.y - hy - r;
                    if (atom.vel.y > 0.0f) atom.vel.y = -atom.vel.y * WALL_RESTITUTION;
                    break;
                case 3:
                    atom.pos.y = b.pos.y + hy + r;
                    if (atom.vel.y < 0.0f) atom.vel.y = -atom.vel.y * WALL_RESTITUTION;
                    break;
                }
            }
        }
    }
}


// ============================================================
// Готовая молекула H2O
// ============================================================
std::array<Atom, 3> makeWaterMolecule(int baseIndex,
    sf::Vector2f center,
    float rotationRad,
    sf::Vector2f vel)
{
    std::array<Atom, 3> result;

    const float halfAng = WATER_ANGLE_DEG * 0.5f * 3.14159265f / 180.0f;
    const float bLen = getMorsePair(0, 1).re;   // O–H ~ 0.97 Å

    sf::Vector2f h1Local = rotateVec(
        { std::cos(halfAng),  std::sin(halfAng) }, rotationRad) * bLen;
    sf::Vector2f h2Local = rotateVec(
        { std::cos(halfAng), -std::sin(halfAng) }, rotationRad) * bLen;

    Atom o = makeAtom(1, center, vel);
    o.bonds = { baseIndex + 1, baseIndex + 2, -1, -1 };

    Atom h1 = makeAtom(0, center + h1Local, vel);
    h1.bonds = { baseIndex, -1, -1, -1 };

    Atom h2 = makeAtom(0, center + h2Local, vel);
    h2.bonds = { baseIndex, -1, -1, -1 };

    result[0] = o;
    result[1] = h1;
    result[2] = h2;
    return result;
}