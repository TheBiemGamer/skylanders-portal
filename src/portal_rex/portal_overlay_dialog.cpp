#include "portal_rex/portal_overlay_dialog.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <thread>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_drawer.h>

#include "portal_rex/portal_rex.h"
#include "portal/figure_catalog.h"
#include "portal/figure_file.h"
#include "portal/figure_stats.h"
#include "portal/portal_mode.h"
#include "portal/software/software_portal.h"
#include "portal/trap_villain.h"
#include "portal/usb/usb_portal.h"
#include "portal/text.h"
#include "portal/utf8_path.h"

namespace skylanders {

namespace {

// "Slot N: <figure name>" or "Slot N: empty".
std::string SlotLabel(portal::SoftwarePortal* software, int slot) {
  std::string label = "Slot " + std::to_string(slot) + ": ";
  const auto figure = software->Figure(slot);
  if (!figure) return label + "empty";
  // A trap shows the villain inside it, which the file name may not say.
  if (std::string name = portal::FigureDisplayName(*figure); !name.empty()) return label + name;
  if (auto source = software->Source(slot)) return label + Utf8Path(source->stem());
  return label + "(unnamed)";
}

}  // namespace

PortalOverlayDialog::PortalOverlayDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {
  Rescan();
}

void PortalOverlayDialog::Rescan() {
  figures_dir_at_last_scan_ = REXCVAR_GET(portal_figures_dir);
  entries_ = figures_dir_at_last_scan_.empty()
                 ? std::vector<portal::FigureCatalogEntry>{}
                 : portal::ScanFigureCatalog(Utf8ToPath(figures_dir_at_last_scan_));
  entry_stats_.clear();
  entry_stats_.reserve(entries_.size());
  entry_mtimes_.clear();
  entry_mtimes_.reserve(entries_.size());
  for (const auto& entry : entries_) {
    std::optional<portal::FigureStats> stats;
    if (auto data = portal::LoadFigureFile(entry.path)) {
      stats = portal::ParseFigureStats(*data);
    }
    entry_stats_.push_back(stats);
    std::error_code ec;
    entry_mtimes_.push_back(std::filesystem::last_write_time(entry.path, ec));
  }
}

void PortalOverlayDialog::RefreshChangedFigureStats() {
  for (size_t i = 0; i < entries_.size(); ++i) {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(entries_[i].path, ec);
    if (ec || mtime == entry_mtimes_[i]) continue;
    entry_mtimes_[i] = mtime;
    entry_stats_[i] = std::nullopt;
    if (auto data = portal::LoadFigureFile(entries_[i].path)) {
      entry_stats_[i] = portal::ParseFigureStats(*data);
    }
  }
}

void SaveErrorBanner::OnDraw(ImGuiIO& io) {
  const std::string error = FigureSaveError();
  if (error.empty()) return;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 16.0f), ImGuiCond_Always,
                          ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("##figure_save_error", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
  ImGui::PushTextWrapPos(io.DisplaySize.x * 0.6f);
  ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.5f, 1.0f), "%s", error.c_str());
  ImGui::PopTextWrapPos();
  ImGui::End();
}

void PortalOverlayDialog::SetStatus(std::string message, bool is_error) {
  status_ = std::move(message);
  status_is_error_ = is_error;
}

void PortalOverlayDialog::DrawStatus() const {
  if (status_.empty()) return;
  if (status_is_error_) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.5f, 1.0f));
  ImGui::TextWrapped("%s", status_.c_str());
  if (status_is_error_) ImGui::PopStyleColor();
  ImGui::Separator();
}

void PortalOverlayDialog::OnDraw(ImGuiIO& io) {
  (void)io;
  ImGui::SetNextWindowSize(ImVec2(480, 520), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Portal of Power", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  if (dump_job_) {
    std::lock_guard<std::mutex> lock(dump_job_->mu);
    if (dump_job_->done) {
      SetStatus(dump_job_->message, !dump_job_->ok);
      dump_job_.reset();
    }
  }
  if (dump_job_) {
    ImGui::TextWrapped("Dumping the figure in slot %d...", dump_job_->slot);
    ImGui::Separator();
  } else {
    DrawStatus();
  }

  if (portal::UsbPortal* usb = GetUsbPortal()) {
    ImGui::TextWrapped("Real USB portal connected.");
    if (usb->HadError()) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                         "A read or write error occurred -- see the log for details.");
    }
    ImGui::Separator();
    const std::vector<int> present_slots = usb->PresentSlots();
    if (present_slots.empty()) {
      ImGui::TextWrapped("No figure detected on the portal.");
    } else {
      // A real portal can hold more than one figure at once (2-player co-op, items), so every
      // occupied slot is listed, not just the first.
      for (int slot : present_slots) {
        if (auto id_variant = usb->DetectedIdVariant(slot)) {
          const auto* sky = portal::FindSkylander(id_variant->first, id_variant->second);
          if (sky) {
            ImGui::Text("Slot %d: %s", slot, std::string(sky->name).c_str());
          } else {
            ImGui::Text("Slot %d: unrecognized figure (id %u, variant %u)", slot,
                        static_cast<unsigned>(id_variant->first),
                        static_cast<unsigned>(id_variant->second));
          }
        } else {
          ImGui::Text("Slot %d: figure detected, identity not read yet", slot);
        }
        std::optional<portal::FigureStats> stats;
        if (auto blocks = ReadRealFigureBlocks(slot)) stats = portal::ParseFigureStats(*blocks);
        if (stats) {
          if (stats->nickname.empty()) {
            ImGui::Text("  Level %u, %u gold", static_cast<unsigned>(stats->level),
                        static_cast<unsigned>(stats->gold));
          } else {
            ImGui::Text("  Level %u, %u gold (\"%s\")", static_cast<unsigned>(stats->level),
                        static_cast<unsigned>(stats->gold), stats->nickname.c_str());
          }
          if (stats->next_xp > stats->xp) {
            ImGui::TextDisabled("  %u XP to level %u", static_cast<unsigned>(stats->next_xp - stats->xp),
                                static_cast<unsigned>(stats->level) + 1);
          }
        } else {
          ImGui::TextDisabled("  (level/gold/nickname not read yet)");
        }
        ImGui::PushID(slot);
        ImGui::BeginDisabled(dump_job_ != nullptr);
        if (ImGui::Button("Dump to file")) {
          dump_job_ = std::make_shared<DumpJob>();
          dump_job_->slot = slot;
          std::thread([job = dump_job_, slot] {
            std::filesystem::path saved;
            std::string err;
            const bool ok = DumpRealFigureToFile(slot, &saved, &err);
            std::lock_guard<std::mutex> lock(job->mu);
            job->ok = ok;
            job->message = ok ? "Saved: " + Utf8Path(saved) : "Dump failed: " + err;
            job->done = true;
          }).detach();
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
          ImGui::SetTooltip(
              "Reads this figure's full data and saves it as a .dump file under your figures "
              "folder. The game pauses its portal for about half a second while this runs. "
              "Nothing changes on the physical toy.");
        }
        ImGui::PopID();
      }
    }
    ImGui::End();
    return;
  }
  if (portal::ParsePortalMode(REXCVAR_GET(portal_mode)) == portal::PortalMode::kUsb) {
    // Only a portal really there is "active" (an Xbox 360 one on WinUSB has no figure list here,
    // GetUsbPortal() is the HID one); without one it isn't found and the game keeps looking.
    ImGui::TextWrapped("%s", RealPortalConnected()
                                 ? "The real USB portal is active. Put your figures on it."
                                 : "No real portal found. Plug it in; the game keeps looking for it.");
    ImGui::End();
    return;
  }

  portal::SoftwarePortal* software = GetSoftwarePortal();
  if (!software) {
    ImGui::TextWrapped(
        "The virtual portal is off. Set Portal mode to 'software' in the settings (F4) to use "
        "the figure picker.");
    ImGui::End();
    return;
  }

  if (ImGui::Button(creating_ ? "Browse" : "New Figure")) creating_ = !creating_;
  ImGui::SameLine();

  // Slot selector: also shows every slot's current figure by name.
  if (ImGui::BeginCombo("Slot", SlotLabel(software, selected_slot_).c_str())) {
    for (int i = 0; i < portal::kMaxFigures; ++i) {
      const bool selected = (i == selected_slot_);
      if (ImGui::Selectable(SlotLabel(software, i).c_str(), selected)) selected_slot_ = i;
      if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  const bool has_figure = software->Figure(selected_slot_).has_value();
  ImGui::BeginDisabled(!has_figure);
  if (ImGui::Button("Remove")) {
    RemoveFigureFromSlot(selected_slot_);
    SetStatus({}, false);
  }
  ImGui::EndDisabled();

  // Backups of the selected slot's file (portal_rex.cpp makes one before the game first saves to
  // it). Restore swaps the backup onto the portal first, like a real portal swap: the slot takes no
  // writes until the game has seen it, so the game can't save the old data over the restored file.
  // The file as it was becomes the newest backup, so a restore can be undone.
  if (const auto source = software->Source(selected_slot_)) {
    const std::string name = Utf8Path(source->filename());
    const std::filesystem::path backup_dir =
        portal::FigureBackupDir(*source, Utf8ToPath(REXCVAR_GET(portal_figures_dir)));
    if (ImGui::TreeNode("backups", "Backups of %s", name.c_str())) {
      const std::vector<std::filesystem::path> backups = portal::ListFigureBackups(backup_dir);
      if (backups.empty()) ImGui::TextDisabled("None yet: one is made before the game first saves.");
      for (const auto& backup : backups) {
        ImGui::PushID(Utf8Path(backup).c_str());
        ImGui::TextUnformatted(Utf8Path(backup.stem()).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 80);
        if (ImGui::Button("Restore")) {
          const auto data = portal::LoadFigureFile(backup);
          if (!data) {
            SetStatus("That backup of " + name + " is not a figure file.", true);
          } else if (!software->PlaceFigure(selected_slot_, *data, *source)) {
            SetStatus("Could not put that backup of " + name + " on the portal.", true);
          } else if (!portal::RestoreFigureBackup(backup, *source, backup_dir)) {
            PlaceFigureFromFile(selected_slot_, *source);  // back to the file, which is unchanged
            SetStatus("Could not restore " + name + ". It is unchanged.", true);
          } else {
            SetStatus("Restored " + name + " from " + Utf8Path(backup.stem()) +
                          ". What it held is now its newest backup.",
                      false);
          }
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
  }

  ImGui::Separator();

  if (creating_) {
    ImGui::InputTextWithHint("Filter", "Skylander name", filter_, sizeof(filter_));
    const std::string filter = Lower(filter_);
    auto matches = [&filter](std::string_view name) {
      return filter.empty() || Lower(std::string(name)).find(filter) != std::string::npos;
    };
    const auto villains = portal::AllTrapVillains();
    const bool any_villain = std::any_of(villains.begin(), villains.end(),
                                         [&](const auto& v) { return matches(v.name); });

    // Traps with a villain already inside, under Trap Team. A villain trap has the same figure
    // id/variant as the empty trap (the villain is in its save data), so they have their own list.
    auto draw_villains = [&] {
      if (!any_villain) return;
      if (!filter.empty()) ImGui::SetNextItemOpen(true);
      if (!ImGui::TreeNode("Villains")) return;
      for (const auto& villain : villains) {
        if (!matches(villain.name)) continue;
        ImGui::PushID(1000000 + villain.id * 2 + (villain.variant ? 1 : 0));
        ImGui::TextUnformatted(std::string(villain.name).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 80);
        if (ImGui::Button("Create")) {
          std::string err;
          if (!CreateAndPlaceVillainTrap(selected_slot_, villain, &err)) {
            REXLOG_WARN("Portal overlay: could not create a trap holding '{}': {}", villain.name,
                        err);
            SetStatus(err, true);
          } else {
            SetStatus("Created " + std::string(villain.name) + " in slot " +
                          std::to_string(selected_slot_) + ".",
                      false);
            Rescan();
            creating_ = false;
          }
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    };

    constexpr std::string_view kTrapTeam = "Trap Team";
    constexpr std::string_view kTraps = "Traps";
    ImGui::BeginChild("create_list", ImVec2(0, 0), true);
    // Game headers hold one tree node per category; all collapsed by default, and a filter opens
    // every one with a match. Villains follow Traps inside Trap Team.
    std::string_view game, category;
    bool game_open = false, category_open = false, villains_drawn = false;
    auto close_category = [&] {
      if (category_open) ImGui::TreePop();
      category_open = false;
      if (game_open && game == kTrapTeam && category == kTraps && !villains_drawn) {
        draw_villains();
        villains_drawn = true;
      }
    };
    auto close_game = [&] {
      close_category();
      if (game_open && game == kTrapTeam && !villains_drawn) draw_villains();
      if (game == kTrapTeam) villains_drawn = true;
      if (game_open) ImGui::PopID();
      game_open = false;
    };
    for (const auto& sky : portal::AllSkylanders()) {
      if (!matches(sky.name)) continue;
      if (sky.game != game) {
        close_game();
        if (!filter.empty()) ImGui::SetNextItemOpen(true);
        game_open = ImGui::CollapsingHeader(sky.game.data());
        game = sky.game;
        // Category names repeat across games ("Variants"), so each game gets its own ID scope.
        if (game_open) ImGui::PushID(sky.game.data());
        category = {};
      }
      if (!game_open) continue;
      if (sky.category != category) {
        close_category();
        if (!filter.empty()) ImGui::SetNextItemOpen(true);
        category_open = ImGui::TreeNode(sky.category.data());
        category = sky.category;
      }
      if (!category_open) continue;
      ImGui::PushID(static_cast<int>(sky.id) * 100000 + sky.variant);
      ImGui::TextUnformatted(std::string(sky.name).c_str());
      ImGui::SameLine(ImGui::GetWindowWidth() - 80);
      if (ImGui::Button("Create")) {
        std::string err;
        if (!CreateAndPlaceFigure(selected_slot_, sky, &err)) {
          REXLOG_WARN("Portal overlay: could not create '{}': {}", sky.name, err);
          SetStatus(err, true);
        } else {
          SetStatus("Created " + std::string(sky.name) + " in slot " +
                        std::to_string(selected_slot_) + ".",
                    false);
          Rescan();
          creating_ = false;
        }
      }
      ImGui::PopID();
    }
    close_game();
    if (!villains_drawn && any_villain) {
      // The filter matched only villains: show them under their own Trap Team header.
      if (!filter.empty()) ImGui::SetNextItemOpen(true);
      if (ImGui::CollapsingHeader(kTrapTeam.data())) draw_villains();
    }
    ImGui::EndChild();
    ImGui::End();
    return;
  }

  const std::string current_dir = REXCVAR_GET(portal_figures_dir);
  if (current_dir.empty()) {
    ImGui::TextWrapped(
        "No figures folder is set. Start the game with --portal_figures_dir \"<folder>\" to "
        "browse your .dump files here.");
    ImGui::End();
    return;
  }
  if (current_dir != figures_dir_at_last_scan_) Rescan();  // the cvar can change via the console
  RefreshChangedFigureStats();  // cheap mtime check, catches e.g. the game saving progress back

  ImGui::InputTextWithHint("Filter", "figure name", filter_, sizeof(filter_));
  ImGui::SameLine();
  if (ImGui::Button("Rescan")) Rescan();

  if (entries_.empty()) {
    // Distinguish a typo'd or missing folder from a folder that is genuinely just empty, rather
    // than showing the same "no .dump files" message for both.
    std::error_code ec;
    const auto dir_path = Utf8ToPath(figures_dir_at_last_scan_);
    if (!std::filesystem::exists(dir_path, ec) || ec) {
      ImGui::TextWrapped("The folder '%s' does not exist.", figures_dir_at_last_scan_.c_str());
    } else if (!std::filesystem::is_directory(dir_path, ec) || ec) {
      ImGui::TextWrapped("'%s' is not a folder.", figures_dir_at_last_scan_.c_str());
    } else {
      ImGui::TextWrapped("No .dump files found under '%s'.", figures_dir_at_last_scan_.c_str());
    }
    ImGui::End();
    return;
  }

  // Which file each slot holds, so a file already on the portal shows its slot instead of Place.
  std::array<std::optional<std::filesystem::path>, portal::kMaxFigures> on_portal;
  for (int i = 0; i < portal::kMaxFigures; ++i) on_portal[i] = software->Source(i);
  auto slot_holding = [&on_portal](const std::filesystem::path& p) {
    for (int i = 0; i < portal::kMaxFigures; ++i) {
      if (on_portal[i] == p) return i;
    }
    return -1;
  };

  const std::string filter = Lower(filter_);
  ImGui::BeginChild("figure_list", ImVec2(0, 0), true);
  std::string last_game;
  bool section_open = false;
  for (size_t i = 0; i < entries_.size(); ++i) {
    const auto& entry = entries_[i];
    if (!filter.empty() && Lower(entry.name).find(filter) == std::string::npos) continue;
    if (entry.game != last_game) {
      // Collapsed by default; a filter opens every section with a match.
      if (!filter.empty()) ImGui::SetNextItemOpen(true);
      section_open = ImGui::CollapsingHeader(entry.game.empty() ? "(no game folder)" : entry.game.c_str());
      last_game = entry.game;
    }
    if (!section_open) continue;
    ImGui::PushID(Utf8Path(entry.path).c_str());  // path::string() throws on non-ANSI names
    ImGui::TextUnformatted(entry.display_name.c_str());
    if (entry_stats_[i]) {
      ImGui::SameLine();
      if (entry_stats_[i]->nickname.empty()) {
        ImGui::TextDisabled("(Lv %u, %u gold)", static_cast<unsigned>(entry_stats_[i]->level),
                            static_cast<unsigned>(entry_stats_[i]->gold));
      } else {
        ImGui::TextDisabled("(Lv %u, %u gold, \"%s\")", static_cast<unsigned>(entry_stats_[i]->level),
                            static_cast<unsigned>(entry_stats_[i]->gold),
                            entry_stats_[i]->nickname.c_str());
      }
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
    if (const int holder = slot_holding(entry.path); holder != -1) {
      ImGui::TextDisabled("Slot %d", holder);
    } else if (ImGui::Button("Place")) {
      std::string err;
      if (!PlaceFigureFromFile(selected_slot_, entry.path, &err)) {
        REXLOG_WARN("Portal overlay: could not place '{}': {}", Utf8Path(entry.path), err);
        SetStatus(err, true);
      } else {
        SetStatus({}, false);
      }
    }
    ImGui::PopID();
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace skylanders
