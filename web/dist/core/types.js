"use strict";
// ============================================================
// types.ts — port of Types.hpp
// Типы атомов, нуклоны, нейтроны, стены, фабрика атома,
// spatial grid, TIP4P, тетраэдрический порядок.
// ============================================================
Object.defineProperty(exports, "__esModule", { value: true });
exports.clampNum = exports.SpatialGrid = exports.isHBondedTo = exports.countHBonds = exports.hasFreeBond = exports.isBondedTo = exports.countBonds = exports.ATOM_TYPES = exports.v2 = void 0;
exports.generateNucleons = generateNucleons;
exports.makeAtom = makeAtom;
exports.addBond = addBond;
exports.removeBond = removeBond;
exports.removeAllBondsTo = removeAllBondsTo;
exports.addHBond = addHBond;
exports.removeAllHBondsTo = removeAllHBondsTo;
exports.computeTip4pSite = computeTip4pSite;
exports.computeTetrahedralOrder = computeTetrahedralOrder;
exports.atomOutlineColor = atomOutlineColor;
exports.atomChargeLabel = atomChargeLabel;
exports.atomChargeColor = atomChargeColor;
const config_1 = require("./config");
const rng_1 = require("./rng");
const v2 = (x = 0, y = 0) => ({ x, y });
exports.v2 = v2;
exports.ATOM_TYPES = [
    { name: 'Hydrogen', symbol: 'H', mass: config_1.H_MASS, radius: 0.35, color: [240, 240, 240], maxBonds: 1, Z: 1, N: 0 },
    { name: 'Oxygen', symbol: 'O', mass: config_1.O_MASS, radius: 0.45, color: [230, 80, 80], maxBonds: 3, Z: 8, N: 8 },
    { name: 'Chlorine', symbol: 'Cl', mass: config_1.CL_MASS, radius: 0.55, color: [110, 200, 90], maxBonds: 1, Z: 17, N: 18 },
    { name: 'Uranium-235', symbol: 'U', mass: config_1.U235_MASS, radius: 0.85, color: [90, 130, 90], maxBonds: 0, Z: 92, N: 143 },
    { name: 'Sodium', symbol: 'Na', mass: config_1.NA_MASS, radius: 0.6, color: [200, 180, 240], maxBonds: 1, Z: 11, N: 12 },
    { name: 'Barium', symbol: 'Ba', mass: config_1.BA_MASS, radius: 0.7, color: [210, 180, 120], maxBonds: 0, Z: 56, N: 82 },
    { name: 'Krypton', symbol: 'Kr', mass: config_1.KR_MASS, radius: 0.6, color: [130, 220, 180], maxBonds: 0, Z: 36, N: 58 },
    { name: 'Neutron', symbol: 'n', mass: config_1.NEUTRON_MASS, radius: config_1.NUCLEON_RADIUS, color: [240, 240, 245], maxBonds: 0, Z: 0, N: 1 },
];
// ============================================================
// Генерация нуклонов — спираль Вогеля (phyllotaxis)
// (детерминированный сид по Z и N — один изотоп всегда выглядит одинаково)
// ============================================================
function generateNucleons(Z, N, nucleonR, sizeScale = config_1.NUCLEUS_SIZE_SCALE) {
    let total = Z + N;
    if (total <= 0)
        return [];
    if (total > config_1.MAX_NUCLEONS_DISPLAY) {
        const s = config_1.MAX_NUCLEONS_DISPLAY / total;
        Z = Math.max(1, Math.round(Z * s));
        N = Math.max(0, config_1.MAX_NUCLEONS_DISPLAY - Z);
        total = Z + N;
    }
    const rn = nucleonR * sizeScale;
    // Детерминированный сид: одинаковый изотоп → одинаковая картинка
    const seed = ((Z * 73856093) ^ (N * 19349663) ^ 0x9E3779B9) >>> 0;
    const rng = new rng_1.Rng(seed);
    // Радиус ядра: гексагональная упаковка кружков радиуса rn (78% площади)
    let R = rn * Math.sqrt(total / 0.78 / 2.0);
    if (R < rn)
        R = rn;
    const GOLDEN_ANGLE = 2.39996323;
    const positions = [];
    const jitter = rn * 0.1;
    for (let i = 0; i < total; ++i) {
        const angle = i * GOLDEN_ANGLE;
        const r = R * Math.sqrt((i + 0.5) / total);
        positions.push({
            x: Math.cos(angle) * r + (rng.next() * 2 - 1) * jitter,
            y: Math.sin(angle) * r + (rng.next() * 2 - 1) * jitter,
        });
    }
    // Разворот: наружные нуклоны рисуются раньше внутренних
    positions.reverse();
    // Типы P/N: случайная раздача из пула Z протонов + N нейтронов
    const types = [];
    for (let i = 0; i < Z; ++i)
        types.push(0);
    for (let i = 0; i < N; ++i)
        types.push(1);
    // Fisher-Yates
    for (let i = types.length - 1; i > 0; --i) {
        const j = Math.floor(rng.next() * (i + 1));
        const t = types[i];
        types[i] = types[j];
        types[j] = t;
    }
    const result = [];
    for (let i = 0; i < total; ++i) {
        result.push({ relPos: positions[i], type: types[i] });
    }
    return result;
}
// ============================================================
// Фабрика атома с готовым ядром
// ============================================================
function makeAtom(elementId, pos, vel) {
    const t = exports.ATOM_TYPES[elementId];
    return {
        pos: { ...pos },
        vel: { ...vel },
        elementId,
        mass: t.mass,
        radius: t.radius,
        color: t.color,
        bonds: [-1, -1, -1, -1],
        hbonds: [-1, -1, -1, -1],
        selected: false,
        age: 0,
        nucleons: generateNucleons(t.Z, t.N, config_1.NUCLEON_RADIUS, config_1.NUCLEUS_SIZE_SCALE),
    };
}
// ------------------------------------------------------------
// Хелперы для работы со связями
// ------------------------------------------------------------
const countBonds = (a) => a.bonds.filter(k => k >= 0).length;
exports.countBonds = countBonds;
const isBondedTo = (a, j) => a.bonds.includes(j);
exports.isBondedTo = isBondedTo;
function addBond(a, j) {
    for (let s = 0; s < 4; ++s)
        if (a.bonds[s] < 0) {
            a.bonds[s] = j;
            return;
        }
}
function removeBond(a, j) {
    for (let s = 0; s < 4; ++s)
        if (a.bonds[s] === j) {
            a.bonds[s] = -1;
            return;
        }
}
function removeAllBondsTo(a, j) {
    for (let s = 0; s < 4; ++s)
        if (a.bonds[s] === j)
            a.bonds[s] = -1;
}
const hasFreeBond = (a) => (0, exports.countBonds)(a) < exports.ATOM_TYPES[a.elementId]?.maxBonds;
exports.hasFreeBond = hasFreeBond;
const countHBonds = (a) => a.hbonds.filter(k => k >= 0).length;
exports.countHBonds = countHBonds;
const isHBondedTo = (a, j) => a.hbonds.includes(j);
exports.isHBondedTo = isHBondedTo;
function addHBond(a, j) {
    for (let s = 0; s < 4; ++s)
        if (a.hbonds[s] < 0) {
            a.hbonds[s] = j;
            return;
        }
}
function removeAllHBondsTo(a, j) {
    for (let s = 0; s < 4; ++s)
        if (a.hbonds[s] === j)
            a.hbonds[s] = -1;
}
// ------------------------------------------------------------
// SpatialGrid
// ------------------------------------------------------------
const clampI = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;
class SpatialGrid {
    constructor() {
        this.cellSize = 1;
        this.min = 0;
        this._cols = 1;
        this._rows = 1;
        this.cells = [];
    }
    init(worldMin, worldSize, cellSize) {
        this.cellSize = cellSize;
        this.min = worldMin;
        this._cols = Math.max(1, Math.ceil(worldSize / cellSize));
        this._rows = this._cols;
        this.cells = Array.from({ length: this._cols * this._rows }, () => []);
    }
    clear() { for (const c of this.cells)
        c.length = 0; }
    insert(idx, pos) {
        const cx = clampI(Math.trunc((pos.x - this.min) / this.cellSize), 0, this._cols - 1);
        const cy = clampI(Math.trunc((pos.y - this.min) / this.cellSize), 0, this._rows - 1);
        this.cells[cy * this._cols + cx].push(idx);
    }
    cellX(x) { return clampI(Math.trunc((x - this.min) / this.cellSize), 0, this._cols - 1); }
    cellY(y) { return clampI(Math.trunc((y - this.min) / this.cellSize), 0, this._rows - 1); }
    at(cx, cy) { return this.cells[cy * this._cols + cx]; }
    cols() { return this._cols; }
    rows() { return this._rows; }
}
exports.SpatialGrid = SpatialGrid;
// ------------------------------------------------------------
// TIP4P: виртуальный сайт M на кислороде
// ------------------------------------------------------------
function computeTip4pSite(atoms, oIdx) {
    if (oIdx < 0 || oIdx >= atoms.length)
        return null;
    const o = atoms[oIdx];
    if (o.elementId !== 1)
        return null;
    let h1 = -1, h2 = -1;
    for (const k of o.bonds) {
        if (k < 0 || k >= atoms.length)
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
    if (h1 < 0 || h2 < 0)
        return null;
    const d1 = { x: atoms[h1].pos.x - o.pos.x, y: atoms[h1].pos.y - o.pos.y };
    const d2 = { x: atoms[h2].pos.x - o.pos.x, y: atoms[h2].pos.y - o.pos.y };
    const L1 = Math.hypot(d1.x, d1.y);
    const L2 = Math.hypot(d2.x, d2.y);
    if (L1 < 1e-6 || L2 < 1e-6)
        return null;
    const bis = { x: d1.x / L1 + d2.x / L2, y: d1.y / L1 + d2.y / L2 };
    const bl = Math.hypot(bis.x, bis.y);
    if (bl < 1e-6)
        return null;
    return { x: o.pos.x - (bis.x / bl) * config_1.TIP4P_OM_DIST, y: o.pos.y - (bis.y / bl) * config_1.TIP4P_OM_DIST };
}
// ------------------------------------------------------------
// Тетраэдрический порядок параметр (для статистики воды)
// ------------------------------------------------------------
function computeTetrahedralOrder(atoms, oIdx) {
    let neighborCount = 0;
    if (oIdx < 0 || oIdx >= atoms.length)
        return { q: 0, neighborCount };
    const o = atoms[oIdx];
    if (o.elementId !== 1)
        return { q: 0, neighborCount };
    const dirs = [];
    for (const k of [...o.bonds, ...o.hbonds]) {
        if (k < 0 || k >= atoms.length)
            continue;
        const d = { x: atoms[k].pos.x - o.pos.x, y: atoms[k].pos.y - o.pos.y };
        const L = Math.hypot(d.x, d.y);
        if (L > 1e-6)
            dirs.push({ x: d.x / L, y: d.y / L });
    }
    neighborCount = dirs.length;
    if (neighborCount < 4)
        return { q: 0, neighborCount };
    if (dirs.length > 4)
        dirs.length = 4;
    let sum = 0;
    for (let i = 0; i < dirs.length; ++i) {
        for (let j = i + 1; j < dirs.length; ++j) {
            const c = (0, exports.clampNum)(dirs[i].x * dirs[j].x + dirs[i].y * dirs[j].y, -1, 1);
            const t = c + 1 / 3;
            sum += t * t;
        }
    }
    return { q: 1 - (3 / 8) * sum, neighborCount };
}
const clampNum = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;
exports.clampNum = clampNum;
// ------------------------------------------------------------
// Цвет обводки атома
// ------------------------------------------------------------
function atomOutlineColor(elementId) {
    if (elementId === 1)
        return [196, 68, 68];
    if (elementId === 2)
        return [94, 170, 77];
    if (elementId === 3)
        return [80, 110, 80];
    if (elementId === 4)
        return [160, 144, 192];
    if (elementId === 5)
        return [168, 144, 96]; // Ba
    if (elementId === 6)
        return [104, 176, 144]; // Kr
    return [204, 204, 204];
}
// ------------------------------------------------------------
// Отображаемые заряды (тумблер Charges)
// ------------------------------------------------------------
function atomChargeLabel(elementId) {
    if (elementId === 0)
        return '+'; // H (частично +)
    if (elementId === 1)
        return '-'; // O (частично -)
    if (elementId === 2)
        return '-'; // Cl-
    if (elementId === 4)
        return '+'; // Na+
    return '';
}
function atomChargeColor(elementId) {
    if (elementId === 0)
        return [255, 200, 200];
    if (elementId === 1)
        return [255, 130, 130];
    if (elementId === 2)
        return [150, 230, 150];
    if (elementId === 4)
        return [160, 180, 255];
    return [255, 255, 255];
}
