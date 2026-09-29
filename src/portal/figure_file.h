#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "portal/portal_device.h"

namespace skylanders::portal {

// Reads a raw figure dump: exactly kFigureSize (1024) bytes, 64 blocks of 16. Returns nullopt if the
// path is not a regular file or its size is anything else. The file is only read, never modified.
std::optional<FigureData> LoadFigureFile(const std::filesystem::path& path);

// Atomically writes `data` to `path`: writes to a temporary file next to it, then renames the
// temporary file over `path`. A crash or power loss mid-save leaves the original file untouched
// rather than a half-written one. Returns false, and leaves `path` unchanged, if either step fails
// (for example the parent directory does not exist); the temporary file is cleaned up either way.
bool SaveFigureFileAtomic(const std::filesystem::path& path, const FigureData& data);

// Reads the Skylander id / variant a figure's bytes encode: little-endian uint16 at a fixed
// offset (0x10 for id, 0x1C for variant). Never fails -- any 1024-byte figure has these bytes.
uint16_t ReadFigureId(const FigureData& data);
uint16_t ReadFigureVariant(const FigureData& data);

// Builds a blank, valid figure for `id`/`variant`: correct manufacturer bytes, id, variant, CRC,
// and Mifare sector-trailer access bits (see docs/architecture.md, "Figures"). The 4-byte serial is normally
// random; the explicit-serial overload exists so callers (tests) can get a deterministic result.
FigureData CreateBlankFigure(uint16_t id, uint16_t variant);
FigureData CreateBlankFigure(uint16_t id, uint16_t variant, std::array<uint8_t, 4> serial);

// Returns a path under `dir` for `name` that does not currently exist: "<name>.dump", or
// "<name> (2).dump", "<name> (3).dump", ... on collision. Never creates `dir`, the returned path,
// or anything else -- it only picks a name.
std::filesystem::path UniqueFigurePath(const std::filesystem::path& dir, std::string_view name);

}  // namespace skylanders::portal
