// ============================================================
// config.ts — port of Config.hpp
// Физические параметры, морс-потенциалы, панели.
// ============================================================

// --- Физические параметры ---
export const H_MASS = 1.008;
export const O_MASS = 15.999;
export const CL_MASS = 35.45;
export const NEUTRON_MASS = 1.0;
export const U235_MASS = 235.0;
export const NA_MASS = 22.99;
export const BA_MASS = 138.0;   // типичный осколок Ba-138 (Z=56, N=82)
export const KR_MASS = 94.0;    // типичный осколок Kr-94  (Z=36, N=58)

export const F_MAX = 50.0;

export const VDW_De = 0.02;
export const VDW_re = 3.5;
export const VDW_a = 0.8;
export const VDW_CUTOFF = 5.0;

export const BOND_FORM_DIST = 1.4;
export const BOND_BREAK_DIST = 2.5;
export const HARD_CORE_K = 30.0;

export const PHYS_DT = 0.005;
export const SUBSTEPS = 20;
export const BOUNDARY_REST = 0.9;

export const GAMMA_DAMPING = 0.5;
export const GLOBAL_DAMPING = 1.0;

export const TEMP_MIN_C = -273.15;
export const TEMP_MAX_C = 110.0;
export const TEMP_DEFAULT_C = 0.0;
export const TEMP_PHYS_MAX = 0.3;

export const DRAG_THRESHOLD_PX = 6.0;

// --- Нуклоны в ядрах ---
export const MAX_NUCLEONS_DISPLAY = 45;
export const NUCLEON_RADIUS = 0.024;      // Å
export const NUCLEUS_SIZE_SCALE = 0.5;

// --- Спавн с направлением/скоростью (drag) ---
export const SPAWN_DRAG_MIN_PX = 8.0;
export const SPAWN_SPEED_K = 2.0;
export const SPAWN_SPEED_MAX = 15.0;

// --- Реакция натрия с водой ---
export const NA_WATER_REACT_DIST = 1.8;
export const NA_EXPLOSION_KICK = 18.0;
export const NA_EXPLOSION_RADIUS = 4.0;

// --- Ионная связь Na+Cl- ---
export const NACL_De = 3.0;
export const NACL_re = 2.2;
export const NACL_a = 1.5;
export const NACL_cutoff = 3.5;

// --- HCl-лёд: усиленная водородная связь H···Cl ---
export const HCL_HB_De = 0.7;
export const HCL_HB_re = 2.2;
export const HCL_HB_a = 1.5;
export const HCL_HB_cutoff = 3.5;
export const HCL_HB_FORM_DIST = 2.6;
export const HCL_HB_BREAK_DIST = 3.5;
export const HCL_HB_ANGLE_COS_MIN = 0.5;
export const HCL_HB_SLOTS_CL = 2;

// --- Испарение воды / плавление HCl-льда ---
export const HBOND_FORM_TEMP_MAX = 0.28;
export const HBOND_BREAK_TEMP_MIN = 0.26;
export const HBOND_BREAK_TEMP_MAX = 0.3;

export const HCL_HBOND_FORM_TEMP_MAX = 0.0485;
export const HCL_HB_BREAK_TEMP_MIN = 0.0485;
export const HCL_HB_BREAK_TEMP_MAX = 0.055;

// --- TIP4P ---
export const TIP4P_OM_DIST = 0.15;

// --- Водородная связь (вода) ---
export const HBOND_De = 1.0;
export const HBOND_re = 1.7;
export const HBOND_a = 1.5;
export const HBOND_CUTOFF = 2.85;
export const HBOND_FORM_DIST = 2.15;
export const HBOND_BREAK_DIST = 2.85;
export const HBOND_ANGLE_COS_MIN = 0.7;
export const HBOND_SLOTS_O = 2;
export const HBOND_K_ANG = 10.0;
export const ANGLE_FORCE_MAX = 20.0;

export const WATER_ANGLE_DEG = 104.5;
export const WATER_ANGLE_K = 11.0;

// --- Диссоциация HCl в воде ---
export const HCL_DISSOC_DIST = 2.3;

// --- Пероксид водорода H2O2 ---
export const OO_SINGLE_De = 2.5;
export const OO_SINGLE_re = 1.48;
export const OO_SINGLE_a = 2.0;
export const OO_SINGLE_cutoff = 3.2;

export const PEROXIDE_ANGLE_DEG = 115.0;
export const PEROXIDE_ANGLE_K = 11.0;

// --- Гравитация ---
export const GRAVITY_DEFAULT_DIR_DEG = 90.0;
export const GRAVITY_DEFAULT_MAG = 1.0;
export const GRAVITY_MAG_MAX = 3.0;
export const GRAVITY_ACCEL_SCALE = 2.0;

// --- Стены ---
export const WALL_THICKNESS = 0.1;
export const WALL_HIT_RADIUS = 0.3;
export const WALL_MIN_LENGTH = 0.5;
export const WALL_RESTITUTION = 0.5;

// --- Скорость симуляции (лог-слайдер) ---
export const SPEED_MIN = 0.01;
export const SPEED_MAX = 50.0;

// --- Размер коробки ---
export const BOX_MIN = 4.0;
export const BOX_MAX = 80.0;
export const BOX_DEFAULT = 20.0;

// ============================================================
// Морс-параметры пар элементов
// ============================================================
export interface MorsePair { De: number; re: number; a: number; cutoff: number; }

const loHi = (e1: number, e2: number): [number, number] =>
  e1 <= e2 ? [e1, e2] : [e2, e1];

export function getMorsePair(e1: number, e2: number): MorsePair {
  const [lo, hi] = loHi(e1, e2);
  if (lo === 0 && hi === 0) return { De: 5.0, re: 0.74, a: 1.94, cutoff: 3.0 };   // H-H
  if (lo === 1 && hi === 1) return { De: 5.5, re: 1.21, a: 2.0, cutoff: 3.2 };    // O-O
  if (lo === 2 && hi === 2) return { De: 2.5, re: 1.99, a: 1.5, cutoff: 3.5 };    // Cl-Cl
  if (lo === 0 && hi === 1) return { De: 5.0, re: 0.97, a: 1.8, cutoff: 3.0 };    // H-O
  if (lo === 0 && hi === 2) return { De: 4.5, re: 1.27, a: 1.8, cutoff: 3.0 };    // H-Cl
  if (lo === 1 && hi === 2) return { De: 0.5, re: 3.0, a: 1.5, cutoff: 3.5 };     // O-Cl
  if (lo === 2 && hi === 4) return { De: NACL_De, re: NACL_re, a: NACL_a, cutoff: NACL_cutoff }; // Na-Cl
  return { De: 5.0, re: 0.97, a: 1.8, cutoff: 3.0 };                               // fallback
}

// Одинарная O-O (H2O2) vs двойная O=O (O2)
export function getMorsePairByOrder(e1: number, e2: number, order: number): MorsePair {
  if (e1 === 1 && e2 === 1 && order <= 1) {
    return { De: OO_SINGLE_De, re: OO_SINGLE_re, a: OO_SINGLE_a, cutoff: OO_SINGLE_cutoff };
  }
  return getMorsePair(e1, e2);
}

// --- Парные дистанции образования/разрыва связей ---
export interface BondDists { form: number; brk: number; }

export function getBondDists(e1: number, e2: number): BondDists {
  const [lo, hi] = loHi(e1, e2);
  if (lo === 2 && hi === 2) return { form: 2.4, brk: 3.5 };       // Cl-Cl
  if (lo === 0 && hi === 2) return { form: 1.7, brk: 2.9 };       // H-Cl
  if (lo === 2 && hi === 4) return { form: 2.6, brk: 3.8 };       // Na-Cl (ионная)
  return { form: BOND_FORM_DIST, brk: BOND_BREAK_DIST };
}
