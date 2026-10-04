"use strict";
// ============================================================
// config.ts — port of Config.hpp
// Физические параметры, морс-потенциалы, панели.
// ============================================================
Object.defineProperty(exports, "__esModule", { value: true });
exports.HBOND_BREAK_TEMP_MAX = exports.HBOND_BREAK_TEMP_MIN = exports.HBOND_FORM_TEMP_MAX = exports.HCL_HB_SLOTS_CL = exports.HCL_HB_ANGLE_COS_MIN = exports.HCL_HB_BREAK_DIST = exports.HCL_HB_FORM_DIST = exports.HCL_HB_cutoff = exports.HCL_HB_a = exports.HCL_HB_re = exports.HCL_HB_De = exports.NACL_cutoff = exports.NACL_a = exports.NACL_re = exports.NACL_De = exports.NA_EXPLOSION_RADIUS = exports.NA_EXPLOSION_KICK = exports.NA_WATER_REACT_DIST = exports.SPAWN_SPEED_MAX = exports.SPAWN_SPEED_K = exports.SPAWN_DRAG_MIN_PX = exports.NUCLEUS_SIZE_SCALE = exports.NUCLEON_RADIUS = exports.MAX_NUCLEONS_DISPLAY = exports.DRAG_THRESHOLD_PX = exports.TEMP_PHYS_MAX = exports.TEMP_DEFAULT_C = exports.TEMP_MAX_C = exports.TEMP_MIN_C = exports.GLOBAL_DAMPING = exports.GAMMA_DAMPING = exports.BOUNDARY_REST = exports.SUBSTEPS = exports.PHYS_DT = exports.HARD_CORE_K = exports.BOND_BREAK_DIST = exports.BOND_FORM_DIST = exports.VDW_CUTOFF = exports.VDW_a = exports.VDW_re = exports.VDW_De = exports.F_MAX = exports.KR_MASS = exports.BA_MASS = exports.NA_MASS = exports.U235_MASS = exports.NEUTRON_MASS = exports.CL_MASS = exports.O_MASS = exports.H_MASS = void 0;
exports.BOX_DEFAULT = exports.BOX_MAX = exports.BOX_MIN = exports.SPEED_MAX = exports.SPEED_MIN = exports.WALL_RESTITUTION = exports.WALL_MIN_LENGTH = exports.WALL_HIT_RADIUS = exports.WALL_THICKNESS = exports.GRAVITY_ACCEL_SCALE = exports.GRAVITY_MAG_MAX = exports.GRAVITY_DEFAULT_MAG = exports.GRAVITY_DEFAULT_DIR_DEG = exports.PEROXIDE_ANGLE_K = exports.PEROXIDE_ANGLE_DEG = exports.OO_SINGLE_cutoff = exports.OO_SINGLE_a = exports.OO_SINGLE_re = exports.OO_SINGLE_De = exports.HCL_DISSOC_DIST = exports.WATER_ANGLE_K = exports.WATER_ANGLE_DEG = exports.ANGLE_FORCE_MAX = exports.HBOND_K_ANG = exports.HBOND_SLOTS_O = exports.HBOND_ANGLE_COS_MIN = exports.HBOND_BREAK_DIST = exports.HBOND_FORM_DIST = exports.HBOND_CUTOFF = exports.HBOND_a = exports.HBOND_re = exports.HBOND_De = exports.TIP4P_OM_DIST = exports.HCL_HB_BREAK_TEMP_MAX = exports.HCL_HB_BREAK_TEMP_MIN = exports.HCL_HBOND_FORM_TEMP_MAX = void 0;
exports.getMorsePair = getMorsePair;
exports.getMorsePairByOrder = getMorsePairByOrder;
exports.getBondDists = getBondDists;
// --- Физические параметры ---
exports.H_MASS = 1.008;
exports.O_MASS = 15.999;
exports.CL_MASS = 35.45;
exports.NEUTRON_MASS = 1.0;
exports.U235_MASS = 235.0;
exports.NA_MASS = 22.99;
exports.BA_MASS = 138.0; // типичный осколок Ba-138 (Z=56, N=82)
exports.KR_MASS = 94.0; // типичный осколок Kr-94  (Z=36, N=58)
exports.F_MAX = 50.0;
exports.VDW_De = 0.02;
exports.VDW_re = 3.5;
exports.VDW_a = 0.8;
exports.VDW_CUTOFF = 5.0;
exports.BOND_FORM_DIST = 1.4;
exports.BOND_BREAK_DIST = 2.5;
exports.HARD_CORE_K = 30.0;
exports.PHYS_DT = 0.005;
exports.SUBSTEPS = 20;
exports.BOUNDARY_REST = 0.9;
exports.GAMMA_DAMPING = 0.5;
exports.GLOBAL_DAMPING = 1.0;
exports.TEMP_MIN_C = -273.15;
exports.TEMP_MAX_C = 110.0;
exports.TEMP_DEFAULT_C = 0.0;
exports.TEMP_PHYS_MAX = 0.3;
exports.DRAG_THRESHOLD_PX = 6.0;
// --- Нуклоны в ядрах ---
exports.MAX_NUCLEONS_DISPLAY = 45;
exports.NUCLEON_RADIUS = 0.024; // Å
exports.NUCLEUS_SIZE_SCALE = 0.5;
// --- Спавн с направлением/скоростью (drag) ---
exports.SPAWN_DRAG_MIN_PX = 8.0;
exports.SPAWN_SPEED_K = 2.0;
exports.SPAWN_SPEED_MAX = 15.0;
// --- Реакция натрия с водой ---
exports.NA_WATER_REACT_DIST = 1.8;
exports.NA_EXPLOSION_KICK = 18.0;
exports.NA_EXPLOSION_RADIUS = 4.0;
// --- Ионная связь Na+Cl- ---
exports.NACL_De = 3.0;
exports.NACL_re = 2.2;
exports.NACL_a = 1.5;
exports.NACL_cutoff = 3.5;
// --- HCl-лёд: усиленная водородная связь H···Cl ---
exports.HCL_HB_De = 0.7;
exports.HCL_HB_re = 2.2;
exports.HCL_HB_a = 1.5;
exports.HCL_HB_cutoff = 3.5;
exports.HCL_HB_FORM_DIST = 2.6;
exports.HCL_HB_BREAK_DIST = 3.5;
exports.HCL_HB_ANGLE_COS_MIN = 0.5;
exports.HCL_HB_SLOTS_CL = 2;
// --- Испарение воды / плавление HCl-льда ---
exports.HBOND_FORM_TEMP_MAX = 0.28;
exports.HBOND_BREAK_TEMP_MIN = 0.26;
exports.HBOND_BREAK_TEMP_MAX = 0.3;
exports.HCL_HBOND_FORM_TEMP_MAX = 0.0485;
exports.HCL_HB_BREAK_TEMP_MIN = 0.0485;
exports.HCL_HB_BREAK_TEMP_MAX = 0.055;
// --- TIP4P ---
exports.TIP4P_OM_DIST = 0.15;
// --- Водородная связь (вода) ---
exports.HBOND_De = 1.0;
exports.HBOND_re = 1.7;
exports.HBOND_a = 1.5;
exports.HBOND_CUTOFF = 2.85;
exports.HBOND_FORM_DIST = 2.15;
exports.HBOND_BREAK_DIST = 2.85;
exports.HBOND_ANGLE_COS_MIN = 0.7;
exports.HBOND_SLOTS_O = 2;
exports.HBOND_K_ANG = 10.0;
exports.ANGLE_FORCE_MAX = 20.0;
exports.WATER_ANGLE_DEG = 104.5;
exports.WATER_ANGLE_K = 11.0;
// --- Диссоциация HCl в воде ---
exports.HCL_DISSOC_DIST = 2.3;
// --- Пероксид водорода H2O2 ---
exports.OO_SINGLE_De = 2.5;
exports.OO_SINGLE_re = 1.48;
exports.OO_SINGLE_a = 2.0;
exports.OO_SINGLE_cutoff = 3.2;
exports.PEROXIDE_ANGLE_DEG = 115.0;
exports.PEROXIDE_ANGLE_K = 11.0;
// --- Гравитация ---
exports.GRAVITY_DEFAULT_DIR_DEG = 90.0;
exports.GRAVITY_DEFAULT_MAG = 1.0;
exports.GRAVITY_MAG_MAX = 3.0;
exports.GRAVITY_ACCEL_SCALE = 2.0;
// --- Стены ---
exports.WALL_THICKNESS = 0.1;
exports.WALL_HIT_RADIUS = 0.3;
exports.WALL_MIN_LENGTH = 0.5;
exports.WALL_RESTITUTION = 0.5;
// --- Скорость симуляции (лог-слайдер) ---
exports.SPEED_MIN = 0.01;
exports.SPEED_MAX = 50.0;
// --- Размер коробки ---
exports.BOX_MIN = 4.0;
exports.BOX_MAX = 80.0;
exports.BOX_DEFAULT = 20.0;
const loHi = (e1, e2) => e1 <= e2 ? [e1, e2] : [e2, e1];
function getMorsePair(e1, e2) {
    const [lo, hi] = loHi(e1, e2);
    if (lo === 0 && hi === 0)
        return { De: 5.0, re: 0.74, a: 1.94, cutoff: 3.0 }; // H-H
    if (lo === 1 && hi === 1)
        return { De: 5.5, re: 1.21, a: 2.0, cutoff: 3.2 }; // O-O
    if (lo === 2 && hi === 2)
        return { De: 2.5, re: 1.99, a: 1.5, cutoff: 3.5 }; // Cl-Cl
    if (lo === 0 && hi === 1)
        return { De: 5.0, re: 0.97, a: 1.8, cutoff: 3.0 }; // H-O
    if (lo === 0 && hi === 2)
        return { De: 4.5, re: 1.27, a: 1.8, cutoff: 3.0 }; // H-Cl
    if (lo === 1 && hi === 2)
        return { De: 0.5, re: 3.0, a: 1.5, cutoff: 3.5 }; // O-Cl
    if (lo === 2 && hi === 4)
        return { De: exports.NACL_De, re: exports.NACL_re, a: exports.NACL_a, cutoff: exports.NACL_cutoff }; // Na-Cl
    return { De: 5.0, re: 0.97, a: 1.8, cutoff: 3.0 }; // fallback
}
// Одинарная O-O (H2O2) vs двойная O=O (O2)
function getMorsePairByOrder(e1, e2, order) {
    if (e1 === 1 && e2 === 1 && order <= 1) {
        return { De: exports.OO_SINGLE_De, re: exports.OO_SINGLE_re, a: exports.OO_SINGLE_a, cutoff: exports.OO_SINGLE_cutoff };
    }
    return getMorsePair(e1, e2);
}
function getBondDists(e1, e2) {
    const [lo, hi] = loHi(e1, e2);
    if (lo === 2 && hi === 2)
        return { form: 2.4, brk: 3.5 }; // Cl-Cl
    if (lo === 0 && hi === 2)
        return { form: 1.7, brk: 2.9 }; // H-Cl
    if (lo === 2 && hi === 4)
        return { form: 2.6, brk: 3.8 }; // Na-Cl (ионная)
    return { form: exports.BOND_FORM_DIST, brk: exports.BOND_BREAK_DIST };
}
