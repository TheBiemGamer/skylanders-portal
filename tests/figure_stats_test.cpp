#include "portal/figure_file.h"
#include "portal/figure_stats.h"
#include "test_util.h"

using namespace skylanders::portal;

int main() {
  // A figure with no save data at all (e.g. --portal_test_figure) has nothing to report.
  {
    FigureData d{};
    CHECK(!ParseFigureStats(d).has_value());
  }

  // A freshly created blank figure (real id/variant/CRC, but never played -- its save-data
  // region is still zero) also has no stats yet, not zeros presented as real data.
  {
    FigureData d = CreateBlankFigure(112, 0, {0x11, 0x22, 0x33, 0x44});
    CHECK(!ParseFigureStats(d).has_value());
  }

  return Finish("figure_stats");
}
