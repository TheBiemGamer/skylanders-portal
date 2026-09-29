#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skylanders::portal {

struct SkylanderInfo {
  uint16_t id;
  uint16_t variant;
  std::string_view name;
  std::string_view game;
};

// The full built-in catalog, sorted by game then name -- the order any UI walking it should use.
std::span<const SkylanderInfo> AllSkylanders();

// Looks up a figure's real name/game from its id/variant. Returns nullptr if the pair isn't in
// the built-in catalog (e.g. a homebrew or unrecognized figure).
const SkylanderInfo* FindSkylander(uint16_t id, uint16_t variant);

struct FigureCatalogEntry {
  std::string name;          // filename, without extension
  std::string display_name;  // resolved Skylander name if recognized, else same as `name`
  std::string game;
  std::filesystem::path path;
};

// Recursively finds every *.dump file (case-insensitive extension) under `root`. A missing root,
// a root that is not a directory, or a root with nothing found all give an empty result — never an
// error. `game` is the top-level folder directly under `root` the file was found in ("" if the
// file sits directly under `root`). Entries are sorted by game, then by name, case-insensitively;
// entries with no game sort first.
std::vector<FigureCatalogEntry> ScanFigureCatalog(const std::filesystem::path& root);

}  // namespace skylanders::portal
