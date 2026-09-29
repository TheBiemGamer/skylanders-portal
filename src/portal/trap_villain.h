#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "portal/portal_device.h"

namespace skylanders::portal {

// A villain a Trap Team trap can hold. `trap_id` is the trap element's figure id (210 Magic,
// 211 Water, 212 Air, 213 Undead, 214 Tech, 215 Fire, 216 Earth, 217 Life, 218 Dark, 219 Light,
// 220 Kaos): a villain only fits traps of its own element. Variant villains ("Outlaw Brawl and
// Chain", ...) share the id of their base villain and set `variant`.
struct TrapVillain {
  uint8_t id;
  std::string_view name;
  uint16_t trap_id;
  bool variant;
};

// Every villain: 46 base villains (ids 1-46) and 6 variants, in id order.
std::span<const TrapVillain> AllTrapVillains();
const TrapVillain* FindTrapVillain(uint8_t id, bool variant);

// What a trap currently holds. villain_id 0 means empty.
struct TrapContents {
  uint8_t villain_id;
  bool variant;
  bool evolved;
};

// Reads the villain from a trap's save data (the newer of its two save areas that checks out).
// nullopt if `data` isn't a trap (figure id outside 210-220) or neither save area is valid; a
// never-written trap reads as empty.
std::optional<TrapContents> ReadTrapVillain(const FigureData& data);

// A new trap of the given shape (figure id + variant) holding `villain`, as a real trap looks
// after capturing it. nullopt if the villain doesn't belong to this trap's element.
std::optional<FigureData> CreateTrapWithVillain(uint16_t trap_id, uint16_t trap_variant,
                                                const TrapVillain& villain, bool evolved = false);

// The variant (shape) of this element's first regular crystal trap in the built-in catalog, used
// when creating a villain trap. 0 if the catalog has no trap for `trap_id`.
uint16_t DefaultTrapVariant(uint16_t trap_id);

}  // namespace skylanders::portal
