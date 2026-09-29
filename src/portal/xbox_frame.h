#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "portal/portal_device.h"

namespace skylanders::portal {

// The Xbox 360 game exchanges 32-byte frames: the header 0B 14 followed by a 30-byte payload.
constexpr size_t kFrameSize = 32;
constexpr size_t kFramePayload = kFrameSize - 2;
constexpr uint8_t kFrameHeader0 = 0x0B;
constexpr uint8_t kFrameHeader1 = 0x14;

// `frame` points at kFrameSize bytes. Returns the payload as a raw report (rest zero), or nullopt
// if the header is wrong.
std::optional<Report> ReportFromFrame(const uint8_t* frame);

// Writes kFrameSize bytes to `frame`: the header, then the first kFramePayload bytes of `report`.
void FrameFromReport(const Report& report, uint8_t* frame);

}  // namespace skylanders::portal
