#include "portal/audio.h"

namespace skylanders::portal {

std::array<int16_t, kAudioSamplesPerPacket> SamplesFromAudioPacket(const AudioPacket& packet,
                                                                    AudioSampleFormat format) {
  std::array<int16_t, kAudioSamplesPerPacket> out{};
  const bool big_endian =
      format == AudioSampleFormat::kSigned16BE || format == AudioSampleFormat::kUnsigned16BE;
  const bool is_unsigned =
      format == AudioSampleFormat::kUnsigned16LE || format == AudioSampleFormat::kUnsigned16BE;
  for (size_t i = 0; i < kAudioSamplesPerPacket; i++) {
    const uint8_t first = packet[i * 2], second = packet[i * 2 + 1];
    uint16_t raw = big_endian ? uint16_t(first << 8 | second) : uint16_t(second << 8 | first);
    if (is_unsigned) raw ^= 0x8000;  // unsigned midpoint 0x8000 -> signed 0
    out[i] = static_cast<int16_t>(raw);
  }
  return out;
}

void AudioPacketQueue::Push(const AudioPacket& packet) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (closed_) return;
    if (packets_.size() >= capacity_) {
      packets_.pop_front();
      ++dropped_;
    }
    packets_.push_back(packet);
  }
  cv_.notify_one();
}

std::optional<AudioPacket> AudioPacketQueue::PopWait(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mu_);
  cv_.wait_for(lock, timeout, [this] { return closed_ || !packets_.empty(); });
  if (packets_.empty()) return std::nullopt;
  AudioPacket p = packets_.front();
  packets_.pop_front();
  return p;
}

size_t AudioPacketQueue::Dropped() const {
  std::lock_guard<std::mutex> lock(mu_);
  return dropped_;
}

void AudioPacketQueue::Close() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    closed_ = true;
    packets_.clear();
  }
  cv_.notify_all();
}

}  // namespace skylanders::portal
