#ifndef AKTUALIZR_PARTIAL_TARGETS_H_
#define AKTUALIZR_PARTIAL_TARGETS_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "crypto.h"
#include "uptane_time.h"

typedef enum {
	TARGETS_OK_NOIMAGE,		/* Valid targets.json, no image for this ECU  */
	TARGETS_OK_NOUPDATE,		/* Valid targets.json, version is the same    */
	TARGETS_OK_UPDATE,		/* Valid targets.json, new image available    */
	TARGETS_NOMEM,			/* Dynamic memory error			      */
	TARGETS_ECUDUPLICATE,		/* ECU is mentioned twice		      */
	TARGETS_NOHASH,			/* No suitable hash found		      */
	TARGETS_SIGFAIL,		/* Signature verification failed	      */
	TARGETS_JSONERR,		/* Malformed JSON			      */
	TARGETS_WRONGTYPE,		/* _type field is not "Targets"		      */
	TARGETS_EXPIRED,		/* targets.json has expired		      */
	TARGETS_DOWNGRADE		/* New version is lower that the previous one */
} targets_result_t;

struct targets_ctx;
typedef struct targets_ctx targets_ctx_t;

/* Both are blocking functions returning false if the processing should
 * be interrupted. They are generally responsible for timeout logic, preventing
 * failures due to too large input etc. Processing procedure yields to these functions
 * from time to time, so it is also a right place to perform system tasks, unrelated
 * to JSON processing 
 *
 * TODO: isolate from targets, the interface should be the same for root.json and time.json
 * as well, including, probably, read_verify_wrapper (see targets.c)
 * */

typedef bool (*targets_read_t)(void* priv, uint8_t* buf, int len);
typedef bool (*targets_peek_t)(void* priv, uint8_t* buf);

targets_ctx_t* targets_ctx_new(void);
void targets_ctx_free(targets_ctx_t* ctx);
bool targets_init(targets_ctx_t* ctx, int version_prev, uptane_time_t time,
		  const uint8_t* ecu_id, const uint8_t* hardware_id,
		  const crypto_key_t* keys, int key_num, int threshold,
		  targets_read_t read_cb, targets_peek_t peek_cb, void* cb_priv);
targets_result_t targets_process(targets_ctx_t* ctx);
bool targets_get_result(const targets_ctx_t* ctx, uint8_t* sha512_hash, int* length, int* version);
#endif /* AKTUALIZR_PARTIAL_TARGETS_H_ */

