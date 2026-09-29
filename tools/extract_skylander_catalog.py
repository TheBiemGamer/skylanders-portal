"""Regenerates src/portal/skylander_catalog_data.h.

Scans every .dump file under the user's local dump collection, reads each figure's id/variant
bytes, and writes out the compiled-in {id, variant, name, game, category} catalog used by the in-game
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

# The figure picker groups each game's figures by the collection's second-level folder. Series
# folders ("New (Series 1)", "Returning (...)") and "Figures" are the core figures; the in-game
# variant folders get short names.
CATEGORY_NAMES = {
    "Figures": "Cores",
    "Magic Items": "Items",
    "In-Game Variants": "Variants",
    "In-Game Variant SuperChargers": "Variants",
    "In-Game Variant Vehicles": "Vehicle Variants",
    "In-Game Variant Senseis": "Sensei Variants",
    "In-Game Variant Villain Senseis": "Villain Variants",
}

def category_of(game_dir, f):
    folder = f.relative_to(game_dir).parts[0]
    m = re.match(r'^(\d+)\)\s*(.*)$', folder)
    order, name = (int(m.group(1)), m.group(2)) if m else (999, folder)
    name = name.replace('_s ', "'s ")
    if name.startswith("New (") or name.startswith("Returning ("):
        name = "Cores"
    return CATEGORY_NAMES.get(name, name), order

def release_order(g):
    m = re.match(r'^(\d+)\.', g)
    return int(m.group(1)) if m else 999

by_key = defaultdict(list)
villain_only_traps = {}
for g in games:
    for f in g.rglob("*.dump"):
        # A trap holding a villain has the same id/variant as the empty crystal trap of that shape
        # (the villain lives in its save data), so villain dumps would mislabel the trap. Villains
        # come from portal/trap_villain.cpp instead. A trap shape seen only in villain dumps is
        # still a real trap, listed as "<Element> Trap" below.
        data = f.read_bytes()
        sky_id = struct.unpack_from("<H", data, 0x10)[0]
        variant = struct.unpack_from("<H", data, 0x1C)[0]
        if "Trappable Villain" in str(f):
            if f.parent.name != "Trappable Villains" and "Variants" not in f.parent.name:
                villain_only_traps.setdefault((sky_id, variant), (f.parent.name, g))
            continue
        category, category_order = category_of(g, f)
        by_key[(sky_id, variant)].append(
            (f.stem, clean_game(g.name), release_order(g.name), category, category_order))

for key, (element, g) in villain_only_traps.items():
    if key not in by_key:
        traps_dir = next(d for d in g.iterdir() if d.is_dir() and d.name.endswith(") Traps"))
        by_key[key].append((f"{element} Trap", clean_game(g.name), release_order(g.name),
                            *category_of(g, traps_dir / "x.dump")))

table = []
for (sky_id, variant), entries in by_key.items():
    name, game, order, category, category_order = min(entries, key=lambda e: len(e[0]))
    table.append((sky_id, variant, name, game, order, category, category_order))

# A merged category ("Cores" from several series folders) sorts at its first folder.
first_order = defaultdict(lambda: 999)
for e in table:
    first_order[(e[3], e[5])] = min(first_order[(e[3], e[5])], e[6])
table.sort(key=lambda e: (e[4], first_order[(e[3], e[5])], e[2].lower()))

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
lines.append("// Sorted by game release order (Spyro's Adventure -> Imaginators), then category, then name.")
lines.append(f"inline constexpr std::array<SkylanderInfo, {len(table)}> kSkylanderCatalog = {{{{")
for sky_id, variant, name, game, _order, category, _category_order in table:
    lines.append(f'    {{{sky_id}, {variant}, "{name}", "{game}", "{category}"}},')
lines.append("}};")
lines.append("")
lines.append("}  // namespace skylanders::portal")
lines.append("")

Path("src/portal/skylander_catalog_data.h").write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {len(table)} entries across {len({e[3] for e in table})} games")
