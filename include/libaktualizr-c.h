#ifndef AKTUALIZR_LIBAKTUALIZRC_H
#define AKTUALIZR_LIBAKTUALIZRC_H

#ifdef __cplusplus
#include "primary/aktualizr.h"

using Campaign = campaign::Campaign;
using Updates = std::vector<Uptane::Target>;
using Target = Uptane::Target;

extern "C" {
#else
typedef struct Aktualizr Aktualizr;
typedef struct Campaign Campaign;
typedef struct Updates Updates;
typedef struct Target Target;
#endif

Aktualizr *Aktualizr_create(const char *config_path);
int Aktualizr_initialize(Aktualizr *a);
int Aktualizr_uptane_cycle(Aktualizr *a);
void Aktualizr_destroy(Aktualizr *a);

Campaign *Aktualizr_campaigns_check(Aktualizr *a);
int Aktualizr_campaign_accept(Aktualizr *a, Campaign *c);
int Aktualizr_campaign_postpone(Aktualizr *a, Campaign *c);
int Aktualizr_campaign_decline(Aktualizr *a, Campaign *c);
void Aktualizr_campaign_free(Campaign *c);

Updates *Aktualizr_updates_check(Aktualizr *a);
void Aktualizr_updates_free(Updates *u);

size_t Aktualizr_get_targets_num(Updates *u);
Target *Aktualizr_get_nth_target(Updates *u, size_t n);
const char *Aktualizr_get_target_name(Target *t);

int Aktualizr_download_target(Aktualizr *a, Target *t);

int Aktualizr_install_target(Aktualizr *a, Target *t);

#ifdef __cplusplus
}
#endif

#endif //AKTUALIZR_LIBAKTUALIZR_H
