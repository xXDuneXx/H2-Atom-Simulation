// ============================================================
// renderer.ts — Canvas2D рендер мира (порт логики отрисовки AtomSimulation.cpp)
// Камера: центр коробки в центре канваса, scale = px на Å.
// ============================================================

import { type Atom, type Neutron, type Gamma, type Wall, ATOM_TYPES, atomOutlineColor } from './core/types';
import { NUCLEON_RADIUS } from './core/config';

export interface Camera { scale: number; }

export class Renderer {
  private ctx: CanvasRenderingContext2D;
  canvas: HTMLCanvasElement;
  scale = 18;   // px на Å

  constructor(canvas: HTMLCanvasElement) {
    this.canvas = canvas;
    const ctx = canvas.getContext('2d');
    if (!ctx) throw new Error('no 2d context');
    this.ctx = ctx;
  }

  worldToScreen(x: number, y: number): [number, number] {
    return [this.canvas.width / 2 + x * this.scale, this.canvas.height / 2 + y * this.scale];
  }
  screenToWorld(sx: number, sy: number): [number, number] {
    return [(sx - this.canvas.width / 2) / this.scale, (sy - this.canvas.height / 2) / this.scale];
  }

  resize(w: number, h: number, dpr: number): void {
    this.canvas.width = w * dpr;
    this.canvas.height = h * dpr;
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    // после setTransform width/height в пикселях буфера; рисуем в CSS-координатах
  }

  clear(): void {
    const c = this.ctx;
    c.fillStyle = '#0b0e14';
    c.fillRect(0, 0, this.canvas.width, this.canvas.height);
  }

  drawBox(boxX: number, boxY: number): void {
    const c = this.ctx;
    const [x0, y0] = this.worldToScreen(-boxX / 2, -boxY / 2);
    const [x1, y1] = this.worldToScreen(boxX / 2, boxY / 2);
    c.strokeStyle = 'rgba(120,150,200,0.35)';
    c.lineWidth = 2;
    c.strokeRect(x0, y0, x1 - x0, y1 - y0);
  }

  drawWalls(walls: Wall[]): void {
    const c = this.ctx;
    c.lineCap = 'round';
    for (const w of walls) {
      const [ax, ay] = this.worldToScreen(w.a.x, w.a.y);
      const [bx, by] = this.worldToScreen(w.b.x, w.b.y);
      c.strokeStyle = w.selected ? '#ffd166' : '#8a93a6';
      c.lineWidth = Math.max(3, 0.1 * this.scale);
      c.beginPath(); c.moveTo(ax, ay); c.lineTo(bx, by); c.stroke();
    }
  }

  drawBonds(atoms: Atom[]): void {
    const c = this.ctx;
    // ковалентные
    for (let i = 0; i < atoms.length; ++i) {
      const a = atoms[i];
      for (const j of a.bonds) {
        if (j < 0 || j <= i || j >= atoms.length) continue;
        const b = atoms[j];
        const [x1, y1] = this.worldToScreen(a.pos.x, a.pos.y);
        const [x2, y2] = this.worldToScreen(b.pos.x, b.pos.y);
        c.strokeStyle = 'rgba(200,210,230,0.7)';
        c.lineWidth = 2.5;
        c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.stroke();
      }
    }
    // водородные — пунктир
    for (let i = 0; i < atoms.length; ++i) {
      const a = atoms[i];
      for (const j of a.hbonds) {
        if (j < 0 || j <= i || j >= atoms.length) continue;
        const b = atoms[j];
        const [x1, y1] = this.worldToScreen(a.pos.x, a.pos.y);
        const [x2, y2] = this.worldToScreen(b.pos.x, b.pos.y);
        c.strokeStyle = 'rgba(120,180,255,0.45)';
        c.lineWidth = 1.5;
        c.setLineDash([4, 4]);
        c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.stroke();
        c.setLineDash([]);
      }
    }
  }

  drawAtoms(atoms: Atom[], showNucleons: boolean): void {
    const c = this.ctx;
    for (const a of atoms) {
      const [sx, sy] = this.worldToScreen(a.pos.x, a.pos.y);
      const rPx = Math.max(2, a.radius * this.scale);

      if (showNucleons && a.nucleons.length > 0) {
        // нуклоны
        for (const nuc of a.nucleons) {
          const nx = sx + nuc.relPos.x * this.scale;
          const ny = sy + nuc.relPos.y * this.scale;
          const nr = Math.max(1.2, NUCLEON_RADIUS * 0.5 * this.scale);
          c.fillStyle = nuc.type === 0 ? '#e05a5a' : '#c9ccd6';
          c.beginPath(); c.arc(nx, ny, nr, 0, Math.PI * 2); c.fill();
        }
      } else {
        const [R, G, B] = a.color;
        c.fillStyle = `rgb(${R},${G},${B})`;
        c.beginPath(); c.arc(sx, sy, rPx, 0, Math.PI * 2); c.fill();
        const [or_, og, ob] = atomOutlineColor(a.elementId);
        c.strokeStyle = `rgb(${or_},${og},${ob})`;
        c.lineWidth = 1.5;
        c.stroke();
      }

      if (a.selected) {
        c.strokeStyle = '#ffd166';
        c.lineWidth = 2;
        c.beginPath(); c.arc(sx, sy, rPx + 4, 0, Math.PI * 2); c.stroke();
      }
    }

    // символы при крупном масштабе
    if (this.scale > 25) {
      c.fillStyle = 'rgba(10,12,18,0.9)';
      c.font = `${Math.min(14, this.scale * 0.5)}px sans-serif`;
      c.textAlign = 'center';
      c.textBaseline = 'middle';
      for (const a of atoms) {
        const [sx, sy] = this.worldToScreen(a.pos.x, a.pos.y);
        const t = ATOM_TYPES[a.elementId];
        if (t) c.fillText(t.symbol, sx, sy);
      }
    }
  }

  drawNeutrons(neutrons: Neutron[]): void {
    const c = this.ctx;
    c.fillStyle = '#eef2ff';
    for (const n of neutrons) {
      const [sx, sy] = this.worldToScreen(n.pos.x, n.pos.y);
      c.beginPath(); c.arc(sx, sy, 3, 0, Math.PI * 2); c.fill();
      // короткий хвост по направлению
      c.strokeStyle = 'rgba(238,242,255,0.5)';
      c.lineWidth = 1;
      c.beginPath();
      c.moveTo(sx, sy);
      c.lineTo(sx - n.dir.x * 10, sy - n.dir.y * 10);
      c.stroke();
    }
  }

  drawGammas(gammas: Gamma[]): void {
    const c = this.ctx;
    c.strokeStyle = 'rgba(255,220,80,0.8)';
    c.lineWidth = 1.5;
    for (const g of gammas) {
      const [sx, sy] = this.worldToScreen(g.pos.x, g.pos.y);
      c.beginPath();
      c.moveTo(sx - g.dir.x * 8, sy - g.dir.y * 8);
      c.lineTo(sx + g.dir.x * 8, sy + g.dir.y * 8);
      c.stroke();
    }
  }

  render(scene: {
    atoms: Atom[]; neutrons: Neutron[]; gammas: Gamma[]; walls: Wall[];
    boxSizeX: number; boxSizeY: number; showNucleons: boolean;
  }): void {
    this.clear();
    this.drawBox(scene.boxSizeX, scene.boxSizeY);
    this.drawWalls(scene.walls);
    this.drawBonds(scene.atoms);
    this.drawAtoms(scene.atoms, scene.showNucleons);
    this.drawNeutrons(scene.neutrons);
    this.drawGammas(scene.gammas);
  }
}
