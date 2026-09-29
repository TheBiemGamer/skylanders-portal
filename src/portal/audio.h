#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <span>

namespace skylanders::portal {

// The Traptanium portal's speaker takes 64-byte packets: 32 samples of 16-bit PCM, 8 kHz mono.
constexpr size_t kAudioPacketSize = 64;
constexpr size_t kAudioSamplesPerPacket = 32;
constexpr int kAudioSampleRate = 8000;
using AudioPacket = std::array<uint8_t, kAudioPacketSize>;

enum class AudioSampleFormat { kSigned16LE, kUnsigned16LE, kSigned16BE, kUnsigned16BE };

// The format the game sends. Confirmed by listening during research (see docs).
inline constexpr AudioSampleFormat kPortalAudioFormat = AudioSampleFormat::kSigned16LE;

// Reply bytes 2-3 to the 'M' command that tell the game the portal has a speaker.
inline constexpr std::array<uint8_t, 2> kAudioCapableVersion = {0x01, 0x19};

std::array<int16_t, kAudioSamplesPerPacket> SamplesFromAudioPacket(const AudioPacket& packet,
                                                                    AudioSampleFormat format);

// Receives decoded speaker audio. Called on the game's thread: must not block.
class PortalAudioSink {
 public:
  virtual ~PortalAudioSink() = default;
  virtual void Submit(std::span<const int16_t> samples) = 0;
};

// Bounded packet queue between the game's thread (Push, never blocks) and a writer thread.
// When full, the oldest packet is dropped.
class AudioPacketQueue {
 public:
  explicit AudioPacketQueue(size_t capacity) : capacity_(capacity) {}
  void Push(const AudioPacket& packet);
  // Waits up to `timeout` for a packet. nullopt on timeout or after Close().
  std::optional<AudioPacket> PopWait(std::chrono::milliseconds timeout);
  size_t Dropped() const;
  void Close();

 private:
  const size_t capacity_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<AudioPacket> packets_;
  size_t dropped_ = 0;
  bool closed_ = false;
};

}  // namespace skylanders::portal
