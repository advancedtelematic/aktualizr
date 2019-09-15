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

