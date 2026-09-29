#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "portal/portal_device.h"

namespace skylanders::portal {

constexpr uint32_t kXamSuccess = 0;
constexpr uint32_t kXamDeviceNotConnected = 0x8007048F;  // X_ERROR_DEVICE_NOT_CONNECTED

// Implements the game side of XamInputNonControllerGetRaw/SetRaw on top of a PortalDevice: adds
// and strips the Xbox 360 frame header (0B 14) and routes 64-byte writes to the speaker.
// Not thread-safe; the game calls these from one polling thread.
class XamBridge {
 public:
  uint32_t Read(PortalDevice* device, std::span<uint8_t> buffer, uint32_t& bytes_read,
                uint16_t& state);
  uint32_t Write(PortalDevice* device, std::span<const uint8_t> buffer);

 private:
  std::optional<uint32_t> previous_status_;
};

}  // namespace skylanders::portal
