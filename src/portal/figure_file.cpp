#include "portal/figure_file.h"

#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace skylanders::portal {

std::optional<FigureData> LoadFigureFile(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
  if (std::filesystem::file_size(path, ec) != kFigureSize || ec) return std::nullopt;

  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  FigureData data{};
  in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (in.gcount() != static_cast<std::streamsize>(data.size())) return std::nullopt;
  return data;
}

bool SaveFigureFileAtomic(const std::filesystem::path& path, const FigureData& data) {
  std::filesystem::path tmp = path;
  tmp += L".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out.good()) {
      out.close();
      std::error_code ignore;
      std::filesystem::remove(tmp, ignore);
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::error_code ignore;
    std::filesystem::remove(tmp, ignore);
    return false;
  }
  return true;
}

namespace {

uint16_t Crc16Ccitt(const uint8_t* data, size_t size, uint16_t crc) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                            : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

void WriteU16LE(FigureData& data, size_t offset, uint16_t value) {
  data[offset] = static_cast<uint8_t>(value & 0xFF);
  data[offset + 1] = static_cast<uint8_t>(value >> 8);
}

}  // namespace

uint16_t ReadFigureId(const FigureData& data) {
  return static_cast<uint16_t>(data[0x10] | (static_cast<uint16_t>(data[0x11]) << 8));
}

uint16_t ReadFigureVariant(const FigureData& data) {
  return static_cast<uint16_t>(data[0x1C] | (static_cast<uint16_t>(data[0x1D]) << 8));
}

FigureData CreateBlankFigure(uint16_t id, uint16_t variant, std::array<uint8_t, 4> serial) {
  FigureData data{};
  data[0] = serial[0];
  data[1] = serial[1];
  data[2] = serial[2];
  data[3] = serial[3];
  data[4] = static_cast<uint8_t>(serial[0] ^ serial[1] ^ serial[2] ^ serial[3]);
  data[5] = 0x81;
  data[6] = 0x01;
  data[7] = 0x0F;
  WriteU16LE(data, 0x10, id);
  WriteU16LE(data, 0x1C, variant);
  const uint16_t crc = Crc16Ccitt(data.data(), 0x1E, 0xFFFF);
  WriteU16LE(data, 0x1E, crc);
  // Little-endian bytes of the sector-trailer access-bit constants (0x690F0F0F for sector 0,
  // 0x69080F7F for sectors 1-15) -- these are raw memcpy'd bytes of a native uint32_t, not the
  // constant's digits read left-to-right.
  for (int sector = 0; sector < 16; ++sector) {
    const size_t offset = static_cast<size_t>(sector) * 0x40 + 0x36;
    if (sector == 0) {
      data[offset] = 0x0F;
      data[offset + 1] = 0x0F;
      data[offset + 2] = 0x0F;
      data[offset + 3] = 0x69;
    } else {
      data[offset] = 0x7F;
      data[offset + 1] = 0x0F;
      data[offset + 2] = 0x08;
      data[offset + 3] = 0x69;
    }
  }
  return data;
}

FigureData CreateBlankFigure(uint16_t id, uint16_t variant) {
  std::random_device rd;
  std::array<uint8_t, 4> serial{};
  for (auto& b : serial) b = static_cast<uint8_t>(rd());
  return CreateBlankFigure(id, variant, serial);
}

std::filesystem::path UniqueFigurePath(const std::filesystem::path& dir, std::string_view name) {
  // Built entirely from UTF-8 (path::u8string()/the char8_t constructor), not path::native(): that
  // returns std::wstring on Windows but std::string on Linux/macOS, so concatenating it with wide
  // string literals compiles only on Windows.
  const std::u8string u8name(reinterpret_cast<const char8_t*>(name.data()), name.size());
  const std::u8string base = std::filesystem::path(u8name).u8string();
  auto with_suffix = [&](const std::string& suffix) {
    const std::u8string u8suffix(reinterpret_cast<const char8_t*>(suffix.data()), suffix.size());
    return dir / std::filesystem::path(base + u8suffix);
  };
  std::filesystem::path candidate = with_suffix(".dump");
  for (int n = 2; std::filesystem::exists(candidate); ++n) {
    candidate = with_suffix(" (" + std::to_string(n) + ").dump");
  }
  return candidate;
}

}  // namespace skylanders::portal
