const { Simulation } = require('./dist/simulation.js');
const sim = new Simulation();
// вода + уран
for (let i = 0; i < 10; i++) sim.spawnWater({ x: (i % 5) * 2 - 4, y: Math.floor(i / 5) * 2 - 1 });
sim.spawn({ elementId: 3, pos: { x: 0, y: 3 }, vel: { x: 0, y: 0 } });
sim.spawn({ elementId: 3, pos: { x: 2, y: 3 }, vel: { x: 0, y: 0 } });
sim.targetTempCelsius = 25;
sim.timeScale = 1;
sim.update(1/60); // много подшагов
// инжектируем нейтроны прямо в U
sim.injectNeutrons({ x: 0, y: 3 }, 5, 0.0253);
for (let f = 0; f < 120; f++) sim.update(1/60);
const s = sim.computeStats();
console.log('stats:', JSON.stringify(s, null, 1));
if (!isFinite(s.atomsTotal)) throw new Error('NaN!');
console.log('SMOKE OK');
