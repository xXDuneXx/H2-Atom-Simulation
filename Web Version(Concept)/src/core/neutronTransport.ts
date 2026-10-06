// ============================================================
// neutronTransport.ts — port of NeutronTransport.cpp
// Монте-Карло транспорт нейтронов, деление U-235, осколки,
// запаздывающие нейтроны, гамма-каскады.
// ============================================================

import {
  type Atom, type Neutron, type Gamma, type Vec2, SpatialGrid, makeAtom,
} from './types';
import { Rng, randomUnitVector2D } from './rng';
import {
  U235_XS, U235_YIELDS, DELAYED_GROUPS, U235_RESONANCES,
} from './nuclearData';
import { compactAtomsAfterFission } from './physics';

const clampNum = (v: number, lo: number, hi: number) =>
  v < lo ? lo : v > hi ? hi : v;

// ============================================================
// SECTION 1. Утилиты
// ============================================================

// Скорость нейтрона в симуляционных единицах.
// Тепловой нейтрон (0.0253 эВ) — 5 у.е./с. Быстрые ограничены 20 у.е./с.
export function neutronSpeed(E_eV: number): number {
  const THERMAL_EV = 0.0253;
  const THERMAL_SPEED = 5.0;
  const MAX_SPEED = 20.0;
  if (E_eV <= 0) return 0;
  let v = THERMAL_SPEED * Math.sqrt(E_eV / THERMAL_EV);
  if (v > MAX_SPEED) v = MAX_SPEED;
  return v;
}

// Сэмплирование Пуассона (алгоритм Кнута, для λ < 30)
export function poissonSample(lambda: number, rng: Rng): number {
  if (lambda <= 0) return 0;
  const L = Math.exp(-lambda);
  let p = 1.0;
  let k = 0;
  do {
    ++k;
    p *= rng.next();
  } while (p > L && k < 30);
  return k - 1;
}

// ============================================================
// SECTION 2. Сечения
// ============================================================

/** Лог-линейная интерполяция по таблице U235_XS. type: 0=σ_f, 1=σ_c, 2=σ_s */
export function lookupSigma(E_eV: number, type: number): number {
  const N = U235_XS.length;
  const pick = (r: typeof U235_XS[0]): number =>
    type === 0 ? r.sigma_f : type === 1 ? r.sigma_c : r.sigma_s;

  if (E_eV <= U235_XS[0].E_eV) return pick(U235_XS[0]);
  if (E_eV >= U235_XS[N - 1].E_eV) return pick(U235_XS[N - 1]);

  let lo = 0;
  for (let i = 1; i < N; ++i) {
    if (U235_XS[i].E_eV > E_eV) { lo = i - 1; break; }
  }
  const a = U235_XS[lo], b = U235_XS[lo + 1];
  const t = (Math.log(E_eV) - Math.log(a.E_eV)) / (Math.log(b.E_eV) - Math.log(a.E_eV));
  const sa = pick(a), sb = pick(b);
  return sa + (sb - sa) * t;
}

export const lookupSigmaFission = (E_eV: number) => lookupSigma(E_eV, 0);
export const lookupSigmaCapture = (E_eV: number) => lookupSigma(E_eV, 1);
export const lookupSigmaScatter = (E_eV: number) => lookupSigma(E_eV, 2);

/** Резонансный вклад в σ_f по Брейту-Вигнеру */
export function resonanceFissionSigma(E_eV: number): number {
  const HBAR2_OVER_2M = 2.07e5;   // барн·эВ
  const PI = Math.PI;
  let sum = 0.0;
  for (const res of U235_RESONANCES) {
    if (res.E_eV <= 0) continue;   // пропускаем связанный уровень
    const G = res.Gamma_n + res.Gamma_g + res.Gamma_f;
    if (G < 1e-10) continue;
    const dE = E_eV - res.E_eV;
    const denom = dE * dE + 0.25 * G * G;
    const lambda2 = HBAR2_OVER_2M / res.E_eV;
    sum += PI * lambda2 * res.g_J * res.Gamma_n * res.Gamma_f / denom;
  }
  return sum;
}

export function totalFissionSigma(E_eV: number): number {
  return lookupSigmaFission(E_eV) + resonanceFissionSigma(E_eV);
}

/** Полное сечение взаимодействия для атома любого типа */
export function lookupSigmaTotal(elementId: number, E_eV: number): number {
  if (elementId === 3) {   // U-235
    return totalFissionSigma(E_eV) + lookupSigmaCapture(E_eV) + lookupSigmaScatter(E_eV);
  }
  if (elementId === 0) return 20.0;   // H
  if (elementId === 1) return 4.0;    // O
  if (elementId === 2) return 50.0;   // Cl
  return 1.0;
}

/** Радиус «взаимодействия» в 2D, масштабируется как √σ */
export function interactionRadius(elementId: number, E_eV: number): number {
  const R_0 = 5.0;
  const SIGMA_REF = 700.0;
  const sigma = lookupSigmaTotal(elementId, E_eV);
  return R_0 * Math.sqrt(sigma / SIGMA_REF);
}

// ============================================================
// SECTION 3. Сэмплирование
// ============================================================

/** Массовое число лёгкого осколка по таблице выходов */
export function sampleYield(rng: Rng): number {
  let total = 0.0;
  for (const y of U235_YIELDS) total += y.yield_percent;
  const r = rng.next() * total;
  let acc = 0.0;
  for (const y of U235_YIELDS) {
    acc += y.yield_percent;
    if (r <= acc) return y.A;
  }
  return U235_YIELDS[U235_YIELDS.length - 1].A;
}

/** Спектр Уатта для промпт-нейтронов (средняя ≈ 2 МэВ) */
export function sampleWattSpectrum(rng: Rng): number {
  let u1 = rng.next(), u2 = rng.next();
  if (u1 < 1e-10) u1 = 1e-10;
  if (u2 < 1e-10) u2 = 1e-10;
  let E_MeV = -Math.log(u1) - 0.8 * Math.log(u2);
  if (E_MeV < 1e-4) E_MeV = 1e-4;
  if (E_MeV > 12.0) E_MeV = 12.0;
  return E_MeV * 1e6;   // эВ
}

/** Множественность нейтронов. U-235 тепловой: ν ≈ 2.43 */
export const sampleNeutronMultiplicity = (rng: Rng): number => poissonSample(2.43, rng);

/** Группа запаздывающих нейтронов (6 групп, Keepin) */
export function sampleDelayedGroup(rng: Rng): number {
  const N = DELAYED_GROUPS.length;
  let totalBeta = 0.0;
  for (let i = 0; i < N; ++i) totalBeta += DELAYED_GROUPS[i].beta_frac;
  const r = rng.next() * totalBeta;
  let acc = 0.0;
  for (let i = 0; i < N; ++i) {
    acc += DELAYED_GROUPS[i].beta_frac;
    if (r <= acc) return i;
  }
  return N - 1;
}

// ============================================================
// SECTION 4. Рассеяние и гамма-каскады
// ============================================================

/** Упругое рассеяние нейтрона на ядре (точная кинематика, изотропно в СЦМ) */
export function scatterNeutron(n: Neutron, targetMass: number, rng: Rng): void {
  const A = Math.max(1.0, targetMass);   // в единицах массы нейтрона
  let alpha = (A - 1.0) / (A + 1.0);
  alpha = alpha * alpha;

  let E_new = n.energy_eV * (alpha + (1.0 - alpha) * rng.next());
  if (E_new < 1e-4) E_new = 1e-4;

  const cosCom = 2.0 * rng.next() - 1.0;
  const denom = Math.sqrt(1.0 + A * A + 2.0 * A * cosCom);
  const cosLab = clampNum((1.0 + A * cosCom) / denom, -1.0, 1.0);
  const deviation = Math.acos(cosLab);

  const sign = rng.next() < 0.5 ? -1.0 : 1.0;
  const currentAngle = Math.atan2(n.dir.y, n.dir.x);
  const newAngle = currentAngle + sign * deviation;
  n.dir = { x: Math.cos(newAngle), y: Math.sin(newAngle) };
  n.energy_eV = E_new;
}

/** Каскад гамма-квантов (3–5 фотонов, суммарно ≈ 7 МэВ) */
export function spawnGammaCascade(pos: Vec2, gammas: Gamma[], rng: Rng): void {
  const count = 3 + Math.floor(rng.next() * 3.0);   // 3..5
  const totalE = 7.0;   // МэВ
  for (let i = 0; i < count; ++i) {
    gammas.push({
      pos: { ...pos },
      dir: randomUnitVector2D(rng),
      energy_MeV: totalE / count * (0.7 + 0.6 * rng.next()),
      age: 0.0,
      alive: true,
    });
  }
}

// ============================================================
// SECTION 5. Деление
// ============================================================

export interface FissionResult {
  fissions: number;
}

/**
 * Обработка деления U-235. Добавляет осколки в atoms, промпт-нейтроны
 * в newNeutrons, запаздывающие в delayedPool, гамма в gammas.
 * Помечает U-235 elementId = -1 (удаление при компактизации).
 */
export function handleFission(
  hitIdx: number,
  atoms: Atom[],
  newNeutrons: Neutron[],
  gammas: Gamma[],
  delayedPool: Neutron[],
  rng: Rng,
): void {
  const uPos = { ...atoms[hitIdx].pos };

  // 1) Массы осколков
  const A1 = sampleYield(rng);
  const A2 = 236 - A1;

  // 2) Кинематика осколков
  const KE_FRAG_MeV = 169.0;
  const SPEED_SCALE = 0.5;
  const E1 = KE_FRAG_MeV * (A2 / (A1 + A2));
  const E2 = KE_FRAG_MeV * (A1 / (A1 + A2));
  const v1 = SPEED_SCALE * Math.sqrt(E1 / A1);
  const v2 = SPEED_SCALE * Math.sqrt(E2 / A2);

  const baseAngle = rng.next() * 2.0 * Math.PI;
  const dir1 = { x: Math.cos(baseAngle), y: Math.sin(baseAngle) };
  const dir2 = { x: -dir1.x, y: -dir1.y };

  // 3) Осколки: лёгкий — Kr (Z=36), тяжёлый — Ba (Z=56)
  const KR_ID = 6, BA_ID = 5;
  let A_kr: number, A_ba: number;
  let pos_kr: Vec2, pos_ba: Vec2, vel_kr: Vec2, vel_ba: Vec2;
  if (A1 < A2) {
    A_kr = A1; A_ba = A2;
    pos_kr = { x: uPos.x + dir1.x * 0.3, y: uPos.y + dir1.y * 0.3 };
    vel_kr = { x: dir1.x * v1, y: dir1.y * v1 };
    pos_ba = { x: uPos.x + dir2.x * 0.3, y: uPos.y + dir2.y * 0.3 };
    vel_ba = { x: dir2.x * v2, y: dir2.y * v2 };
  } else {
    A_kr = A2; A_ba = A1;
    pos_kr = { x: uPos.x + dir2.x * 0.3, y: uPos.y + dir2.y * 0.3 };
    vel_kr = { x: dir2.x * v2, y: dir2.y * v2 };
    pos_ba = { x: uPos.x + dir1.x * 0.3, y: uPos.y + dir1.y * 0.3 };
    vel_ba = { x: dir1.x * v1, y: dir1.y * v1 };
  }

  const kr = makeAtom(KR_ID, pos_kr, vel_kr);
  kr.mass = A_kr;
  kr.radius = 0.45 + 0.025 * Math.sqrt(A_kr);
  atoms.push(kr);

  const ba = makeAtom(BA_ID, pos_ba, vel_ba);
  ba.mass = A_ba;
  ba.radius = 0.45 + 0.025 * Math.sqrt(A_ba);
  atoms.push(ba);

  // 4) Промпт-нейтроны
  const np = sampleNeutronMultiplicity(rng);
  for (let k = 0; k < np; ++k) {
    newNeutrons.push({
      pos: { ...uPos },
      dir: randomUnitVector2D(rng),
      energy_eV: sampleWattSpectrum(rng),
      age: 0.0,
      alive: true,
      parentU: -1,
      delayed: false,
    });
  }

  // 5) Запаздывающие нейтроны (β_total ≈ 0.65%)
  if (rng.next() < 0.0065) {
    const g = sampleDelayedGroup(rng);
    const lambda = Math.max(DELAYED_GROUPS[g].lambda, 1e-3);
    let u01 = rng.next();
    if (u01 < 1e-6) u01 = 1e-6;
    const t_emit = -Math.log(u01) / lambda;

    delayedPool.push({
      pos: { ...uPos },
      dir: { x: 0.0, y: 0.0 },
      energy_eV: 0.5e6,
      age: t_emit,       // здесь age = время до испускания
      alive: true,
      parentU: g,        // индекс группы
      delayed: true,
    });
  }

  // 6) Гамма-каскад (~7 МэВ)
  spawnGammaCascade(uPos, gammas, rng);

  // 7) Помечаем U-235 как удалённый
  atoms[hitIdx].elementId = -1;
}

// ============================================================
// SECTION 6. Запаздывающие нейтроны
// ============================================================

export function updateDelayedNeutrons(
  activeNeutrons: Neutron[],
  delayedPool: Neutron[],
  dt: number,
  rng: Rng,
): void {
  for (const dn of delayedPool) {
    if (!dn.alive) continue;
    dn.age -= dt;
    if (dn.age <= 0.0) {
      activeNeutrons.push({
        pos: { ...dn.pos },
        dir: randomUnitVector2D(rng),
        energy_eV: 0.5e6,
        age: 0.0,
        alive: true,
        parentU: -1,
        delayed: false,
      });
      dn.alive = false;
    }
  }
  for (let i = delayedPool.length - 1; i >= 0; --i) {
    if (!delayedPool[i].alive) delayedPool.splice(i, 1);
  }
}

// ============================================================
// SECTION 7. Основной шаг транспорта
// ============================================================

export interface TransportStats {
  fissionsThisStep: number;
}

export function stepNeutrons(
  neutrons: Neutron[],
  gammas: Gamma[],
  world: { atoms: Atom[] },
  delayedPool: Neutron[],
  grid: SpatialGrid,
  boxSize: Vec2,
  dt: number,
  rng: Rng,
): TransportStats {
  const halfX = boxSize.x * 0.5;
  const halfY = boxSize.y * 0.5;
  let fissionsThisStep = 0;

  const atoms = world.atoms;
  const newNeutrons: Neutron[] = [];

  for (let ni = 0; ni < neutrons.length; ++ni) {
    const n = neutrons[ni];
    if (!n.alive) continue;

    const speed = neutronSpeed(n.energy_eV);
    const move = speed * dt;

    // Подшаги, чтобы не «проскакивать» ядра с малым радиусом взаимодействия
    const MAX_CHUNK = 0.3;   // Å на подшаг
    let nChunks = Math.max(1, Math.ceil(move / MAX_CHUNK));
    nChunks = Math.min(nChunks, 200);
    const chunkSize = move / nChunks;

    for (let c = 0; c < nChunks && n.alive; ++c) {
      n.pos.x += n.dir.x * chunkSize;
      n.pos.y += n.dir.y * chunkSize;

      if (Math.abs(n.pos.x) > halfX || Math.abs(n.pos.y) > halfY) {
        n.alive = false;
        break;
      }

      const cx = grid.cellX(n.pos.x), cy = grid.cellY(n.pos.y);

      for (let dy = -1; dy <= 1 && n.alive; ++dy) {
        for (let dx = -1; dx <= 1 && n.alive; ++dx) {
          const nx = cx + dx, ny = cy + dy;
          if (nx < 0 || nx >= grid.cols() || ny < 0 || ny >= grid.rows()) continue;

          for (const idx of grid.at(nx, ny)) {
            if (idx < 0 || idx >= atoms.length) continue;
            const a = atoms[idx];
            if (a.elementId < 0) continue;

            // Осколки деления не участвуют в транспорте (elementId!=3, mass>60)
            if (a.elementId !== 3 && a.mass > 60.0) continue;

            const ddx = a.pos.x - n.pos.x;
            const ddy = a.pos.y - n.pos.y;
            const r2 = ddx * ddx + ddy * ddy;

            const R_int = interactionRadius(a.elementId, n.energy_eV);
            if (r2 > R_int * R_int) continue;

            // === ВЗАИМОДЕЙСТВИЕ ===
            const sigma_f = a.elementId === 3 ? totalFissionSigma(n.energy_eV) : 0.0;
            const sigma_c = a.elementId === 3 ? lookupSigmaCapture(n.energy_eV) : 0.0;
            let sigma_s: number;
            if (a.elementId === 3) sigma_s = lookupSigmaScatter(n.energy_eV);
            else if (a.elementId === 0) sigma_s = 20.0;
            else if (a.elementId === 1) sigma_s = 4.0;
            else if (a.elementId === 2) sigma_s = 15.0;
            else sigma_s = 5.0;

            const sigma_t = sigma_f + sigma_c + sigma_s;
            if (sigma_t <= 1e-10) continue;

            const rr = rng.next() * sigma_t;
            if (rr < sigma_f) {
              // Деление
              handleFission(idx, atoms, newNeutrons, gammas, delayedPool, rng);
              n.alive = false;
              fissionsThisStep++;
            } else if (rr < sigma_f + sigma_c) {
              // Радиационный захват
              spawnGammaCascade(a.pos, gammas, rng);
              n.alive = false;
            } else {
              // Упругое рассеяние
              scatterNeutron(n, a.mass, rng);
            }
            break;   // одно взаимодействие на chunk
          }
        }
      }
    }

    if (n.alive) n.age += dt;
  }

  // Новые нейтроны от делений
  for (const nn of newNeutrons) neutrons.push(nn);

  // Чистка мёртвых
  for (let i = neutrons.length - 1; i >= 0; --i) {
    if (!neutrons[i].alive) neutrons.splice(i, 1);
  }

  // Компактизация помеченных U-235
  world.atoms = compactAtomsAfterFission(atoms);

  return { fissionsThisStep };
}

// ============================================================
// SECTION 8. Гамма-кванты (упрощённый транспорт)
// ============================================================

export function stepGammas(gammas: Gamma[], boxSize: Vec2, dt: number): void {
  const halfX = boxSize.x * 0.5;
  const halfY = boxSize.y * 0.5;
  const GAMMA_SPEED = 60.0;   // у.е./с, быстрее нейтронов

  for (const g of gammas) {
    if (!g.alive) continue;
    g.pos.x += g.dir.x * GAMMA_SPEED * dt;
    g.pos.y += g.dir.y * GAMMA_SPEED * dt;
    g.age += dt;
    if (Math.abs(g.pos.x) > halfX || Math.abs(g.pos.y) > halfY) g.alive = false;
  }
  for (let i = gammas.length - 1; i >= 0; --i) {
    if (!gammas[i].alive) gammas.splice(i, 1);
  }
}
