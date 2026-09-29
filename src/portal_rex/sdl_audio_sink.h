#pragma once

#include <atomic>
#include <memory>
#include <span>

#include "portal/audio.h"

struct SDL_AudioStream;

namespace skylanders {

// Plays the portal speaker's 8 kHz mono stream on the PC's default audio output. SDL resamples.
class SdlPortalAudioSink final : public portal::PortalAudioSink {
 public:
  static std::unique_ptr<SdlPortalAudioSink> Open();  // nullptr if no output device
  ~SdlPortalAudioSink() override;
  void Submit(std::span<const int16_t> samples) override;
  void SetVolume(float volume) { volume_.store(volume); }

 private:
  explicit SdlPortalAudioSink(SDL_AudioStream* stream) : stream_(stream) {}
  SDL_AudioStream* stream_;
  std::atomic<float> volume_{1.0f};
};

}  // namespace skylanders
