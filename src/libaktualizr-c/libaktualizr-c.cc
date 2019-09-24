#include "libaktualizr-c.h"

Aktualizr *Aktualizr_create(const char *config_path) {
  Aktualizr *a;
  try {
    Config cfg(config_path);
    a = new Aktualizr(cfg);
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_create exception: " << e.what() << std::endl;
    return nullptr;
  }
  return a;
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

size_t Aktualizr_get_targets_num(Updates *u) { return u ? u->size() : 0; }

Target *Aktualizr_get_nth_target(Updates *u, size_t n) {
  try {
    if (u) {
      return &u->at(n);
    } else {
      return nullptr;
    }
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return nullptr;
  }
}

// TODO: leaks memory. Would it be nicer if t->filename returned const ref?
const char *Aktualizr_get_target_name(Target *t) {
  if (t) {
    auto name = new std::string(std::move(t->filename()));
    return name->c_str();
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

void *Aktualizr_open_stored_target(Aktualizr *a, const char *filename, const char *content) {
  if (filename == nullptr || content == nullptr) {
    std::cerr << "Aktualizr_open_stored_target failed: invalid input "
              << (filename ? "content" : "filename") << std::endl;
    return nullptr;
  }

  Json::Value value;
  Json::Reader reader;
  if (!reader.parse(content, value)) {
    std::cerr << "Aktualizr_open_stored_target content parsing failed" << std::endl;
    return nullptr;
  }

  Uptane::Target target(filename, value);
  try {
    auto handle = a->OpenStoredTarget(target);
    return reinterpret_cast<void *>(handle.release());
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_open_stored_target exception: " << e.what() << std::endl;
    return nullptr;
  }
}

size_t Aktualizr_read_stored_target(void *handle, uint8_t *buf, size_t size) {
  if (handle && buf) {
    StorageTargetRHandle *target_handle = reinterpret_cast<StorageTargetRHandle *>(handle);
    return target_handle->rread(buf, size);
  } else {
    std::cerr << "Aktualizr_read_stored_target failed: invalid input "
              << (handle ? "buffer" : "handle") << std::endl;
    return 0;
  }
}

int Aktualizr_close_stored_target(void *handle) {
  if (handle) {
    StorageTargetRHandle *target_handle = reinterpret_cast<StorageTargetRHandle *>(handle);
    target_handle->rclose();
    delete target_handle;
    return 0;
  } else {
    std::cerr << "Aktualizr_close_stored_target failed: no input handle" << std::endl;
    return -1;
  }
}
