#pragma once

#include <filesystem>
#include <string>

namespace skylanders {

// UTF-8 form of a path. Unlike path::string(), never throws for characters outside the ANSI code
// page.
inline std::string Utf8Path(const std::filesystem::path& p) {
  const std::u8string u8 = p.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

}  // namespace skylanders
