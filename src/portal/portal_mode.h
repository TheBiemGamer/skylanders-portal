#pragma once

#include <optional>
#include <string_view>

namespace skylanders::portal {

enum class PortalMode { kNone, kSoftware, kUsb };

// Accepts "none", "software", or "usb" (any case, surrounding whitespace ignored). Anything else,
// including modes that do not exist, gives nullopt.
std::optional<PortalMode> ParsePortalMode(std::string_view text);

}  // namespace skylanders::portal
