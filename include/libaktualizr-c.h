#ifndef AKTUALIZR_LIBAKTUALIZRC_H
#define AKTUALIZR_LIBAKTUALIZRC_H

#ifdef __cplusplus
#include "primary/aktualizr.h"

using Campaign = campaign::Campaign;

extern "C" {
#else
typedef struct Aktualizr Aktualizr;
typedef struct Campaign Campaign;
#endif

Aktualizr *Aktualizr_create(const char *config_path);
int Aktualizr_initialize(Aktualizr *a);
int Aktualizr_uptane_cycle(Aktualizr *a);
void Aktualizr_destroy(Aktualizr *a);

Campaign *Aktualizr_campaign_check(Aktualizr *a);
int Aktualizr_campaign_accept(Aktualizr *a, Campaign *c);
int Aktualizr_campaign_postpone(Aktualizr *a, Campaign *c);
int Aktualizr_campaign_decline(Aktualizr *a, Campaign *c);
void Aktualizr_campaign_free(Campaign *c);

#ifdef __cplusplus
}
#endif

#endif //AKTUALIZR_LIBAKTUALIZR_H
