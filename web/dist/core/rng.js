"use strict";
// ============================================================
// rng.ts — замена std::mt19937 + distributions
// Быстрый mulberry32 + Box-Muller для нормального шума.
// Точное совпадение последовательностей с C++ не требуется:
// вся стохастика (термостат, Монте-Карло транспорта) от этого
// не зависит по смыслу.
// ============================================================
Object.defineProperty(exports, "__esModule", { value: true });
exports.uniform01 = exports.Rng = void 0;
exports.randomUnitVector2D = randomUnitVector2D;
class Rng {
    constructor(seed = 0x9E3779B9) {
        this.s = seed >>> 0;
    }
    reseed(seed) { this.s = seed >>> 0; }
    /** uniform [0,1) */
    next() {
        this.s = (this.s + 0x6D2B79F5) >>> 0;
        let t = this.s;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    }
    uniform(a, b) { return a + (b - a) * this.next(); }
    /** стандартное нормальное (Box–Muller) — аналог std::normal_distribution<float> */
    normal() {
        let u1 = this.next();
        if (u1 < 1e-12)
            u1 = 1e-12;
        const u2 = this.next();
        return Math.sqrt(-2.0 * Math.log(u1)) * Math.cos(2.0 * Math.PI * u2);
    }
}
exports.Rng = Rng;
const uniform01 = (rng) => rng.next();
exports.uniform01 = uniform01;
function randomUnitVector2D(rng) {
    const a = rng.next() * 2.0 * Math.PI;
    return { x: Math.cos(a), y: Math.sin(a) };
}
