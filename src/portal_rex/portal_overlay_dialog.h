#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <rex/ui/imgui_dialog.h>

#include "portal/figure_catalog.h"
#include "portal/figure_stats.h"

struct ImGuiIO;

namespace rex::ui {
class ImGuiDrawer;
}  // namespace rex::ui

namespace skylanders {

// The in-game figure picker (F6). Browses .dump files under the portal_figures_dir cvar and
// places or removes a figure in a chosen slot (0-15) on the active software portal. Talks only to
// PlaceFigureFromFile/RemoveFigureFromSlot/GetSoftwarePortal (portal_rex/portal_rex.h) and
// ScanFigureCatalog (portal/figure_catalog.h); it never touches portal protocol bytes.
class PortalOverlayDialog : public rex::ui::ImGuiDialog {
 public:
  explicit PortalOverlayDialog(rex::ui::ImGuiDrawer* drawer);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void Rescan();
  // Cheap: stats a file's mtime and only re-decodes entries whose file actually changed since the
  // last check (e.g. the game just saved progress back to it) -- called every frame the Browse
  // tab is visible, unlike Rescan() itself (which re-lists the directory).
  void RefreshChangedFigureStats();
  // The result of the last action (place, create, dump), shown at the top until the next one.
  void SetStatus(std::string message, bool is_error);
  void DrawStatus() const;

  std::vector<portal::FigureCatalogEntry> entries_;
  std::vector<std::optional<portal::FigureStats>> entry_stats_;  // parallel to entries_
  std::vector<std::filesystem::file_time_type> entry_mtimes_;    // parallel to entries_
  std::string figures_dir_at_last_scan_;
  char filter_[128] = {};
  int selected_slot_ = 0;  // which slot Place/Remove act on
  bool creating_ = false;  // false: Browse tab: true: New Figure tab
  std::string status_;
  bool status_is_error_ = false;

  // A "Dump to file" running on its own thread, so the overlay doesn't freeze while the portal
  // reads 64 blocks. Shared with that thread, which may outlive the overlay.
  struct DumpJob {
    int slot = -1;
    std::mutex mu;
    bool done = false;
    bool ok = false;
    std::string message;
  };
  std::shared_ptr<DumpJob> dump_job_;
};

// A banner at the top of the screen while FigureSaveError() is set, F6 open or not. Draws nothing
// otherwise, and never takes input.
class SaveErrorBanner : public rex::ui::ImGuiDialog {
 public:
  explicit SaveErrorBanner(rex::ui::ImGuiDrawer* drawer) : rex::ui::ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override;
};

}  // namespace skylanders
