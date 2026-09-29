"""Regenerates src/portal/skylander_catalog_data.h.

Scans every .dump file under the user's local dump collection, reads each figure's id/variant
bytes, and writes out the compiled-in {id, variant, name, game} catalog used by the in-game
figure picker and creator. Games are ordered by real-world release order (folders are numbered
accordingly), not alphabetically. Run with:
    python3 tools/extract_skylander_catalog.py <path to dump collection root>
"""

import argparse
import datetime
import re
import struct
from pathlib import Path
from collections import defaultdict

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("dumps_root", type=Path, help="Root folder containing one subfolder per game of .dump files")
args = parser.parse_args()

root = args.dumps_root
games = sorted([d for d in root.iterdir() if d.is_dir()])

def clean_game(g):
    g = re.sub(r'^\d+\.\s*', '', g)
    g = g.replace('_s ', "'s ")
    return g

def release_order(g):
    m = re.match(r'^(\d+)\.', g)
    return int(m.group(1)) if m else 999

by_key = defaultdict(list)
for g in games:
    for f in g.rglob("*.dump"):
        data = f.read_bytes()
        sky_id = struct.unpack_from("<H", data, 0x10)[0]
        variant = struct.unpack_from("<H", data, 0x1C)[0]
        by_key[(sky_id, variant)].append((f.stem, clean_game(g.name), release_order(g.name)))

table = []
for (sky_id, variant), entries in by_key.items():
    game = entries[0][1]
    order = entries[0][2]
    name = min((n for n, _, _ in entries), key=len)
    table.append((sky_id, variant, name, game, order))

table.sort(key=lambda e: (e[4], e[2].lower()))

assert len(table) == 648, f"expected 648 entries, got {len(table)}"
assert len({e[3] for e in table}) == 6, "expected exactly 6 games"

lines = []
lines.append("#pragma once")
lines.append("")
lines.append("#include <array>")
lines.append("")
lines.append('#include "portal/figure_catalog.h"')
lines.append("")
lines.append("namespace skylanders::portal {")
lines.append("")
lines.append(f"// Generated {datetime.date.today().isoformat()}.")
lines.append("// Sorted by game release order (Spyro's Adventure -> Imaginators), then name.")
lines.append(f"inline constexpr std::array<SkylanderInfo, {len(table)}> kSkylanderCatalog = {{{{")
for sky_id, variant, name, game, _order in table:
    lines.append(f'    {{{sky_id}, {variant}, "{name}", "{game}"}},')
lines.append("}};")
lines.append("")
lines.append("}  // namespace skylanders::portal")
lines.append("")

Path("src/portal/skylander_catalog_data.h").write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {len(table)} entries across {len({e[3] for e in table})} games")
