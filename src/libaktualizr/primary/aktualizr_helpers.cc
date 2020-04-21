#include <set>

#include "aktualizr_helpers.h"

void targets_autoclean_cb(Aktualizr &aktualizr, const std::shared_ptr<event::BaseEvent> &event) {
  if (!event->isTypeOf<event::AllInstallsComplete>()) {
    return;
  }

  std::vector<Uptane::Target> installed_targets = aktualizr.GetStoredTargets();
  std::vector<bool> to_remove(installed_targets.size(), true);

  Aktualizr::InstallationLog log = aktualizr.GetInstallationLog();

  // keep the last two installed targets for each ECU
  for (const Aktualizr::InstallationLogEntry &entry : log) {
    auto start = entry.installs.size() >= 2 ? entry.installs.end() - 2 : entry.installs.begin();
    for (auto it = start; it != entry.installs.end(); it++) {
      auto fit = std::find_if(installed_targets.begin(), installed_targets.end(),
                              [&it](const Uptane::Target &t2) { return it->sha256Hash() == t2.sha256Hash(); });

      if (fit == installed_targets.end()) {
        continue;
      }

      size_t rem_idx = static_cast<size_t>(fit - installed_targets.begin());
      to_remove[rem_idx] = false;
    }
  }

  for (size_t k = 0; k < installed_targets.size(); k++) {
    if (to_remove[k]) {
      aktualizr.DeleteStoredTarget(installed_targets[k]);
    }
  }
}
