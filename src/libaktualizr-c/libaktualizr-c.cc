#include "libaktualizr-c.h"
#include "libaktualizr/events.h"

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

static void handler_wrapper(const std::shared_ptr<event::BaseEvent> &event, void (*handler)(const char *)) {
  if (handler == nullptr) {
    std::cerr << "handler_wrapper error: no external handler" << std::endl;
    return;
  }

  (*handler)(event->variant.c_str());
}

int Aktualizr_set_signal_handler(Aktualizr *a, void (*handler)(const char *event_name)) {
  try {
    auto functor = std::bind(handler_wrapper, std::placeholders::_1, handler);
    a->SetSignalHandler(functor);

  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_set_signal_handler exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

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
    return (!r.updates.empty()) ? new Updates(std::move(r.updates)) : nullptr;
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
const char *Aktualizr_get_target_name(Target *t) {
  if (t != nullptr) {
    auto length = t->filename().length();
    auto *name = new char[length + 1];
    strncpy(name, t->filename().c_str(), length + 1);
    return name;
  } else {
    return nullptr;
  }
}

void Aktualizr_free_target_name(const char *n) { delete[] n; }

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
    a->SendDeviceData().get();
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
    auto *stream = new auto(a->OpenStoredTarget(*t));
    return stream;
  } catch (const std::exception &e) {
    std::cerr << "Aktualizr_open_stored_target exception: " << e.what() << std::endl;
    return nullptr;
  }
}

size_t Aktualizr_read_stored_target(StorageTargetHandle *handle, uint8_t *buf, size_t size) {
  if (handle != nullptr && buf != nullptr) {
    handle->read(reinterpret_cast<char *>(buf), static_cast<std::streamsize>(size));
    return static_cast<size_t>(handle->gcount());
  } else {
    std::cerr << "Aktualizr_read_stored_target failed: invalid input " << (handle == nullptr ? "handle" : "buffer")
              << std::endl;
    return 0;
  }
}

int Aktualizr_close_stored_target(StorageTargetHandle *handle) {
  if (handle != nullptr) {
    handle->close();
    delete handle;
    return 0;
  } else {
    std::cerr << "Aktualizr_close_stored_target failed: no input handle" << std::endl;
    return -1;
  }
}

static Pause_Status_C get_Pause_Status_C(result::PauseStatus in) {
  switch (in) {
    case result::PauseStatus::kSuccess: {
      return Pause_Status_C::kSuccess;
    }
    case result::PauseStatus::kAlreadyPaused: {
      return Pause_Status_C::kAlreadyPaused;
    }
    case result::PauseStatus::kAlreadyRunning: {
      return Pause_Status_C::kAlreadyRunning;
    }
    case result::PauseStatus::kError: {
      return Pause_Status_C::kError;
    }
    default: {
      assert(false);
      return Pause_Status_C::kError;
    }
  }
}

Pause_Status_C Aktualizr_pause(Aktualizr *a) {
  result::Pause pause = a->Pause();
  return ::get_Pause_Status_C(pause.status);
}

Pause_Status_C Aktualizr_resume(Aktualizr *a) {
  result::Pause pause = a->Resume();
  return ::get_Pause_Status_C(pause.status);
}

void Aktualizr_abort(Aktualizr *a) { a->Abort(); }
