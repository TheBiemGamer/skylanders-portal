#include "portal_rex/sdl_audio_sink.h"

#include <algorithm>
#include <array>

#include <SDL3/SDL.h>
#include <rex/logging.h>

namespace skylanders {

std::unique_ptr<SdlPortalAudioSink> SdlPortalAudioSink::Open() {
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    REXLOG_WARN("Portal: cannot start audio for the portal speaker: {}", SDL_GetError());
    return nullptr;
  }
  SDL_AudioSpec spec{SDL_AUDIO_S16, 1, portal::kAudioSampleRate};
  SDL_AudioStream* stream =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!stream) {
    REXLOG_WARN("Portal: cannot open an audio device for the portal speaker: {}", SDL_GetError());
    return nullptr;
  }
  SDL_ResumeAudioStreamDevice(stream);
  return std::unique_ptr<SdlPortalAudioSink>(new SdlPortalAudioSink(stream));
}

SdlPortalAudioSink::~SdlPortalAudioSink() { SDL_DestroyAudioStream(stream_); }

void SdlPortalAudioSink::Submit(std::span<const int16_t> samples) {
  const float v = std::clamp(volume_.load(), 0.0f, 1.0f);
  std::array<int16_t, 64> scaled{};
  while (!samples.empty()) {
    const size_t n = std::min(samples.size(), scaled.size());
    for (size_t i = 0; i < n; i++) scaled[i] = static_cast<int16_t>(samples[i] * v);
    SDL_PutAudioStreamData(stream_, scaled.data(), int(n * sizeof(int16_t)));  // non-blocking
    samples = samples.subspan(n);
  }
}

}  // namespace skylanders
