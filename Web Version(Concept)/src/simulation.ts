// ============================================================
// simulation.ts — оркестратор: состояние мира + игровой цикл
// (порт куска main-цикла AtomSimulation.cpp: физика, спавн, статистика)
// ============================================================

import {
  PHYS_DT, SUBSTEPS, BOX_DEFAULT, TEMP_DEFAULT_C,
} from './core/config';
import {
  type Atom, type Neutron, type Gamma, type Wall, type Vec2,
  SpatialGrid, makeAtom,
} from './core/types';
import { Rng } from './core/rng';
import { tempCelsiusToPhysics, physicsSubStep, type SimParams } from './core/physics';
import { stepNeutrons, updateDelayedNeutrons, stepGammas } from './core/neutronTransport';

export interface SpawnItem { elementId: number; pos: Vec2; vel: Vec2; }

export interface Stats {
  atomsTotal: number;
  neutronsActive: number;
  gammasActive: number;
  fissionCount: number;
  captureCount: number;   // захваты (счётчик пока не ведётся — оставляем 0)
  waterMolecules: number;
  hclMolecules: number;
  naclPairs: number;
  h3oCount: number;
  hydroxideCount: number;
  peroxideCount: number;
  o2Count: number;
  uRemaining: number;
  fragmentsBaKr: number;
  avgTetrahedralQ: number;
  simTime: number;        // симуляционное время (субшаги × PHYS_DT)
}

export class Simulation {
  atoms: Atom[] = [];
  walls: Wall[] = [];
  neutrons: Neutron[] = [];
  gammas: Gamma[] = [];
  delayedPool: Neutron[] = [];

  grid = new SpatialGrid();
  rng = new Rng((Date.now() ^ 0x5EED) >>> 0);

  boxSizeX = BOX_DEFAULT;
  boxSizeY = BOX_DEFAULT;

  targetTempCelsius = TEMP_DEFAULT_C;
  gravityEnabled = false;
  gravityDirDeg = 90.0;
  gravityMagnitude = 1.0;

  timeScale = 1.0;              // слайдер скорости (0..SPEED_MAX)
  paused = false;

  physStepAccumulator = 0.0;
  simTime = 0.0;
  fissionCount = 0;

  constructor() {
    this.initGrid();
  }

  initGrid(): void {
    this.grid.init(-this.boxSizeX * 0.5, this.boxSizeX, 5.0);
  }

  setBoxSize(x: number, y: number): void {
    this.boxSizeX = x;
    this.boxSizeY = y;
    this.initGrid();
  }

  clearWorld(): void {
    this.atoms = [];
    this.neutrons = [];
    this.gammas = [];
    this.delayedPool = [];
    this.fissionCount = 0;
    this.simTime = 0;
  }

  spawn(item: SpawnItem): void {
    this.atoms.push(makeAtom(item.elementId, item.pos, item.vel));
  }

  /** Спавн молекулы воды вокруг точки pos */
  spawnWater(pos: Vec2, vel: Vec2 = { x: 0, y: 0 }): void {
    const o = makeAtom(1, pos, vel);
    const angle = this.rng.next() * Math.PI * 2;
    const rOH = 0.97;
    const half = 52.25 * Math.PI / 180;
    const h1p = {
      x: pos.x + Math.cos(angle + half) * rOH,
      y: pos.y + Math.sin(angle + half) * rOH,
    };
    const h2p = {
      x: pos.x + Math.cos(angle - half) * rOH,
      y: pos.y + Math.sin(angle - half) * rOH,
    };
    const h1 = makeAtom(0, h1p, { ...vel });
    const h2 = makeAtom(0, h2p, { ...vel });
    const oi = this.atoms.length;
    o.bonds[0] = oi + 1; o.bonds[1] = oi + 2;
    h1.bonds[0] = oi; h2.bonds[0] = oi;
    this.atoms.push(o, h1, h2);
  }

  /** Спавн молекулы HCl */
  spawnHCl(pos: Vec2, vel: Vec2 = { x: 0, y: 0 }): void {
    const h = makeAtom(0, { x: pos.x - 0.64, y: pos.y }, vel);
    const cl = makeAtom(2, { x: pos.x + 0.64, y: pos.y }, { ...vel });
    const hi = this.atoms.length;
    h.bonds[0] = hi + 1;
    cl.bonds[0] = hi;
    this.atoms.push(h, cl);
  }

  /** Запустить N тепловых нейтронов из точки (или случайных направлений) */
  injectNeutrons(pos: Vec2, n: number, energy_eV = 0.0253): void {
    for (let i = 0; i < n; ++i) {
      const a = this.rng.next() * Math.PI * 2;
      this.neutrons.push({
        pos: { ...pos },
        dir: { x: Math.cos(a), y: Math.sin(a) },
        energy_eV,
        age: 0,
        alive: true,
        parentU: -1,
        delayed: false,
      });
    }
  }

  /** Вызывается каждый кадр. dtReal — секунды реального времени. */
  update(dtReal: number): void {
    const timeScale = this.paused ? 0.0 : this.timeScale;
    const stepsNeeded = SUBSTEPS * timeScale;
    this.physStepAccumulator += stepsNeeded * dtReal * 60.0 / 60.0; // кадры нормализованы
    let stepsThisFrame = Math.floor(this.physStepAccumulator);
    if (stepsThisFrame > 2000) stepsThisFrame = 2000;
    this.physStepAccumulator -= stepsThisFrame;

    const physTemp = tempCelsiusToPhysics(this.targetTempCelsius);
    const params: SimParams = {
      physTemp,
      gravityEnabled: this.gravityEnabled,
      gravityDirDeg: this.gravityDirDeg,
      gravityMagnitude: this.gravityMagnitude,
      boxSizeX: this.boxSizeX,
      boxSizeY: this.boxSizeY,
    };

    for (let step = 0; step < stepsThisFrame; ++step) {
      const w = {
        atoms: this.atoms,
        walls: this.walls,
        grid: this.grid,
        rng: this.rng,
      };

      physicsSubStep(w, params, (ww, pp, dt) => {
        // --- Нейтронный транспорт ---
        const stats = stepNeutrons(
          this.neutrons, this.gammas, ww, this.delayedPool, ww.grid,
          { x: pp.boxSizeX, y: pp.boxSizeY }, dt, this.rng,
        );
        this.fissionCount += stats.fissionsThisStep;
        updateDelayedNeutrons(this.neutrons, this.delayedPool, dt, this.rng);
        stepGammas(this.gammas, { x: pp.boxSizeX, y: pp.boxSizeY }, dt);
      });

      // transport/Na могли заменить массив atoms — синхронизируем
      this.atoms = w.atoms;
      this.simTime += PHYS_DT;
    }
  }

  computeStats(): Stats {
    const atoms = this.atoms;
    let water = 0, hcl = 0, nacl = 0, h3o = 0, oh = 0, h2o2 = 0, o2 = 0;
    let uRemain = 0, frags = 0;
    let qSum = 0, qCount = 0;

    for (let i = 0; i < atoms.length; ++i) {
      const a = atoms[i];
      if (a.elementId === 3) uRemain++;
      if (a.elementId === 5 || a.elementId === 6) frags++;

      if (a.elementId === 1) {
        let h = 0, o = 0;
        for (const k of a.bonds) {
          if (k < 0 || k >= atoms.length) continue;
          if (atoms[k].elementId === 0) h++;
          else if (atoms[k].elementId === 1) o++;
        }
        if (h === 2 && o === 0) water++;
        else if (h === 3) h3o++;
        else if (h === 1 && o === 1) h2o2++;   // считаем по каждому O пероксида → делим потом
        else if (h === 0 && o === 1) o2++;
        else if (h === 1 && o === 0) oh++;

        // тетраэдрический порядок для кислородов воды
        if (h >= 2) {
          const { q, neighborCount } = tetraQ(atoms, i);
          if (neighborCount >= 4) { qSum += q; qCount++; }
        }
      }
      if (a.elementId === 0) {
        for (const k of a.bonds) {
          if (k >= 0 && atoms[k]?.elementId === 2) hcl++;
        }
      }
      if (a.elementId === 4) {
        for (const k of a.bonds) {
          if (k >= 0 && atoms[k]?.elementId === 2) nacl++;
        }
      }
    }

    return {
      atomsTotal: atoms.length,
      neutronsActive: this.neutrons.length,
      gammasActive: this.gammas.length,
      fissionCount: this.fissionCount,
      captureCount: 0,
      waterMolecules: water,
      hclMolecules: hcl,
      naclPairs: nacl,
      h3oCount: h3o,
      hydroxideCount: oh,
      peroxideCount: h2o2 / 2,
      o2Count: o2 / 2,
      uRemaining: uRemain,
      fragmentsBaKr: frags,
      avgTetrahedralQ: qCount > 0 ? qSum / qCount : 0,
      simTime: this.simTime,
    };
  }
}

// локальная версия (без импорта циклических зависимостей)
function tetraQ(atoms: Atom[], oIdx: number): { q: number; neighborCount: number } {
  const o = atoms[oIdx];
  const dirs: Vec2[] = [];
  for (const k of [...o.bonds, ...o.hbonds]) {
    if (k < 0 || k >= atoms.length) continue;
    const d = { x: atoms[k].pos.x - o.pos.x, y: atoms[k].pos.y - o.pos.y };
    const L = Math.hypot(d.x, d.y);
    if (L > 1e-6) dirs.push({ x: d.x / L, y: d.y / L });
  }
  const neighborCount = dirs.length;
  if (neighborCount < 4) return { q: 0, neighborCount };
  if (dirs.length > 4) dirs.length = 4;
  let sum = 0;
  for (let i = 0; i < dirs.length; ++i) {
    for (let j = i + 1; j < dirs.length; ++j) {
      const c = Math.max(-1, Math.min(1, dirs[i].x * dirs[j].x + dirs[i].y * dirs[j].y));
      const t = c + 1 / 3;
      sum += t * t;
    }
  }
  return { q: 1 - (3 / 8) * sum, neighborCount };
}
