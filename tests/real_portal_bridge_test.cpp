// RealPortalBridge against a second SoftwarePortal standing in for the real portal: the bridge
// talks to it with the real portal's commands, which SoftwarePortal answers the same way.

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "portal/figure_file.h"
#include "portal/hybrid/real_portal_bridge.h"
#include "portal/software/software_portal.h"
#include "test_util.h"

using namespace skylanders::portal;

namespace {

Report Cmd(std::initializer_list<uint8_t> bytes) {
  Report r{};
  size_t i = 0;
  for (uint8_t b : bytes) r[i++] = b;
  return r;
}

// The emulated slot holding a figure, or -1.
int FirstFigure(const SoftwarePortal& p) {
  for (int slot = 0; slot < kMaxFigures; ++slot) {
    if (p.HasFigure(slot)) return slot;
  }
  return -1;
}

void Run(RealPortalBridge& bridge, PortalDevice* real, int steps = 400) {
  for (int i = 0; i < steps; ++i) bridge.Step(real);
}

// The game writing a block to the emulated portal ('W', slot, block, 16 bytes).
void GameWrite(SoftwarePortal& emulated, int slot, int block, uint8_t fill) {
  Report w = Cmd({'W', uint8_t(slot), uint8_t(block)});
  std::fill(w.begin() + 3, w.begin() + 3 + kBlockSize, fill);
  emulated.Write(w);
}

bool BlockIs(const FigureData& data, int block, uint8_t fill) {
  return std::all_of(data.begin() + block * kBlockSize, data.begin() + (block + 1) * kBlockSize,
                     [&](uint8_t b) { return b == fill; });
}

// A real portal that misses the first 'A' after it is plugged in: it then stays switched off.
class MissesFirstActivate : public PortalDevice {
 public:
  explicit MissesFirstActivate(SoftwarePortal* portal) : portal_(portal) {}
  void Write(const Report& report) override {
    if (report[0] == 'A' && !missed_) {
      missed_ = true;
      return;
    }
    portal_->Write(report);
  }
  Report Read() override { return portal_->Read(); }

 private:
  SoftwarePortal* portal_;
  bool missed_ = false;
};

// A real portal that sends no status while it answers a block read, and answers each one late:
// a whole figure takes well over a second, like on a real portal.
class SlowReader : public PortalDevice {
 public:
  explicit SlowReader(SoftwarePortal* portal) : portal_(portal) {}
  void Write(const Report& report) override {
    if (report[0] == 'R') ++resets;
    if (report[0] == 'Q') asked_ = last_read_ = std::chrono::steady_clock::now();
    portal_->Write(report);
  }
  Report Read() override {
    const auto now = std::chrono::steady_clock::now();
    if (asked_) {
      if (now - *asked_ < std::chrono::milliseconds(25)) return Report{};
      asked_.reset();
      Report r;
      do r = portal_->Read(); while (r[0] == 0x53);  // the block reply, not the status before it
      return r;
    }
    if (last_read_ && now - *last_read_ < std::chrono::milliseconds(100)) return Report{};
    return portal_->Read();
  }
  int resets = 0;

 private:
  SoftwarePortal* portal_;
  std::optional<std::chrono::steady_clock::time_point> asked_;
  std::optional<std::chrono::steady_clock::time_point> last_read_;
};

// A portal still on from an earlier run: its first frame is an old status saying a figure is there,
// sent before it has taken the bridge's 'R' and 'A', and nothing more arrives in that step.
class StillOnFromBefore : public PortalDevice {
 public:
  explicit StillOnFromBefore(SoftwarePortal* portal) : portal_(portal) {}
  void Write(const Report& report) override {
    if (report[0] == 'Q' && !answered_activate_) ++reads_before_activate;
    portal_->Write(report);
  }
  Report Read() override {
    if (reads_++ == 0) return Cmd({0x53, 0x01, 0, 0, 0, 0x40, 0x01});
    if (reads_ == 2) return Report{};
    const Report r = portal_->Read();
    if (r[0] == 0x41) answered_activate_ = true;
    return r;
  }
  int reads_before_activate = 0;

 private:
  SoftwarePortal* portal_;
  int reads_ = 0;
  bool answered_activate_ = false;
};

// A real portal that drops a light sent before it has answered 'A', and keeps the last one shown.
class LightPortal : public PortalDevice {
 public:
  explicit LightPortal(SoftwarePortal* portal) : portal_(portal) {}
  void Write(const Report& report) override {
    if (report[0] == 'R') shown.reset();
    if (report[0] == 'C') {
      if (answered_activate_) shown = report;
      return;
    }
    if (report[0] == 'J') sent.push_back(report);
    if (report[0] == 'R' || report[0] == 'A') answered_activate_ = false;
    portal_->Write(report);
  }
  Report Read() override {
    const Report r = portal_->Read();
    if (r[0] == 0x41) answered_activate_ = true;
    return r;
  }
  std::optional<Report> shown;
  std::vector<Report> sent;  // 'J'

 private:
  SoftwarePortal* portal_;
  bool answered_activate_ = false;
};

}  // namespace

int main() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "skylanders_bridge_test";
  std::filesystem::remove_all(dir);
  const FigureData chop = CreateBlankFigure(30, 0, {0x11, 0x22, 0x33, 0x44});

  // A figure on the real portal gets a copy in the emulated portal, from the top slot down, and
  // a backup file that is the copy's source.
  {
    SoftwarePortal real, emulated;
    std::vector<int> arrived;
    RealPortalBridge bridge(&emulated, {dir, nullptr, [&](int slot) { arrived.push_back(slot); }});
    real.PlaceFigure(0, chop);
    Run(bridge, &real);
    const int copy = FirstFigure(emulated);
    CHECK(copy == kMaxFigures - 1);
    CHECK(emulated.Figure(copy) == chop);
    CHECK(arrived.size() == 1 && arrived[0] == copy);
    CHECK(bridge.IsCopySlot(copy));
    const auto uids = bridge.RealUids();
    CHECK(uids.size() == 1 && uids[0] == RealPortalBridge::UidOf(chop));
    const auto source = emulated.Source(copy);
    CHECK(source && std::filesystem::exists(*source));
    CHECK(source && LoadFigureFile(*source) == chop);

    // What the game writes to the copy goes onto the real figure; the identity blocks and the
    // sector trailers never do.
    emulated.SetWriteCallback([&](int slot, const FigureData& data,
                                  const std::optional<std::filesystem::path>&) {
      bridge.OnEmulatedWrite(slot, data);
    });
    GameWrite(emulated, copy, 8, 0xAB);
    GameWrite(emulated, copy, 9, 0xCD);
    GameWrite(emulated, copy, 1, 0xEE);   // identity
    GameWrite(emulated, copy, 11, 0xEE);  // sector trailer
    Run(bridge, &real);
    const auto on_real = real.Figure(0);
    CHECK(on_real && BlockIs(*on_real, 8, 0xAB));
    CHECK(on_real && BlockIs(*on_real, 9, 0xCD));
    CHECK(on_real && !BlockIs(*on_real, 1, 0xEE));
    CHECK(on_real && !BlockIs(*on_real, 11, 0xEE));

    // The portal menu lifts the copy and puts it back into another slot: writes follow it.
    const FigureData copy_data = *emulated.Figure(copy);
    emulated.RemoveFigure(copy);
    Run(bridge, &real, 20);
    CHECK(!bridge.IsCopySlot(copy));
    emulated.PlaceFigure(3, copy_data, source);
    Run(bridge, &real, 20);
    CHECK(bridge.IsCopySlot(3));
    GameWrite(emulated, 3, 12, 0x5A);
    Run(bridge, &real);
    CHECK(real.Figure(0) && BlockIs(*real.Figure(0), 12, 0x5A));

    // Lifted off the real portal: the copy goes, other figures stay.
    const FigureData other = CreateBlankFigure(14, 0, {0x01, 0x02, 0x03, 0x04});
    emulated.PlaceFigure(0, other);  // put on through the menu
    real.RemoveFigure(0);
    Run(bridge, &real);
    CHECK(!emulated.HasFigure(3));
    CHECK(emulated.HasFigure(0));
    CHECK(bridge.RealUids().empty());
  }

  // Two figures on the real portal; unplugging it takes both copies off.
  {
    SoftwarePortal real, emulated;
    RealPortalBridge bridge(&emulated, {dir, nullptr, nullptr});
    real.PlaceFigure(0, chop);
    real.PlaceFigure(1, CreateBlankFigure(14, 0, {0x05, 0x06, 0x07, 0x08}));
    Run(bridge, &real);
    CHECK(emulated.HasFigure(kMaxFigures - 1) && emulated.HasFigure(kMaxFigures - 2));
    CHECK(bridge.RealUids().size() == 2);
    Run(bridge, nullptr, 1);
    CHECK(FirstFigure(emulated) == -1);
    CHECK(bridge.RealUids().empty());
    // Plugged in again: read again.
    Run(bridge, &real);
    CHECK(bridge.RealUids().size() == 2);
  }

  // The game's light command goes on to the real portal (SoftwarePortal ignores it; this only
  // checks nothing breaks and the figures stay).
  {
    SoftwarePortal real, emulated;
    RealPortalBridge bridge(&emulated, {dir, nullptr, nullptr});
    real.PlaceFigure(2, chop);
    bridge.OnLight(Cmd({'C', 0xFF, 0x00, 0x00}));
    Run(bridge, &real);
    CHECK(FirstFigure(emulated) == kMaxFigures - 1);
  }

  // A backup with other data (the figure played from the menu meanwhile) is kept, not overwritten.
  {
    SoftwarePortal real, emulated;
    RealPortalBridge bridge(&emulated, {dir, nullptr, nullptr});
    FigureData played = chop;
    played[8 * kBlockSize] ^= 0xFF;
    const std::filesystem::path backup = dir / "Chop Chop 44332211.dump";
    CHECK(SaveFigureFileAtomic(backup, played));
    real.PlaceFigure(0, chop);
    Run(bridge, &real);
    CHECK(LoadFigureFile(backup) == chop);
    CHECK(LoadFigureFile(dir / "Chop Chop 44332211 before.dump") == played);
  }

  // The backup file is already on the emulated portal (put on from the menu, whose list includes
  // the backup folder): SoftwarePortal refuses one file in two slots, so the figure is not
  // mirrored, the file in use is left alone, and nothing that slot's game writes reaches the toy.
  {
    std::filesystem::remove_all(dir);
    SoftwarePortal real, emulated;
    RealPortalBridge bridge(&emulated, {dir, nullptr, nullptr});
    emulated.SetWriteCallback([&](int slot, const FigureData& data,
                                  const std::optional<std::filesystem::path>&) {
      bridge.OnEmulatedWrite(slot, data);
    });
    FigureData played = chop;
    played[8 * kBlockSize] ^= 0xFF;
    std::filesystem::create_directories(dir);
    const std::filesystem::path backup = dir / "Chop Chop 44332211.dump";
    CHECK(SaveFigureFileAtomic(backup, played));
    CHECK(emulated.PlaceFigure(0, played, backup));
    real.PlaceFigure(0, chop);
    Run(bridge, &real);
    CHECK(FirstFigure(emulated) == 0);
    CHECK(!emulated.HasFigure(kMaxFigures - 1));
    CHECK(!bridge.IsCopySlot(0));
    CHECK(bridge.RealUids().empty());
    CHECK(LoadFigureFile(backup) == played);
    CHECK(!std::filesystem::exists(dir / "Chop Chop 44332211 before.dump"));
    emulated.Write(Cmd({'A', 0x01}));
    GameWrite(emulated, 0, 9, 0x77);
    Run(bridge, &real);
    CHECK(real.Figure(0) == chop);

    // Taken off the emulated portal, then the toy lifted and put back: mirrored as usual.
    emulated.RemoveFigure(0);
    real.RemoveFigure(0);
    Run(bridge, &real);
    real.PlaceFigure(0, chop);
    Run(bridge, &real);
    CHECK(emulated.HasFigure(kMaxFigures - 1));
    CHECK(bridge.RealUids().size() == 1);
  }

  // A portal that missed the first 'A' is activated again, and its figure still gets a copy.
  {
    SoftwarePortal real, emulated;
    MissesFirstActivate device(&real);
    RealPortalBridge bridge(&emulated, {dir / "again", nullptr, nullptr});
    real.PlaceFigure(0, chop);
    Run(bridge, &device);
    CHECK(FirstFigure(emulated) == -1);
    for (int i = 0; i < 300 && FirstFigure(emulated) == -1; ++i) {
      Run(bridge, &device, 10);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(FirstFigure(emulated) == kMaxFigures - 1);
  }

  // A portal busy reading a figure for over a second is not reset as if it were switched off.
  {
    SoftwarePortal real, emulated;
    SlowReader device(&real);
    RealPortalBridge bridge(&emulated, {dir / "slow", nullptr, nullptr});
    real.PlaceFigure(0, chop);
    for (int i = 0; i < 400 && FirstFigure(emulated) == -1; ++i) {
      Run(bridge, &device, 10);
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(FirstFigure(emulated) == kMaxFigures - 1);
    CHECK(device.resets == 1);
  }

  // An old status from before the bridge's 'A' starts no read (a real portal refuses it while it
  // resets, three refusals and the bridge gives up): the figure is read once the portal answered.
  {
    SoftwarePortal real, emulated;
    StillOnFromBefore device(&real);
    RealPortalBridge bridge(&emulated, {dir / "still_on", nullptr, nullptr});
    real.PlaceFigure(0, chop);
    Run(bridge, &device);
    CHECK(device.reads_before_activate == 0);
    CHECK(FirstFigure(emulated) == kMaxFigures - 1);
  }

  // The game's light, set before the portal is there, shows once it has answered 'A', and again on
  // a portal plugged back in (the game sets it only when it changes).
  {
    SoftwarePortal real, emulated;
    LightPortal device(&real);
    RealPortalBridge bridge(&emulated, {dir / "light", nullptr, nullptr});
    const Report red = Cmd({'C', 0xFF, 0x00, 0x00});
    bridge.OnLight(red);
    Run(bridge, nullptr, 1);
    Run(bridge, &device, 50);
    CHECK(device.shown == red);
    Run(bridge, nullptr, 1);
    LightPortal again(&real);
    for (int i = 0; i < 100 && !again.shown; ++i) {  // at most one light every 100 ms
      Run(bridge, &again, 5);
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(again.shown == red);
    // 'J' (a side's colour) goes on too, the newest per side.
    const Report left = Cmd({'J', 0x00, 0x00, 0xFF, 0x00});
    const Report right_old = Cmd({'J', 0x02, 0x10, 0x10, 0x10});
    const Report right = Cmd({'J', 0x02, 0x00, 0x00, 0xFF});
    bridge.OnLight(left);
    bridge.OnLight(right_old);
    bridge.OnLight(right);
    again.sent.clear();
    for (int i = 0; i < 100 && again.sent.size() < 2; ++i) {
      Run(bridge, &again, 5);
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(again.sent.size() == 2);
    CHECK(std::find(again.sent.begin(), again.sent.end(), left) != again.sent.end());
    CHECK(std::find(again.sent.begin(), again.sent.end(), right) != again.sent.end());
  }

  std::filesystem::remove_all(dir);
  return Finish("real portal bridge");
}
