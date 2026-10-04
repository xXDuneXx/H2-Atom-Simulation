"use strict";
// ============================================================
// physics.ts — port of Physics.cpp + шага интеграции из AtomSimulation.cpp
// Молекулярная динамика: Морзе, жёсткое ядро, vdW, H-связи (TIP4P),
// угловые силы, термостат Ланжевена, границы, стены, реакция Na+H2O.
// ============================================================
Object.defineProperty(exports, "__esModule", { value: true });
exports.makeOxygen = exports.makeHydrogen = exports.formatTempCelsius = exports.tempSliderPosToCelsius = exports.tempCelsiusToSliderPos = exports.boxSizePosToSize = exports.boxSizeToPos = exports.posToValue = exports.valueToPos = void 0;
exports.rotateVec = rotateVec;
exports.formatSpeed = formatSpeed;
exports.tempCelsiusToPhysics = tempCelsiusToPhysics;
exports.computeInteraction = computeInteraction;
exports.updateBonds = updateBonds;
exports.updateHBonds = updateHBonds;
exports.applyHBondAngularForces = applyHBondAngularForces;
exports.applyWaterAngleForces = applyWaterAngleForces;
exports.applyPeroxideAngleForces = applyPeroxideAngleForces;
exports.applyWallsToAtoms = applyWallsToAtoms;
exports.compactAtomsAfterFission = compactAtomsAfterFission;
exports.applySodiumWaterReaction = applySodiumWaterReaction;
exports.physicsSubStep = physicsSubStep;
const config_1 = require("./config");
const types_1 = require("./types");
const DEG2RAD = Math.PI / 180;
// ---------- вспомогательные ----------
function rotateVec(v, angle) {
    const c = Math.cos(angle), s = Math.sin(angle);
    return { x: v.x * c - v.y * s, y: v.x * s + v.y * c };
}
const valueToPos = (value) => value <= 0 ? 0 : Math.log(value / config_1.SPEED_MIN) / Math.log(config_1.SPEED_MAX / config_1.SPEED_MIN);
exports.valueToPos = valueToPos;
const posToValue = (pos) => config_1.SPEED_MIN * Math.pow(config_1.SPEED_MAX / config_1.SPEED_MIN, (0, types_1.clampNum)(pos, 0, 1));
exports.posToValue = posToValue;
const boxSizeToPos = (size) => (0, types_1.clampNum)((size - config_1.BOX_MIN) / (config_1.BOX_MAX - config_1.BOX_MIN), 0, 1);
exports.boxSizeToPos = boxSizeToPos;
const boxSizePosToSize = (p) => config_1.BOX_MIN + (0, types_1.clampNum)(p, 0, 1) * (config_1.BOX_MAX - config_1.BOX_MIN);
exports.boxSizePosToSize = boxSizePosToSize;
function formatSpeed(v) {
    if (v <= 0)
        return '0x';
    if (v < 1.0)
        return `${v.toFixed(2)}x`;
    if (v < 10.0)
        return `${v.toFixed(1)}x`;
    return `${Math.round(v)}x`;
}
// Трёхзонная карта температура → physTemp
function tempCelsiusToPhysics(c) {
    const ICE_BASE = 0.07; // physTemp при 0°C → лёд прочный
    const MELT_C = 15.0; // конец зоны плавления, °C
    const MELT_TOP = 0.24; // physTemp при MELT_C → лёд расплавлен
    if (c <= 0.0) {
        const s = (0, types_1.clampNum)((c - config_1.TEMP_MIN_C) / (0.0 - config_1.TEMP_MIN_C), 0, 1);
        return s * ICE_BASE;
    }
    if (c <= MELT_C) {
        const s = c / MELT_C;
        return ICE_BASE + s * (MELT_TOP - ICE_BASE);
    }
    const s = (0, types_1.clampNum)((c - MELT_C) / (config_1.TEMP_MAX_C - MELT_C), 0, 1);
    return MELT_TOP + s * (config_1.TEMP_PHYS_MAX - MELT_TOP);
}
const tempCelsiusToSliderPos = (c) => (0, types_1.clampNum)((c - config_1.TEMP_MIN_C) / (config_1.TEMP_MAX_C - config_1.TEMP_MIN_C), 0, 1);
exports.tempCelsiusToSliderPos = tempCelsiusToSliderPos;
const tempSliderPosToCelsius = (p) => config_1.TEMP_MIN_C + (0, types_1.clampNum)(p, 0, 1) * (config_1.TEMP_MAX_C - config_1.TEMP_MIN_C);
exports.tempSliderPosToCelsius = tempSliderPosToCelsius;
const formatTempCelsius = (c) => c.toFixed(2);
exports.formatTempCelsius = formatTempCelsius;
// Morse-сила вдоль линии связи (знак: + растяжение, - сжатие)
function morseForce(mp, r) {
    if (r > mp.cutoff)
        return 0.0;
    const delta = r - mp.re;
    const expTerm = Math.exp(-mp.a * delta);
    let F = -2.0 * mp.De * mp.a * (1.0 - expTerm) * expTerm;
    if (r > mp.cutoff - 0.5)
        F *= (mp.cutoff - r) / 0.5;
    return F;
}
function vdwForce(r, minDist) {
    if (r <= minDist || r >= config_1.VDW_CUTOFF)
        return 0.0;
    const delta = r - config_1.VDW_re;
    const expTerm = Math.exp(-config_1.VDW_a * delta);
    let F_vdw = -2.0 * config_1.VDW_De * config_1.VDW_a * (1.0 - expTerm) * expTerm;
    if (r > config_1.VDW_CUTOFF - 1.0)
        F_vdw *= (config_1.VDW_CUTOFF - r) / 1.0;
    return F_vdw;
}
// ---------- взаимодействие ----------
/** Возвращает силу, действующую на атом j со стороны i (fx,fy). */
function computeInteraction(atoms, i, j) {
    const a1 = atoms[i], a2 = atoms[j];
    const dx = a2.pos.x - a1.pos.x; // a1 → a2
    const dy = a2.pos.y - a1.pos.y;
    const r2 = dx * dx + dy * dy;
    const r = Math.sqrt(r2);
    if (r < 1e-6)
        return { x: 0, y: 0 };
    const max1 = ATOM_TYPES_MAX(a1), max2 = ATOM_TYPES_MAX(a2);
    const n1 = (0, types_1.countBonds)(a1), n2 = (0, types_1.countBonds)(a2);
    const sat1 = n1 >= max1, sat2 = n2 >= max2;
    const mutuallyBonded = (0, types_1.isBondedTo)(a1, j) && (0, types_1.isBondedTo)(a2, i);
    const mutuallyHBonded = (0, types_1.isHBondedTo)(a1, j) && (0, types_1.isHBondedTo)(a2, i);
    let F_total = 0.0;
    // --- Жёсткое ядро (для H–O ослаблено до 4.0) ---
    let hcK = config_1.HARD_CORE_K;
    if ((a1.elementId === 0 && a2.elementId === 1) ||
        (a1.elementId === 1 && a2.elementId === 0)) {
        hcK = 4.0;
    }
    const minDist = a1.radius + a2.radius;
    if (r < minDist)
        F_total += hcK * (minDist - r);
    if (mutuallyBonded) {
        // Ковалентная связь; порядок = сколько раз j записан в слотах i
        let order = 0;
        for (const k of a1.bonds)
            if (k === j)
                order++;
        F_total += morseForce((0, config_1.getMorsePairByOrder)(a1.elementId, a2.elementId, order), r);
    }
    else if (mutuallyHBonded) {
        // H-связь. Донор — H, акцептор — O (через M-сайт) или Cl (напрямую).
        const hIdx = a1.elementId === 0 ? i : j;
        const accIdx = hIdx === i ? j : i;
        const accElem = atoms[accIdx].elementId;
        const h = atoms[hIdx];
        if (accElem === 1) {
            const mPos = (0, types_1.computeTip4pSite)(atoms, accIdx);
            if (!mPos)
                return { x: 0, y: 0 };
            const rMx = mPos.x - h.pos.x, rMy = mPos.y - h.pos.y;
            const rM = Math.hypot(rMx, rMy);
            if (rM < 1e-6)
                return { x: 0, y: 0 };
            let oDonor = -1;
            for (const k of h.bonds)
                if (k >= 0) {
                    oDonor = k;
                    break;
                }
            let angleFactor = 1.0;
            if (oDonor >= 0 && oDonor < atoms.length) {
                const u = { x: h.pos.x - atoms[oDonor].pos.x, y: h.pos.y - atoms[oDonor].pos.y };
                const uLen = Math.hypot(u.x, u.y);
                if (uLen > 1e-6) {
                    const cosT = (0, types_1.clampNum)((u.x * rMx + u.y * rMy) / (uLen * rM), -1, 1);
                    angleFactor = 0.5 * (1.0 + cosT);
                }
            }
            let F = morseForce({ De: config_1.HBOND_De, re: config_1.HBOND_re, a: config_1.HBOND_a, cutoff: config_1.HBOND_CUTOFF }, rM);
            F *= angleFactor;
            let FH_x = -F * rMx / rM, FH_y = -F * rMy / rM;
            let FO_x = F * rMx / rM, FO_y = F * rMy / rM;
            if (r < minDist) {
                const F_hc = config_1.HARD_CORE_K * (minDist - r);
                const sgn = hIdx === i ? 1.0 : -1.0;
                FH_x += F_hc * dx / r * sgn;
                FH_y += F_hc * dy / r * sgn;
                FO_x -= F_hc * dx / r * sgn;
                FO_y -= F_hc * dy / r * sgn;
            }
            let fx = j === accIdx ? FO_x : FH_x;
            let fy = j === accIdx ? FO_y : FH_y;
            const mag = Math.hypot(fx, fy);
            if (mag > config_1.F_MAX) {
                const s = config_1.F_MAX / mag;
                fx *= s;
                fy *= s;
            }
            return { x: fx, y: fy };
        }
        else if (accElem === 2) {
            const acc = atoms[accIdx];
            const rHx = acc.pos.x - h.pos.x, rHy = acc.pos.y - h.pos.y;
            const rH = Math.hypot(rHx, rHy);
            if (rH < 1e-6)
                return { x: 0, y: 0 };
            let donor = -1;
            for (const k of h.bonds)
                if (k >= 0) {
                    donor = k;
                    break;
                }
            let angleFactor = 1.0;
            if (donor >= 0 && donor < atoms.length) {
                const u = { x: h.pos.x - atoms[donor].pos.x, y: h.pos.y - atoms[donor].pos.y };
                const uLen = Math.hypot(u.x, u.y);
                if (uLen > 1e-6) {
                    const cosT = (0, types_1.clampNum)((u.x * rHx + u.y * rHy) / (uLen * rH), -1, 1);
                    angleFactor = 0.5 * (1.0 + cosT);
                }
            }
            let F = morseForce({ De: config_1.HCL_HB_De, re: config_1.HCL_HB_re, a: config_1.HCL_HB_a, cutoff: config_1.HCL_HB_cutoff }, rH);
            F *= angleFactor;
            let FH_x = -F * rHx / rH, FH_y = -F * rHy / rH;
            let FA_x = F * rHx / rH, FA_y = F * rHy / rH;
            if (r < minDist) {
                const F_hc = config_1.HARD_CORE_K * (minDist - r);
                const sgn = hIdx === i ? 1.0 : -1.0;
                FH_x += F_hc * dx / r * sgn;
                FH_y += F_hc * dy / r * sgn;
                FA_x -= F_hc * dx / r * sgn;
                FA_y -= F_hc * dy / r * sgn;
            }
            let fx = j === accIdx ? FA_x : FH_x;
            let fy = j === accIdx ? FA_y : FH_y;
            const mag = Math.hypot(fx, fy);
            if (mag > config_1.F_MAX) {
                const s = config_1.F_MAX / mag;
                fx *= s;
                fy *= s;
            }
            return { x: fx, y: fy };
        }
        return { x: 0, y: 0 }; // неизвестный акцептор
    }
    else if ((a1.elementId === 0 && a2.elementId === 1) ||
        (a1.elementId === 1 && a2.elementId === 0)) {
        // === H-O special case ===
        const hIdx = a1.elementId === 0 ? i : j;
        const oIdx = a1.elementId === 1 ? i : j;
        const hBonds = (0, types_1.countBonds)(atoms[hIdx]);
        let oHBonds = 0;
        for (const k of atoms[oIdx].bonds) {
            if (k >= 0 && atoms[k].elementId === 0)
                oHBonds++;
        }
        let allowAttract = false;
        if (hBonds === 0 && oHBonds < 2) {
            // свободный H + O с местом, но O не должен сидеть на O-O (пероксид)
            let oHasO = false;
            for (const k of atoms[oIdx].bonds) {
                if (k >= 0 && atoms[k].elementId === 1) {
                    oHasO = true;
                    break;
                }
            }
            if (!oHasO)
                allowAttract = true;
        }
        else if (hBonds === 1) {
            let partner = -1;
            for (const k of atoms[hIdx].bonds)
                if (k >= 0) {
                    partner = k;
                    break;
                }
            if (partner >= 0 && atoms[partner].elementId === 2 && oHBonds < 3) {
                allowAttract = true; // H из HCl → тянем к O (диссоциация кислоты)
            }
        }
        if (allowAttract)
            F_total += morseForce((0, config_1.getMorsePair)(0, 1), r);
    }
    else if ((a1.elementId === 0 && a2.elementId === 2) ||
        (a1.elementId === 2 && a2.elementId === 0)) {
        // === H-Cl, не связанные ковалентно ===
        const hIdx = a1.elementId === 0 ? i : j;
        const clIdx = a1.elementId === 2 ? i : j;
        let hPartner = -1;
        for (const k of atoms[hIdx].bonds)
            if (k >= 0) {
                hPartner = k;
                break;
            }
        let clPartner = -1;
        for (const k of atoms[clIdx].bonds)
            if (k >= 0) {
                clPartner = k;
                break;
            }
        const hInHCl = hPartner >= 0 && atoms[hPartner].elementId === 2;
        const clInHCl = clPartner >= 0 && atoms[clPartner].elementId === 0;
        const differentMolecules = hPartner !== clIdx && clPartner !== hIdx;
        if (hInHCl && clInHCl && differentMolecules) {
            // Водородная связь H···Cl между двумя РАЗНЫМИ молекулами HCl (HCl-лёд)
            const dirX = atoms[clIdx].pos.x - atoms[hIdx].pos.x;
            const dirY = atoms[clIdx].pos.y - atoms[hIdx].pos.y;
            const dLen = Math.hypot(dirX, dirY);
            if (dLen < 1e-6)
                return { x: 0, y: 0 };
            let angleFactor = 1.0;
            const u = { x: atoms[hIdx].pos.x - atoms[hPartner].pos.x, y: atoms[hIdx].pos.y - atoms[hPartner].pos.y };
            const uLen = Math.hypot(u.x, u.y);
            if (uLen > 1e-6) {
                const cosT = (0, types_1.clampNum)((u.x * dirX + u.y * dirY) / (uLen * dLen), -1, 1);
                angleFactor = 0.5 * (1.0 + cosT);
            }
            F_total += morseForce({ De: config_1.HCL_HB_De, re: config_1.HCL_HB_re, a: config_1.HCL_HB_a, cutoff: config_1.HCL_HB_cutoff }, r) * angleFactor;
        }
    }
    else if (a1.elementId === 4 || a2.elementId === 4) {
        // === Натрий (несвязанный) ===
        const otherElem = a1.elementId === 4 ? a2.elementId : a1.elementId;
        if (otherElem === 1) {
            F_total += morseForce({ De: 1.5, re: 2.0, a: 1.5, cutoff: 3.5 }, r); // Na···O
        }
        else if (otherElem === 2) {
            F_total += morseForce({ De: config_1.NACL_De, re: config_1.NACL_re, a: config_1.NACL_a, cutoff: config_1.NACL_cutoff }, r); // Na···Cl
        }
        else {
            F_total += vdwForce(r, minDist);
        }
    }
    else if (!sat1 && !sat2) {
        const e1 = a1.elementId, e2 = a2.elementId;
        if (e1 === 1 && e2 === 1) {
            // === O-O пара ===
            const nb1 = n1, nb2 = n2;
            const canFormOO = (nb1 === 0 && nb2 === 0) || (nb1 === 1 && nb2 === 1);
            if (canFormOO) {
                const mp = (nb1 === 0 && nb2 === 0)
                    ? (0, config_1.getMorsePair)(1, 1) // будет двойная O=O
                    : { De: 1.0, re: 1.48, a: 2.0, cutoff: 4.0 }; // два HO· → одинарная, мягкий захват
                F_total += morseForce(mp, r);
            }
            else {
                F_total += vdwForce(r, minDist);
            }
        }
        else {
            F_total += morseForce((0, config_1.getMorsePair)(e1, e2), r);
        }
    }
    else if (sat1 && sat2) {
        // === H···Cl между двумя разными молекулами HCl (sat-sat) ===
        const pairIsHCl = (a1.elementId === 0 && a2.elementId === 2) || (a1.elementId === 2 && a2.elementId === 0);
        if (pairIsHCl) {
            const hIdx = a1.elementId === 0 ? i : j;
            const clIdx = a1.elementId === 2 ? i : j;
            let hPartner = -1;
            for (const k of atoms[hIdx].bonds)
                if (k >= 0) {
                    hPartner = k;
                    break;
                }
            let clPartner = -1;
            for (const k of atoms[clIdx].bonds)
                if (k >= 0) {
                    clPartner = k;
                    break;
                }
            const hIsFromHCl = hPartner >= 0 && atoms[hPartner].elementId === 2;
            const clIsFromHCl = clPartner >= 0 && atoms[clPartner].elementId === 0;
            const differentMolecules = hPartner !== clIdx && clPartner !== hIdx;
            if (hIsFromHCl && clIsFromHCl && differentMolecules) {
                let angleFactor = 1.0;
                if (hPartner >= 0) {
                    const u = { x: atoms[hIdx].pos.x - atoms[hPartner].pos.x, y: atoms[hIdx].pos.y - atoms[hPartner].pos.y };
                    const uLen = Math.hypot(u.x, u.y);
                    if (uLen > 1e-6) {
                        const cosT = (0, types_1.clampNum)((u.x * dx + u.y * dy) / (uLen * r), -1, 1);
                        angleFactor = 0.5 * (1.0 + cosT);
                    }
                }
                F_total += morseForce({ De: config_1.HCL_HB_De, re: config_1.HCL_HB_re, a: config_1.HCL_HB_a, cutoff: config_1.HCL_HB_cutoff }, r) * angleFactor;
            }
            else {
                F_total += vdwForce(r, minDist);
            }
        }
        else {
            F_total += vdwForce(r, minDist);
        }
    }
    if (Math.abs(F_total) > config_1.F_MAX)
        F_total = Math.sign(F_total) * config_1.F_MAX;
    return { x: F_total * dx / r, y: F_total * dy / r };
}
const ATOM_TYPES_MAX = (a) => types_1.ATOM_TYPES[a.elementId]?.maxBonds ?? 0;
// ---------- связи ----------
function updateBonds(atoms, grid) {
    const n = atoms.length;
    // 1) Разрыв / трансформация связей
    for (let i = 0; i < n; ++i) {
        for (let slot = 0; slot < 4; ++slot) {
            const j = atoms[i].bonds[slot];
            if (j < 0 || j <= i)
                continue;
            if (j >= n || !(0, types_1.isBondedTo)(atoms[j], i)) {
                atoms[i].bonds[slot] = -1;
                continue;
            }
            const e1 = atoms[i].elementId, e2 = atoms[j].elementId;
            let breakIt = false;
            let handled = false;
            // a) растянуто
            const bd = (0, config_1.getBondDists)(e1, e2);
            const brk2 = bd.brk * bd.brk;
            const dx = atoms[j].pos.x - atoms[i].pos.x;
            const dy = atoms[j].pos.y - atoms[i].pos.y;
            if (dx * dx + dy * dy > brk2)
                breakIt = true;
            // b) HCl-диссоциация: атомарный перенос H на O рядом
            if (!breakIt && ((e1 === 0 && e2 === 2) || (e1 === 2 && e2 === 0))) {
                const hIdx = e1 === 0 ? i : j;
                const clIdx = e1 === 2 ? i : j;
                const h = atoms[hIdx];
                const cx = grid.cellX(h.pos.x), cy = grid.cellY(h.pos.y);
                let oTarget = -1;
                outer: for (let ddy = -1; ddy <= 1; ++ddy) {
                    for (let ddx = -1; ddx <= 1; ++ddx) {
                        const nx = cx + ddx, ny = cy + ddy;
                        if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows())
                            continue;
                        for (const k of grid.at(nx, ny)) {
                            if (k === hIdx || k === clIdx)
                                continue;
                            if (atoms[k].elementId !== 1)
                                continue;
                            if ((0, types_1.countBonds)(atoms[k]) >= 3)
                                continue;
                            const dxo = atoms[k].pos.x - h.pos.x;
                            const dyo = atoms[k].pos.y - h.pos.y;
                            if (dxo * dxo + dyo * dyo < config_1.HCL_DISSOC_DIST * config_1.HCL_DISSOC_DIST) {
                                oTarget = k;
                                break outer;
                            }
                        }
                    }
                }
                if (oTarget >= 0) {
                    (0, types_1.removeAllBondsTo)(atoms[hIdx], clIdx);
                    (0, types_1.removeAllBondsTo)(atoms[clIdx], hIdx);
                    (0, types_1.addBond)(atoms[hIdx], oTarget);
                    (0, types_1.addBond)(atoms[oTarget], hIdx);
                    handled = true;
                }
            }
            if (!handled && breakIt) {
                (0, types_1.removeAllBondsTo)(atoms[i], j);
                (0, types_1.removeAllBondsTo)(atoms[j], i);
            }
        }
    }
    // 2) Образование новых связей
    for (let i = 0; i < n; ++i) {
        if (!(0, types_1.hasFreeBond)(atoms[i]))
            continue;
        const cx = grid.cellX(atoms[i].pos.x), cy = grid.cellY(atoms[i].pos.y);
        for (let dy = -1; dy <= 1; ++dy) {
            for (let dx = -1; dx <= 1; ++dx) {
                const nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows())
                    continue;
                for (const j of grid.at(nx, ny)) {
                    if (j <= i)
                        continue;
                    if (!(0, types_1.hasFreeBond)(atoms[i]) || !(0, types_1.hasFreeBond)(atoms[j]))
                        continue;
                    if ((0, types_1.isBondedTo)(atoms[i], j))
                        continue;
                    const e1 = atoms[i].elementId, e2 = atoms[j].elementId;
                    const bd = (0, config_1.getBondDists)(e1, e2);
                    const form2 = bd.form * bd.form;
                    const ddx = atoms[j].pos.x - atoms[i].pos.x;
                    const ddy = atoms[j].pos.y - atoms[i].pos.y;
                    if (ddx * ddx + ddy * ddy >= form2)
                        continue;
                    if (e1 === 1 && e2 === 1) {
                        const nb1 = (0, types_1.countBonds)(atoms[i]), nb2 = (0, types_1.countBonds)(atoms[j]);
                        if (nb1 === 0 && nb2 === 0) {
                            // O=O двойная
                            (0, types_1.addBond)(atoms[i], j);
                            (0, types_1.addBond)(atoms[i], j);
                            (0, types_1.addBond)(atoms[j], i);
                            (0, types_1.addBond)(atoms[j], i);
                        }
                        else if (nb1 === 1 && nb2 === 1) {
                            // O-O одинарная (H2O2)
                            (0, types_1.addBond)(atoms[i], j);
                            (0, types_1.addBond)(atoms[j], i);
                        }
                    }
                    else if ((e1 === 1 && e2 === 2) || (e1 === 2 && e2 === 1)) {
                        continue; // O-Cl запрещено
                    }
                    else if ((e1 === 0 && e2 === 2) || (e1 === 2 && e2 === 0)) {
                        // H-Cl: не создаём рядом с O со свободным слотом
                        const hIdx = e1 === 0 ? i : j;
                        const h = atoms[hIdx];
                        let suppress = false;
                        const hcx = grid.cellX(h.pos.x), hcy = grid.cellY(h.pos.y);
                        sup: for (let d2 = -1; d2 <= 1 && !suppress; ++d2) {
                            for (let d1 = -1; d1 <= 1 && !suppress; ++d1) {
                                const hnx = hcx + d1, hny = hcy + d2;
                                if (hnx < 0 || hnx >= grid.cols() || hny < 0 || hny >= grid.rows())
                                    continue;
                                for (const k of grid.at(hnx, hny)) {
                                    if (k === hIdx)
                                        continue;
                                    if (atoms[k].elementId !== 1)
                                        continue;
                                    if (!(0, types_1.hasFreeBond)(atoms[k]))
                                        continue;
                                    const dxo = atoms[k].pos.x - h.pos.x;
                                    const dyo = atoms[k].pos.y - h.pos.y;
                                    if (dxo * dxo + dyo * dyo < config_1.HCL_DISSOC_DIST * config_1.HCL_DISSOC_DIST) {
                                        suppress = true;
                                        break sup;
                                    }
                                }
                            }
                        }
                        if (suppress)
                            continue;
                        (0, types_1.addBond)(atoms[i], j);
                        (0, types_1.addBond)(atoms[j], i);
                    }
                    else if (e1 === 4 || e2 === 4) {
                        // Na: только ионная Na-Cl
                        if ((e1 === 4 && e2 === 2) || (e1 === 2 && e2 === 4)) {
                            (0, types_1.addBond)(atoms[i], j);
                            (0, types_1.addBond)(atoms[j], i);
                        }
                    }
                    else if (e1 === 2 && e2 === 2) {
                        // Cl-Cl: не образуем, если рядом есть O (вода/H3O)
                        let oNearby = false;
                        const ccx = grid.cellX(atoms[i].pos.x), ccy = grid.cellY(atoms[i].pos.y);
                        near: for (let d2 = -1; d2 <= 1 && !oNearby; ++d2) {
                            for (let d1 = -1; d1 <= 1 && !oNearby; ++d1) {
                                const hnx = ccx + d1, hny = ccy + d2;
                                if (hnx < 0 || hnx >= grid.cols() || hny < 0 || hny >= grid.rows())
                                    continue;
                                for (const k of grid.at(hnx, hny)) {
                                    if (k === i || k === j)
                                        continue;
                                    if (atoms[k].elementId !== 1)
                                        continue;
                                    const dxo = atoms[k].pos.x - atoms[i].pos.x;
                                    const dyo = atoms[k].pos.y - atoms[i].pos.y;
                                    if (dxo * dxo + dyo * dyo < config_1.VDW_CUTOFF * config_1.VDW_CUTOFF) {
                                        oNearby = true;
                                        break near;
                                    }
                                }
                            }
                        }
                        if (oNearby)
                            continue;
                        (0, types_1.addBond)(atoms[i], j);
                        (0, types_1.addBond)(atoms[j], i);
                    }
                    else {
                        // H-O и H-H
                        if ((e1 === 0 && e2 === 1) || (e1 === 1 && e2 === 0)) {
                            // Запрет H-O: у O уже 2 H, или у O есть O-партнёр
                            const oIdx = e1 === 1 ? i : j;
                            let hCount = 0, oCount = 0;
                            for (const k of atoms[oIdx].bonds) {
                                if (k < 0)
                                    continue;
                                if (atoms[k].elementId === 0)
                                    hCount++;
                                else if (atoms[k].elementId === 1)
                                    oCount++;
                            }
                            if (hCount >= 2)
                                continue;
                            if (oCount > 0)
                                continue;
                        }
                        (0, types_1.addBond)(atoms[i], j);
                        (0, types_1.addBond)(atoms[j], i);
                    }
                }
            }
        }
    }
}
function updateHBonds(atoms, grid, physTemp, rng) {
    const n = atoms.length;
    // Температурные пороги разрыва/образования
    let breakProbWater = 0.0;
    if (physTemp > config_1.HBOND_BREAK_TEMP_MIN) {
        breakProbWater = (0, types_1.clampNum)((physTemp - config_1.HBOND_BREAK_TEMP_MIN) / (config_1.HBOND_BREAK_TEMP_MAX - config_1.HBOND_BREAK_TEMP_MIN), 0, 1);
    }
    let breakProbHCl = 0.0;
    if (physTemp > config_1.HCL_HB_BREAK_TEMP_MIN) {
        breakProbHCl = (0, types_1.clampNum)((physTemp - config_1.HCL_HB_BREAK_TEMP_MIN) / (config_1.HCL_HB_BREAK_TEMP_MAX - config_1.HCL_HB_BREAK_TEMP_MIN), 0, 1);
    }
    const canFormWater = physTemp < config_1.HBOND_FORM_TEMP_MAX;
    const canFormHCl = physTemp < config_1.HCL_HBOND_FORM_TEMP_MAX;
    // 1) Разрыв H-связей (геометрия + термический шум)
    const isHDonor = (a) => a.elementId === 0 && (0, types_1.countBonds)(a) === 1;
    const isAcceptor = (a) => {
        if (a.elementId === 1) {
            const nb = (0, types_1.countBonds)(a);
            return nb >= 2 && nb <= 3;
        }
        if (a.elementId === 2)
            return true;
        return false;
    };
    for (let i = 0; i < n; ++i) {
        for (let slot = 0; slot < 4; ++slot) {
            const j = atoms[i].hbonds[slot];
            if (j < 0 || j <= i)
                continue;
            if (j >= n || !(0, types_1.isHBondedTo)(atoms[j], i)) {
                atoms[i].hbonds[slot] = -1;
                continue;
            }
            const pairOk = (isHDonor(atoms[i]) && isAcceptor(atoms[j])) ||
                (isAcceptor(atoms[i]) && isHDonor(atoms[j]));
            if (!pairOk) {
                (0, types_1.removeAllHBondsTo)(atoms[i], j);
                (0, types_1.removeAllHBondsTo)(atoms[j], i);
                continue;
            }
            const isHClPair = (atoms[i].elementId === 0 && atoms[j].elementId === 2) ||
                (atoms[i].elementId === 2 && atoms[j].elementId === 0);
            const brkDist = isHClPair ? config_1.HCL_HB_BREAK_DIST : config_1.HBOND_BREAK_DIST;
            const dx = atoms[j].pos.x - atoms[i].pos.x;
            const dy = atoms[j].pos.y - atoms[i].pos.y;
            if (dx * dx + dy * dy > brkDist * brkDist) {
                (0, types_1.removeAllHBondsTo)(atoms[i], j);
                (0, types_1.removeAllHBondsTo)(atoms[j], i);
                continue;
            }
            const breakProb = isHClPair ? breakProbHCl : breakProbWater;
            if (breakProb > 0 && rng.next() < breakProb) {
                (0, types_1.removeAllHBondsTo)(atoms[i], j);
                (0, types_1.removeAllHBondsTo)(atoms[j], i);
            }
        }
    }
    // 2) Образование H-связей воды (акцептор — O через M-сайт)
    if (canFormWater) {
        for (let i = 0; i < n; ++i) {
            if (atoms[i].elementId !== 0)
                continue;
            if ((0, types_1.countBonds)(atoms[i]) !== 1)
                continue;
            if ((0, types_1.countHBonds)(atoms[i]) > 0)
                continue;
            let oDonor = -1;
            for (const k of atoms[i].bonds)
                if (k >= 0) {
                    oDonor = k;
                    break;
                }
            if (oDonor < 0 || oDonor >= n)
                continue;
            if (atoms[oDonor].elementId !== 1)
                continue;
            if ((0, types_1.countBonds)(atoms[oDonor]) < 2)
                continue;
            const cx = grid.cellX(atoms[i].pos.x), cy = grid.cellY(atoms[i].pos.y);
            for (let dy = -1; dy <= 1; ++dy) {
                for (let dx = -1; dx <= 1; ++dx) {
                    const nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows())
                        continue;
                    for (const j of grid.at(nx, ny)) {
                        if (j === i || j === oDonor)
                            continue;
                        if (atoms[j].elementId !== 1)
                            continue;
                        if ((0, types_1.countBonds)(atoms[j]) < 2)
                            continue;
                        if ((0, types_1.countHBonds)(atoms[j]) >= config_1.HBOND_SLOTS_O)
                            continue;
                        let reverseExists = false;
                        for (const hk of atoms[j].bonds) {
                            if (hk < 0 || hk >= n)
                                continue;
                            if (atoms[hk].elementId !== 0)
                                continue;
                            if ((0, types_1.isHBondedTo)(atoms[hk], oDonor)) {
                                reverseExists = true;
                                break;
                            }
                        }
                        if (reverseExists)
                            continue;
                        const mPos = (0, types_1.computeTip4pSite)(atoms, j);
                        if (!mPos)
                            continue;
                        const ddx = mPos.x - atoms[i].pos.x;
                        const ddy = mPos.y - atoms[i].pos.y;
                        const r2 = ddx * ddx + ddy * ddy;
                        if (r2 > config_1.HBOND_FORM_DIST * config_1.HBOND_FORM_DIST)
                            continue;
                        const rH = Math.sqrt(r2);
                        if (rH < 1e-6)
                            continue;
                        const v1 = { x: atoms[i].pos.x - atoms[oDonor].pos.x, y: atoms[i].pos.y - atoms[oDonor].pos.y };
                        const l1 = Math.hypot(v1.x, v1.y);
                        if (l1 < 1e-6)
                            continue;
                        const cosT = (v1.x * ddx + v1.y * ddy) / (l1 * rH);
                        if (cosT < config_1.HBOND_ANGLE_COS_MIN)
                            continue;
                        let angularConflict = false;
                        for (const existing of atoms[j].hbonds) {
                            if (existing < 0 || existing >= n)
                                continue;
                            const dE = { x: atoms[existing].pos.x - atoms[j].pos.x, y: atoms[existing].pos.y - atoms[j].pos.y };
                            const LE = Math.hypot(dE.x, dE.y);
                            if (LE < 1e-6)
                                continue;
                            const cosBt = (-ddx / rH) * (dE.x / LE) + (-ddy / rH) * (dE.y / LE);
                            if (cosBt > 0.5) {
                                angularConflict = true;
                                break;
                            }
                        }
                        if (angularConflict)
                            continue;
                        (0, types_1.addHBond)(atoms[i], j);
                        (0, types_1.addHBond)(atoms[j], i);
                    }
                }
            }
        }
    }
    // 3) Образование H-связей HCl (акцептор — Cl напрямую)
    if (canFormHCl) {
        for (let i = 0; i < n; ++i) {
            if (atoms[i].elementId !== 0)
                continue;
            if ((0, types_1.countBonds)(atoms[i]) !== 1)
                continue;
            if ((0, types_1.countHBonds)(atoms[i]) > 0)
                continue;
            let donor = -1;
            for (const k of atoms[i].bonds)
                if (k >= 0) {
                    donor = k;
                    break;
                }
            if (donor < 0 || donor >= n)
                continue;
            const donorElem = atoms[donor].elementId;
            if (donorElem !== 1 && donorElem !== 2)
                continue;
            const cx = grid.cellX(atoms[i].pos.x), cy = grid.cellY(atoms[i].pos.y);
            for (let dy = -1; dy <= 1; ++dy) {
                for (let dx = -1; dx <= 1; ++dx) {
                    const nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows())
                        continue;
                    for (const j of grid.at(nx, ny)) {
                        if (j === i || j === donor)
                            continue;
                        if (atoms[j].elementId !== 2)
                            continue;
                        if ((0, types_1.countHBonds)(atoms[j]) >= config_1.HCL_HB_SLOTS_CL)
                            continue;
                        let clClBonded = false;
                        for (const k of atoms[j].bonds) {
                            if (k < 0)
                                continue;
                            if (atoms[k].elementId === 2) {
                                clClBonded = true;
                                break;
                            }
                        }
                        if (clClBonded)
                            continue;
                        const ddx = atoms[j].pos.x - atoms[i].pos.x;
                        const ddy = atoms[j].pos.y - atoms[i].pos.y;
                        const r2 = ddx * ddx + ddy * ddy;
                        if (r2 > config_1.HCL_HB_FORM_DIST * config_1.HCL_HB_FORM_DIST)
                            continue;
                        const rH = Math.sqrt(r2);
                        if (rH < 1e-6)
                            continue;
                        const v1 = { x: atoms[i].pos.x - atoms[donor].pos.x, y: atoms[i].pos.y - atoms[donor].pos.y };
                        const l1 = Math.hypot(v1.x, v1.y);
                        if (l1 < 1e-6)
                            continue;
                        const cosT = (v1.x * ddx + v1.y * ddy) / (l1 * rH);
                        if (cosT < config_1.HCL_HB_ANGLE_COS_MIN)
                            continue;
                        (0, types_1.addHBond)(atoms[i], j);
                        (0, types_1.addHBond)(atoms[j], i);
                    }
                }
            }
        }
    }
}
// ---------- угловые силы ----------
function applyHBondAngularForces(atoms, forces) {
    const n = atoms.length;
    for (let i = 0; i < n; ++i) {
        if (atoms[i].elementId !== 0)
            continue;
        let donor = -1;
        for (const k of atoms[i].bonds)
            if (k >= 0) {
                donor = k;
                break;
            }
        if (donor < 0 || donor >= n)
            continue;
        const u = { x: atoms[i].pos.x - atoms[donor].pos.x, y: atoms[i].pos.y - atoms[donor].pos.y };
        const uLen2 = u.x * u.x + u.y * u.y;
        if (uLen2 < 1e-8)
            continue;
        const uLen = Math.sqrt(uLen2);
        const uHat = { x: u.x / uLen, y: u.y / uLen };
        for (const j of atoms[i].hbonds) {
            if (j < 0 || j >= n)
                continue;
            const accElem = atoms[j].elementId;
            if (accElem !== 1 && accElem !== 2)
                continue;
            if (!(0, types_1.isHBondedTo)(atoms[j], i))
                continue;
            const w = { x: atoms[j].pos.x - atoms[i].pos.x, y: atoms[j].pos.y - atoms[i].pos.y };
            const along = w.x * uHat.x + w.y * uHat.y;
            const perp = { x: w.x - uHat.x * along, y: w.y - uHat.y * along };
            let Fax = -config_1.HBOND_K_ANG * perp.x, Fay = -config_1.HBOND_K_ANG * perp.y;
            const m = Math.hypot(Fax, Fay);
            if (m > config_1.ANGLE_FORCE_MAX) {
                const sc = config_1.ANGLE_FORCE_MAX / m;
                Fax *= sc;
                Fay *= sc;
            }
            forces[j].x += Fax;
            forces[j].y += Fay;
            forces[i].x -= Fax;
            forces[i].y -= Fay;
        }
    }
}
// Общая угловая пружина для трёх атомов A—B—C (угол при B)
function applyAngleSpring(atoms, forces, aIdx, bIdx, cIdx, theta0, K) {
    const A = atoms[aIdx], B = atoms[bIdx], C = atoms[cIdx];
    const u1 = { x: A.pos.x - B.pos.x, y: A.pos.y - B.pos.y };
    const u2 = { x: C.pos.x - B.pos.x, y: C.pos.y - B.pos.y };
    const L1 = Math.hypot(u1.x, u1.y), L2 = Math.hypot(u2.x, u2.y);
    if (L1 < 1e-6 || L2 < 1e-6)
        return;
    const e1 = { x: u1.x / L1, y: u1.y / L1 };
    const e2 = { x: u2.x / L2, y: u2.y / L2 };
    const c = (0, types_1.clampNum)(e1.x * e2.x + e1.y * e2.y, -1, 1);
    const theta = Math.acos(c);
    const s = Math.sin(theta);
    if (Math.abs(s) < 1e-4)
        return;
    const dTheta = theta - theta0;
    const common = 2.0 * K * dTheta / s;
    const F1 = { x: (common / L1) * (e2.x - c * e1.x), y: (common / L1) * (e2.y - c * e1.y) };
    const F2 = { x: (common / L2) * (e1.x - c * e2.x), y: (common / L2) * (e1.y - c * e2.y) };
    const F0 = { x: -(F1.x + F2.x), y: -(F1.y + F2.y) };
    const maxM = Math.max(Math.hypot(F1.x, F1.y), Math.hypot(F2.x, F2.y), Math.hypot(F0.x, F0.y));
    if (maxM > config_1.ANGLE_FORCE_MAX) {
        const scale = config_1.ANGLE_FORCE_MAX / maxM;
        F1.x *= scale;
        F1.y *= scale;
        F2.x *= scale;
        F2.y *= scale;
        F0.x *= scale;
        F0.y *= scale;
    }
    forces[aIdx].x += F1.x;
    forces[aIdx].y += F1.y;
    forces[cIdx].x += F2.x;
    forces[cIdx].y += F2.y;
    forces[bIdx].x += F0.x;
    forces[bIdx].y += F0.y;
}
function applyWaterAngleForces(atoms, forces) {
    const n = atoms.length;
    const theta0 = config_1.WATER_ANGLE_DEG * DEG2RAD;
    for (let i = 0; i < n; ++i) {
        if (atoms[i].elementId !== 1)
            continue;
        let h1 = -1, h2 = -1;
        for (const k of atoms[i].bonds) {
            if (k < 0 || k >= n)
                continue;
            if (atoms[k].elementId !== 0)
                continue;
            if (h1 < 0)
                h1 = k;
            else if (h2 < 0) {
                h2 = k;
                break;
            }
        }
        if (h1 < 0 || h2 < 0 || h1 === h2)
            continue;
        applyAngleSpring(atoms, forces, h1, i, h2, theta0, config_1.WATER_ANGLE_K);
    }
}
function applyPeroxideAngleForces(atoms, forces) {
    const n = atoms.length;
    const theta0 = config_1.PEROXIDE_ANGLE_DEG * DEG2RAD;
    for (let i = 0; i < n; ++i) {
        if (atoms[i].elementId !== 1)
            continue;
        let hIdx = -1, oIdx = -1, nBonds = 0;
        for (const k of atoms[i].bonds) {
            if (k < 0 || k >= n)
                continue;
            nBonds++;
            if (atoms[k].elementId === 0)
                hIdx = k;
            else if (atoms[k].elementId === 1)
                oIdx = k;
        }
        if (nBonds !== 2)
            continue;
        if (hIdx < 0 || oIdx < 0)
            continue;
        let partnerHasH = false, partnerHasO = false;
        for (const k of atoms[oIdx].bonds) {
            if (k < 0 || k >= n)
                continue;
            if (atoms[k].elementId === 0)
                partnerHasH = true;
            else if (atoms[k].elementId === 1)
                partnerHasO = true;
        }
        if (!partnerHasH || !partnerHasO)
            continue;
        // угол H—O(i)—O: центр угла — кислород i
        applyAngleSpring(atoms, forces, hIdx, i, oIdx, theta0, config_1.PEROXIDE_ANGLE_K);
    }
}
// ---------- стены ----------
function applyWallsToAtoms(atoms, walls) {
    const halfT = config_1.WALL_THICKNESS * 0.5;
    for (const atom of atoms) {
        for (const wall of walls) {
            const ab = { x: wall.b.x - wall.a.x, y: wall.b.y - wall.a.y };
            const ab2 = ab.x * ab.x + ab.y * ab.y;
            if (ab2 < 1e-8)
                continue;
            const ap = { x: atom.pos.x - wall.a.x, y: atom.pos.y - wall.a.y };
            const t = (0, types_1.clampNum)((ap.x * ab.x + ap.y * ab.y) / ab2, 0, 1);
            const closest = { x: wall.a.x + t * ab.x, y: wall.a.y + t * ab.y };
            const diff = { x: atom.pos.x - closest.x, y: atom.pos.y - closest.y };
            const d2 = diff.x * diff.x + diff.y * diff.y;
            const rr = atom.radius + halfT;
            if (d2 >= rr * rr)
                continue;
            if (d2 < 1e-8) {
                const len = Math.sqrt(ab2);
                const nx = -ab.y / len, ny = ab.x / len;
                atom.pos.x = closest.x + nx * rr;
                atom.pos.y = closest.y + ny * rr;
                continue;
            }
            const d = Math.sqrt(d2);
            const nx = diff.x / d, ny = diff.y / d;
            atom.pos.x = closest.x + nx * rr;
            atom.pos.y = closest.y + ny * rr;
            const vn = atom.vel.x * nx + atom.vel.y * ny;
            if (vn < 0) {
                atom.vel.x -= (1.0 + config_1.WALL_RESTITUTION) * vn * nx;
                atom.vel.y -= (1.0 + config_1.WALL_RESTITUTION) * vn * ny;
            }
        }
    }
}
// ---------- компактизация ----------
function compactAtomsAfterFission(atoms) {
    let anyDead = false;
    for (const a of atoms)
        if (a.elementId < 0) {
            anyDead = true;
            break;
        }
    if (!anyDead)
        return atoms;
    const remap = new Array(atoms.length).fill(-1);
    let newIdx = 0;
    for (let k = 0; k < atoms.length; ++k) {
        if (atoms[k].elementId >= 0)
            remap[k] = newIdx++;
    }
    const result = [];
    for (const a of atoms) {
        if (a.elementId < 0)
            continue;
        for (let s = 0; s < 4; ++s) {
            if (a.bonds[s] >= 0)
                a.bonds[s] = a.bonds[s] < remap.length ? remap[a.bonds[s]] : -1;
            if (a.hbonds[s] >= 0)
                a.hbonds[s] = a.hbonds[s] < remap.length ? remap[a.hbonds[s]] : -1;
        }
        result.push(a);
    }
    return result;
}
// ---------- реакция натрия с водой ----------
function applySodiumWaterReaction(atomsIn) {
    let atoms = atomsIn;
    let anyDead = false;
    for (let i = 0; i < atoms.length; ++i) {
        if (atoms[i].elementId !== 4)
            continue; // Na
        if ((0, types_1.countBonds)(atoms[i]) > 0)
            continue; // NaCl нейтрализован
        const naPos = atoms[i].pos;
        // Ближайшая вода (O с двумя H) в радиусе реакции
        let waterO = -1;
        let bestD2 = config_1.NA_WATER_REACT_DIST * config_1.NA_WATER_REACT_DIST;
        for (let j = 0; j < atoms.length; ++j) {
            if (j === i)
                continue;
            if (atoms[j].elementId !== 1)
                continue;
            let hCount = 0;
            for (const k of atoms[j].bonds)
                if (k >= 0 && atoms[k].elementId === 0)
                    hCount++;
            if (hCount < 2)
                continue;
            const dx = atoms[j].pos.x - naPos.x;
            const dy = atoms[j].pos.y - naPos.y;
            const d2 = dx * dx + dy * dy;
            if (d2 < bestD2) {
                bestD2 = d2;
                waterO = j;
            }
        }
        if (waterO < 0)
            continue;
        // --- ВЗРЫВ ---
        // 1) Рвём O–H связи воды, разгоняем H радиально
        for (let slot = 0; slot < 4; ++slot) {
            const k = atoms[waterO].bonds[slot];
            if (k < 0 || atoms[k].elementId !== 0)
                continue;
            (0, types_1.removeAllBondsTo)(atoms[waterO], k);
            (0, types_1.removeAllBondsTo)(atoms[k], waterO);
            for (const a of atoms) {
                (0, types_1.removeAllHBondsTo)(a, k);
                (0, types_1.removeAllHBondsTo)(a, waterO);
            }
            const dx = atoms[k].pos.x - naPos.x;
            const dy = atoms[k].pos.y - naPos.y;
            const L = Math.hypot(dx, dy);
            if (L > 1e-6) {
                atoms[k].vel.x += dx / L * config_1.NA_EXPLOSION_KICK;
                atoms[k].vel.y += dy / L * config_1.NA_EXPLOSION_KICK;
            }
        }
        // 2) Ударная волна
        const R2 = config_1.NA_EXPLOSION_RADIUS * config_1.NA_EXPLOSION_RADIUS;
        for (let j = 0; j < atoms.length; ++j) {
            if (j === i)
                continue;
            const dx = atoms[j].pos.x - naPos.x;
            const dy = atoms[j].pos.y - naPos.y;
            const r2 = dx * dx + dy * dy;
            if (r2 > R2)
                continue;
            const L = Math.sqrt(r2);
            if (L < 1e-6)
                continue;
            const falloff = 1.0 - L / config_1.NA_EXPLOSION_RADIUS;
            atoms[j].vel.x += dx / L * config_1.NA_EXPLOSION_KICK * falloff * 0.6;
            atoms[j].vel.y += dy / L * config_1.NA_EXPLOSION_KICK * falloff * 0.6;
        }
        // 3) Поглощаем Na
        atoms[i].elementId = -1;
        anyDead = true;
    }
    return anyDead ? compactAtomsAfterFission(atoms) : atoms;
}
// ---------- фабрики ----------
const makeHydrogen = (pos, vel) => (0, types_1.makeAtom)(0, pos, vel);
exports.makeHydrogen = makeHydrogen;
const makeOxygen = (pos, vel) => (0, types_1.makeAtom)(1, pos, vel);
exports.makeOxygen = makeOxygen;
function physicsSubStep(world, p, stepNeutronsCb) {
    const { grid, rng } = world;
    let atoms = world.atoms;
    // Сетка под текущие позиции
    grid.clear();
    for (let k = 0; k < atoms.length; ++k)
        grid.insert(k, atoms[k].pos);
    updateBonds(atoms, grid);
    updateHBonds(atoms, grid, p.physTemp, rng);
    // Na + вода может удалить атомы → перестроить сетку
    const before = atoms.length;
    atoms = applySodiumWaterReaction(atoms);
    if (atoms.length !== before) {
        world.atoms = atoms;
        grid.clear();
        for (let k = 0; k < atoms.length; ++k)
            grid.insert(k, atoms[k].pos);
    }
    const forces = atoms.map(() => ({ x: 0, y: 0 }));
    for (let i = 0; i < atoms.length; ++i) {
        const cx = grid.cellX(atoms[i].pos.x), cy = grid.cellY(atoms[i].pos.y);
        for (let dy = -1; dy <= 1; ++dy) {
            for (let dx = -1; dx <= 1; ++dx) {
                const nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows())
                    continue;
                for (const j of grid.at(nx, ny)) {
                    if (j <= i)
                        continue;
                    const f = computeInteraction(atoms, i, j);
                    forces[i].x -= f.x;
                    forces[i].y -= f.y;
                    forces[j].x += f.x;
                    forces[j].y += f.y;
                }
            }
        }
    }
    applyHBondAngularForces(atoms, forces);
    applyWaterAngleForces(atoms, forces);
    applyPeroxideAngleForces(atoms, forces);
    // Гравитация
    if (p.gravityEnabled) {
        const rad = p.gravityDirDeg * DEG2RAD;
        const gx = Math.cos(rad), gy = Math.sin(rad);
        const g = p.gravityMagnitude * 2.0; // GRAVITY_ACCEL_SCALE
        for (let i = 0; i < atoms.length; ++i) {
            forces[i].x += atoms[i].mass * g * gx;
            forces[i].y += atoms[i].mass * g * gy;
        }
    }
    // Интегрирование + термостат Ланжевена
    for (let i = 0; i < atoms.length; ++i) {
        const ax = forces[i].x / atoms[i].mass;
        const ay = forces[i].y / atoms[i].mass;
        atoms[i].vel.x += ax * config_1.PHYS_DT;
        atoms[i].vel.y += ay * config_1.PHYS_DT;
        const noiseAmp = Math.sqrt(2.0 * config_1.GAMMA_DAMPING * p.physTemp / atoms[i].mass * config_1.PHYS_DT);
        atoms[i].vel.x += -config_1.GAMMA_DAMPING * atoms[i].vel.x * config_1.PHYS_DT + noiseAmp * rng.normal();
        atoms[i].vel.y += -config_1.GAMMA_DAMPING * atoms[i].vel.y * config_1.PHYS_DT + noiseAmp * rng.normal();
        atoms[i].vel.x *= config_1.GLOBAL_DAMPING;
        atoms[i].vel.y *= config_1.GLOBAL_DAMPING;
        atoms[i].pos.x += atoms[i].vel.x * config_1.PHYS_DT;
        atoms[i].pos.y += atoms[i].vel.y * config_1.PHYS_DT;
        atoms[i].age += config_1.PHYS_DT * 0.1;
    }
    applyWallsToAtoms(atoms, world.walls);
    // Границы коробки
    const halfX = p.boxSizeX * 0.5, halfY = p.boxSizeY * 0.5;
    for (const a of atoms) {
        if (a.pos.x < -halfX + a.radius) {
            a.pos.x = -halfX + a.radius;
            a.vel.x = -a.vel.x * config_1.BOUNDARY_REST;
        }
        if (a.pos.x > halfX - a.radius) {
            a.pos.x = halfX - a.radius;
            a.vel.x = -a.vel.x * config_1.BOUNDARY_REST;
        }
        if (a.pos.y < -halfY + a.radius) {
            a.pos.y = -halfY + a.radius;
            a.vel.y = -a.vel.y * config_1.BOUNDARY_REST;
        }
        if (a.pos.y > halfY - a.radius) {
            a.pos.y = halfY - a.radius;
            a.vel.y = -a.vel.y * config_1.BOUNDARY_REST;
        }
    }
    // Нейтронный транспорт (опционально)
    if (stepNeutronsCb)
        stepNeutronsCb(world, p, config_1.PHYS_DT);
}
