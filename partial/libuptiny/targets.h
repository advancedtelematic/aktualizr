#ifndef LIBUPTINY_TARGETS_H
#define LIBUPTINY_TARGETS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "state_api.h"

typedef enum {
  RESULT_ERROR = -1,
  RESULT_IN_PROGRESS = 0,
  RESULT_BEGIN_SIGNED,
  RESULT_END_SIGNED,
  RESULT_BEGIN_AND_END_SIGNED,
  RESULT_FOUND,
  RESULT_NOT_FOUND,
  RESULT_WRONG_HW_ID,
} targets_result_t;

void uptane_parse_targets_init(void);
int uptane_parse_targets_feed(const char *message, size_t len, uptane_targets_t *out_targets, targets_result_t* result);
int uptane_targets_get_num_signatures();
int uptane_targets_get_begin_signed();
int uptane_targets_get_end_signed();

#ifdef __cplusplus
}
#endif
#endif // LIBUPTINY_TARGETS_H
