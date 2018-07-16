#ifndef LIBUPTINY_TARGETS_H
#define LIBUPTINY_TARGETS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "state_api.h"

typedef enum {
  RESULT_ERROR = 0xFFFF,
  RESULT_WRONG_HW_ID = 0xFFFE,
  RESULT_IN_PROGRESS = 0x0000,
  RESULT_BEGIN_SIGNED = 0x0001,
  RESULT_END_SIGNED = 0x0002,
  RESULT_END_FOUND = 0x0004,
  RESULT_END_NOT_FOUND = 0x0008,
} targets_result_t;

void uptane_parse_targets_init(void);
int uptane_parse_targets_feed(const char *message, size_t len, uptane_targets_t *out_targets, uint16_t *result);
int uptane_targets_get_num_signatures();
int uptane_targets_get_begin_signed();
int uptane_targets_get_end_signed();

#ifdef __cplusplus
}
#endif
#endif  // LIBUPTINY_TARGETS_H
