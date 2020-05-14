#ifndef AKTUALIZR_LIBAKTUALIZRC_H
#define AKTUALIZR_LIBAKTUALIZRC_H

#include <stdint.h> // for uint8_t

#ifdef __cplusplus
#include "primary/aktualizr.h"

using Campaign = campaign::Campaign;
using Updates = std::vector<Uptane::Target>;
using Target = Uptane::Target;
using StorageTargetHandle = std::ifstream;

extern "C" {
#else
typedef struct Aktualizr Aktualizr;
typedef struct Campaign Campaign;
typedef struct Updates Updates;
typedef struct Target Target;
typedef struct StorageTargetHandle StorageTargetHandle;
#endif

Aktualizr *Aktualizr_create_from_cfg(Config *cfg);
Aktualizr *Aktualizr_create_from_path(const char *config_path);
int Aktualizr_initialize(Aktualizr *a);
int Aktualizr_uptane_cycle(Aktualizr *a);
void Aktualizr_destroy(Aktualizr *a);

int Aktualizr_set_signal_handler(Aktualizr *a, void (*handler)(const char* event_name));

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
void Aktualizr_free_target_name(const char *n);

int Aktualizr_download_target(Aktualizr *a, Target *t);

int Aktualizr_install_target(Aktualizr *a, Target *t);

int Aktualizr_send_manifest(Aktualizr *a, const char *manifest);
int Aktualizr_send_device_data(Aktualizr *a);

StorageTargetHandle *Aktualizr_open_stored_target(Aktualizr *a, const Target *t);
size_t Aktualizr_read_stored_target(StorageTargetHandle *handle, uint8_t* buf, size_t size);
int Aktualizr_close_stored_target(StorageTargetHandle *handle);

typedef enum {
  kSuccess = 0,
  kAlreadyPaused,
  kAlreadyRunning,
  kError }
Pause_Status_C;

Pause_Status_C Aktualizr_pause(Aktualizr *a);
Pause_Status_C Aktualizr_resume(Aktualizr *a);
void Aktualizr_abort(Aktualizr *a);

#ifdef __cplusplus
}
#endif

#endif //AKTUALIZR_LIBAKTUALIZR_H
