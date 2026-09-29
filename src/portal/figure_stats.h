#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "portal/portal_device.h"

namespace skylanders::portal {

// A Skylander's in-game progress, decoded from its save-data bytes.
struct FigureStats {
  uint8_t level;
  uint32_t gold;
  std::string nickname;
};

// Decodes level/gold/nickname from a figure's raw bytes. Returns nullopt for a figure with no
// save data yet (freshly created, or any other all-zero save-data region) or bytes that don't
// checksum-validate as real save data -- never a guessed or partial result.
//
// Gameplay data lives in AES-128-ECB-encrypted "areas" (community reverse-engineering: see
// Marijn Kneppers, "Reverse engineering Skylanders' Toys-to-life mechanics", 2024). Verified
// against real dumps in this project: the per-block key is MD5(first 0x20 bytes of the tag +
// the 1-byte block index + a 53-byte constant, " Copyright (C) 2010 Activision. All Rights
// Reserved. " -- note the trailing space, easy to drop), and the area's own CRC16/CCITT-FALSE
// checksum (block offset 0x0E, with THAT field itself replaced by the literal bytes 0x05,0x00
// before computing -- not zeroed) is what's used to confirm the decrypt is correct before
// trusting any field from this block. Level comes from a 24-bit XP value at offset 0x00 of this
// same checksummed block (cross-checked against the independent SkyReader project's
// Skylander::getXP(), which reads the same three bytes) -- exactly as trustworthy as gold.
std::optional<FigureStats> ParseFigureStats(const FigureData& data);

}  // namespace skylanders::portal
