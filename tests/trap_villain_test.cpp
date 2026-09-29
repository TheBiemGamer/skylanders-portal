#include <cstdlib>
#include <filesystem>
#include <set>

#include "portal/figure_file.h"
#include "portal/trap_villain.h"
#include "test_util.h"

using namespace skylanders::portal;

int main() {
  // The villain table: 46 villains (ids 1-46) plus 6 variant villains, each tied to a trap element.
  {
    int base = 0, variants = 0;
    std::set<int> ids;
    for (const TrapVillain& v : AllTrapVillains()) {
      CHECK(v.trap_id >= 210 && v.trap_id <= 220);
      CHECK(!v.name.empty());
      if (v.variant) {
        ++variants;
      } else {
        ++base;
        ids.insert(v.id);
      }
    }
    CHECK(base == 46);
    CHECK(variants == 6);
    CHECK(ids.size() == 46 && *ids.begin() == 1 && *ids.rbegin() == 46);
    const TrapVillain* juju = FindTrapVillain(37, false);
    CHECK(juju && juju->name == "Bad Juju" && juju->trap_id == 212);
    const TrapVillain* outlaw = FindTrapVillain(16, true);
    CHECK(outlaw && outlaw->name == "Outlaw Brawl and Chain" && outlaw->trap_id == 211);
    CHECK(FindTrapVillain(47, false) == nullptr);
  }

  // Create, then read back: the villain, its variant flag and the evolved flag survive.
  {
    const TrapVillain* juju = FindTrapVillain(37, false);
    auto data = CreateTrapWithVillain(212, 12305, *juju);
    REQUIRE_OR_RETURN(data.has_value());
    CHECK(ReadFigureId(*data) == 212);
    CHECK(ReadFigureVariant(*data) == 12305);
    auto contents = ReadTrapVillain(*data);
    REQUIRE_OR_RETURN(contents.has_value());
    CHECK(contents->villain_id == 37);
    CHECK(!contents->variant);
    CHECK(!contents->evolved);

    const TrapVillain* outlaw = FindTrapVillain(16, true);
    auto v = CreateTrapWithVillain(211, 12295, *outlaw, /*evolved=*/true);
    REQUIRE_OR_RETURN(v.has_value());
    auto vc = ReadTrapVillain(*v);
    REQUIRE_OR_RETURN(vc.has_value());
    CHECK(vc->villain_id == 16 && vc->variant && vc->evolved);
  }

  // A villain only fits a trap of its own element.
  CHECK(!CreateTrapWithVillain(211, 12295, *FindTrapVillain(37, false)).has_value());

  // A blank trap reads as empty; a non-trap figure isn't a trap at all.
  {
    auto empty = ReadTrapVillain(CreateBlankFigure(212, 12305));
    CHECK(empty.has_value() && empty->villain_id == 0);
    CHECK(!ReadTrapVillain(CreateBlankFigure(450, 12288)).has_value());
  }

  // The default shape for an element is one of that element's crystal traps.
  CHECK(DefaultTrapVariant(212) != 0);
  CHECK(DefaultTrapVariant(999) == 0);

  // Real dumps, when available (set SKYLANDERS_TEST_DUMPS to the "Dumps Clean" folder).
  if (const char* root = std::getenv("SKYLANDERS_TEST_DUMPS")) {
    const std::filesystem::path villains =
        std::filesystem::path(root) / "4. Trap Team" / "7) Traps" / "2) Trappable Villains" / "Air";
    auto juju = LoadFigureFile(villains / "Bad Juju.dump");
    auto dream = LoadFigureFile(villains / "Dreamcatcher (Doom Raider).dump");
    REQUIRE_OR_RETURN(juju && dream);
    auto jc = ReadTrapVillain(*juju);
    auto dc = ReadTrapVillain(*dream);
    CHECK(jc && jc->villain_id == 37);
    CHECK(dc && dc->villain_id == 8);  // uses its second save area
  } else {
    std::puts("(real-dump checks skipped: SKYLANDERS_TEST_DUMPS not set)");
  }

  return Finish("trap_villain");
}
