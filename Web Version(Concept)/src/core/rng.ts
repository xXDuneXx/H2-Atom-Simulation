// ============================================================
// rng.ts — замена std::mt19937 + distributions
// Быстрый mulberry32 + Box-Muller для нормального шума.
// Точное совпадение последовательностей с C++ не требуется:
// вся стохастика (термостат, Монте-Карло транспорта) от этого
// не зависит по смыслу.
// ============================================================

export class Rng {
  private s: number;

  constructor(seed = 0x9E3779B9) {
    this.s = seed >>> 0;
  }

  reseed(seed: number): void { this.s = seed >>> 0; }

  /** uniform [0,1) */
  next(): number {
    this.s = (this.s + 0x6D2B79F5) >>> 0;
    let t = this.s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }

  uniform(a: number, b: number): number { return a + (b - a) * this.next(); }

  /** стандартное нормальное (Box–Muller) — аналог std::normal_distribution<float> */
  normal(): number {
    let u1 = this.next();
    if (u1 < 1e-12) u1 = 1e-12;
    const u2 = this.next();
    return Math.sqrt(-2.0 * Math.log(u1)) * Math.cos(2.0 * Math.PI * u2);
  }
}

export const uniform01 = (rng: Rng): number => rng.next();

export function randomUnitVector2D(rng: Rng): { x: number; y: number } {
  const a = rng.next() * 2.0 * Math.PI;
  return { x: Math.cos(a), y: Math.sin(a) };
}
