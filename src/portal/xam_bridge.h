#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "portal/g726.h"
#include "portal/portal_device.h"

namespace skylanders::portal {

constexpr uint32_t kXamSuccess = 0;
constexpr uint32_t kXamDeviceNotConnected = 0x8007048F;  // X_ERROR_DEVICE_NOT_CONNECTED

// Implements the game side of XamInputNonControllerGetRaw/SetRaw on top of a PortalDevice: adds
// and strips the Xbox 360 frame header (0B 14 for commands and replies) and decodes speaker audio
// frames (0B 17, G.726) into PCM for the device.
// Not thread-safe; the game calls these from one polling thread.
class XamBridge {
 public:
  uint32_t Read(PortalDevice* device, std::span<uint8_t> buffer, uint32_t& bytes_read,
                uint16_t& state);
  uint32_t Write(PortalDevice* device, std::span<const uint8_t> buffer);

 private:
  bool delivered_last_ = false;  // the previous Read handed the game a report
  G726Decoder speaker_decoder_;  // one stream; reset when the game switches the speaker ('M')
  std::vector<int16_t> pcm_;     // reused decode buffer
};

}  // namespace skylanders::portal
