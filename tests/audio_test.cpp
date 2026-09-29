#include <chrono>
#include <thread>

#include "portal/audio.h"
#include "test_util.h"

using namespace skylanders::portal;

int main() {
  // Sample conversion: little-endian signed, first sample 0x1234, last 0x8000.
  AudioPacket p{};
  p[0] = 0x34;
  p[1] = 0x12;
  p[62] = 0x00;
  p[63] = 0x80;
  auto s = SamplesFromAudioPacket(p, AudioSampleFormat::kSigned16LE);
  CHECK(s[0] == 0x1234);
  CHECK(s[31] == -32768);
  // Unsigned: 0x8000 is silence (0), 0x0000 is the most negative value.
  auto u = SamplesFromAudioPacket(p, AudioSampleFormat::kUnsigned16LE);
  CHECK(u[31] == 0);
  CHECK(u[1] == -32768);
  // Big-endian signed.
  AudioPacket b{};
  b[0] = 0x12;
  b[1] = 0x34;
  CHECK(SamplesFromAudioPacket(b, AudioSampleFormat::kSigned16BE)[0] == 0x1234);

  // Queue: bounded, drops the oldest, never blocks the producer.
  AudioPacketQueue q(2);
  AudioPacket a1{}, a2{}, a3{};
  a1[0] = 1;
  a2[0] = 2;
  a3[0] = 3;
  q.Push(a1);
  q.Push(a2);
  q.Push(a3);
  CHECK(q.Dropped() == 1);
  CHECK(q.PopWait(std::chrono::milliseconds(0))->at(0) == 2);
  CHECK(q.PopWait(std::chrono::milliseconds(0))->at(0) == 3);
  CHECK(!q.PopWait(std::chrono::milliseconds(0)).has_value());
  // Close wakes a waiting consumer.
  std::thread t([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.Close();
  });
  CHECK(!q.PopWait(std::chrono::seconds(5)).has_value());
  t.join();
  return Finish("audio");
}
