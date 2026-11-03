#!/usr/bin/env node
/*
 * Erzeugt esp32-epaper/BerlinUBahnEPD/lines_geo.h aus der index.html:
 * pro Linie eine (ausgeduennte) Punktfolge, gemeinsam normiert auf das
 * 200x200-Display, damit die geografische Form/Lage stimmt.
 *
 *   node esp32-epaper/gen_geo.js
 */
const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "index.html"), "utf8");

// STATIONS: "Name":[x,y]
const coords = {};
for (const m of html.matchAll(/"([^"]+)"\s*:\s*\[(\d+),\s*(\d+)\]/g)) {
  coords[m[1]] = [parseInt(m[2]), parseInt(m[3])];
}
// Mehringdamm wird im JS nachgetragen
if (!coords["Mehringdamm"]) coords["Mehringdamm"] = [500, 438];

// LINES: { id:"U1", color:"...", stations:[ ... ] }
const lines = [];
for (const m of html.matchAll(/\{\s*id:\s*"(U\d)"\s*,\s*color:\s*"[^"]+"\s*,\s*stations:\s*\[([\s\S]*?)\]\s*\}/g)) {
  const id = m[1];
  const names = [...m[2].matchAll(/"([^"]+)"/g)].map(x => x[1]);
  const pts = names.map(n => coords[n]).filter(Boolean);
  lines.push({ id, pts });
}
lines.sort((a, b) => a.id.localeCompare(b.id));

// Gemeinsame Begrenzung ueber alle Punkte
let minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
for (const l of lines) for (const [x, y] of l.pts) {
  if (x < minX) minX = x; if (x > maxX) maxX = x;
  if (y < minY) minY = y; if (y > maxY) maxY = y;
}
// Zielbereich auf dem Display (Platz oben fuer Badge lassen)
const BX0 = 6, BY0 = 40, BX1 = 194, BY1 = 194;
const sx = (BX1 - BX0) / (maxX - minX);
const sy = (BY1 - BY0) / (maxY - minY);
const s = Math.min(sx, sy);
const offX = BX0 + ((BX1 - BX0) - (maxX - minX) * s) / 2;
const offY = BY0 + ((BY1 - BY0) - (maxY - minY) * s) / 2;
const proj = ([x, y]) => [Math.round(offX + (x - minX) * s), Math.round(offY + (y - minY) * s)];

// Punktzahl je Linie begrenzen (gleichmaessig ausduennen)
function decimate(pts, maxN) {
  if (pts.length <= maxN) return pts;
  const out = [];
  for (let i = 0; i < maxN; i++) out.push(pts[Math.round(i * (pts.length - 1) / (maxN - 1))]);
  return out;
}

let out = "// AUTO-GENERIERT von gen_geo.js aus index.html - nicht editieren.\n";
out += "// Punktfolgen je Linie (U1..U9), normiert auf das 200x200-Display.\n#pragma once\n#include <Arduino.h>\n\n";
const names = [];
for (const l of lines) {
  const pts = decimate(l.pts, 26).map(proj);
  names.push(l.id);
  out += `const uint8_t GEO_${l.id}[] PROGMEM = {`;
  out += pts.map(p => `${p[0]},${p[1]}`).join(", ");
  out += `};\n`;
}
out += `\nstruct GeoLine { const uint8_t *pts; uint8_t n; };\n`;
out += `const GeoLine GEO[] = {\n`;
for (const id of names) out += `  { GEO_${id}, sizeof(GEO_${id}) / 2 },\n`;
out += `};\n`;

const dst = path.join(root, "esp32-epaper", "BerlinUBahnEPD", "lines_geo.h");
fs.writeFileSync(dst, out);
console.log("geschrieben:", path.relative(root, dst));
console.log("Linien:", names.join(", "));
console.log("Bounds:", minX, minY, maxX, maxY, "scale", s.toFixed(3));
