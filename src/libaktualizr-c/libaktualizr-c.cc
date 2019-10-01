#include "libaktualizr-c.h"

Aktualizr *Aktualizr_create_from_cfg(Config *cfg) {
  Aktualizr *a;
  try {
    a = new Aktualizr(*cfg);
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_create exception: " << e.what() << std::endl;
    return nullptr;
  }
  return a;
}

Aktualizr *Aktualizr_create_from_path(const char *config_path) {
  try {
    Config cfg(config_path);
    return Aktualizr_create_from_cfg(&cfg);
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_create exception: " << e.what() << std::endl;
    return nullptr;
  }
}

int Aktualizr_initialize(Aktualizr *a) {
  try {
    a->Initialize();
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_initialize exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

int Aktualizr_uptane_cycle(Aktualizr *a) {
  try {
    a->UptaneCycle();
  } catch (const std::exception &e) {
    std::cerr << "Uptane cycle exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

void Aktualizr_destroy(Aktualizr *a) { delete a; }

Campaign *Aktualizr_campaigns_check(Aktualizr *a) {
  try {
    auto r = a->CampaignCheck().get();
    if (!r.campaigns.empty()) {
      // We don't support multiple campaigns at the moment
      return new Campaign(r.campaigns[0]);
    }
  } catch (const std::exception &e) {
    std::cerr << "Campaign check exception: " << e.what() << std::endl;
    return nullptr;
  }
  return nullptr;
}

int Aktualizr_campaign_accept(Aktualizr *a, Campaign *c) {
  try {
    a->CampaignControl(c->id, campaign::Cmd::Accept).get();
  } catch (const std::exception &e) {
    std::cerr << "Campaign accept exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

int Aktualizr_campaign_postpone(Aktualizr *a, Campaign *c) {
  try {
    a->CampaignControl(c->id, campaign::Cmd::Postpone).get();
  } catch (const std::exception &e) {
    std::cerr << "Campaign postpone exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

int Aktualizr_campaign_decline(Aktualizr *a, Campaign *c) {
  try {
    a->CampaignControl(c->id, campaign::Cmd::Decline).get();
  } catch (const std::exception &e) {
    std::cerr << "Campaign decline exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

void Aktualizr_campaign_free(Campaign *c) { delete c; }

Updates *Aktualizr_updates_check(Aktualizr *a) {
  try {
    auto r = a->CheckUpdates().get();
    return (r.updates.size() > 0) ? new Updates(std::move(r.updates)) : nullptr;
  } catch (const std::exception &e) {
    std::cerr << "Campaign decline exception: " << e.what() << std::endl;
    return nullptr;
  }
}

void Aktualizr_updates_free(Updates *u) { delete u; }

size_t Aktualizr_get_targets_num(Updates *u) { return (u == nullptr) ? 0 : u->size(); }

Target *Aktualizr_get_nth_target(Updates *u, size_t n) {
  try {
    if (u != nullptr) {
      return &u->at(n);
    } else {
      return nullptr;
    }
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return nullptr;
  }
}

// TODO: Would it be nicer if t->filename returned const ref?
char *Aktualizr_get_target_name(Target *t) {
  if (t != nullptr) {
    void *name_ptr = malloc(sizeof(char) * (t->filename().size() + 1));
    auto *name = static_cast<char *>(name_ptr);
    strncpy(name, t->filename().c_str(), t->filename().size() + 1);
    return name;
  } else {
    return nullptr;
  }
}

int Aktualizr_download_target(Aktualizr *a, Target *t) {
  try {
    a->Download(std::vector<Uptane::Target>({*t})).get();
  } catch (const std::exception &e) {
    std::cerr << "Campaign decline exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

int Aktualizr_install_target(Aktualizr *a, Target *t) {
  try {
    a->Install(std::vector<Uptane::Target>({*t})).get();
  } catch (const std::exception &e) {
    std::cerr << "Campaign decline exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

int Aktualizr_send_manifest(Aktualizr *a, const char *manifest) {
  try {
    Json::Value custom = Utils::parseJSON(manifest);
    bool r = a->SendManifest(custom).get();
    return r ? 0 : -1;
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_send_manifest exception: " << e.what() << std::endl;
    return -1;
  }
}

int Aktualizr_send_device_data(Aktualizr *a) {
  try {
    a->SendDeviceData();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_send_device_data exception: " << e.what() << std::endl;
    return -1;
  }
}

StorageTargetHandle *Aktualizr_open_stored_target(Aktualizr *a, const Target *t) {
  if (t == nullptr) {
    std::cerr << "Aktualizr_open_stored_target failed: invalid input" << std::endl;
    return nullptr;
  }

  try {
    auto handle = a->OpenStoredTarget(*t);
    return handle.release();
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_open_stored_target exception: " << e.what() << std::endl;
    return nullptr;
  }
}

size_t Aktualizr_read_stored_target(StorageTargetHandle *handle, uint8_t *buf, size_t size) {
  if (handle != nullptr && buf != nullptr) {
    return handle->rread(buf, size);
  } else {
    std::cerr << "Aktualizr_read_stored_target failed: invalid input " << (handle == nullptr ? "handle" : "buffer")
              << std::endl;
    return 0;
  }
}

int Aktualizr_close_stored_target(StorageTargetHandle *handle) {
  if (handle != nullptr) {
    handle->rclose();
    delete handle;
    return 0;
  } else {
    std::cerr << "Aktualizr_close_stored_target failed: no input handle" << std::endl;
    return -1;
  }
}
