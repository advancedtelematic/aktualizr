#ifndef AKTUALIZR_PARTIAL_READJSON_H_
#define AKTUALIZR_PARTIAL_READJSON_H_

#include <stdint.h>
#include "uptane_config.h"
#include "uptane_time.h"
#include "crypto.h"

/* Both are blocking functions. Read should return exactly as many bytes as requested.
 * If the processing is to be cancelled, it is recommended that both functions write
 * zeros to provided buffer without blocking, targets_process should then return after
 * no longer than it takes to process a valid JSON. Cancellation points might be added
 * in future.
 * */

typedef void (*readjson_read_t)(void* priv, uint8_t* buf, int len);
typedef void (*readjson_peek_t)(void* priv, uint8_t* buf);

/* Helper structure, part of readjson_ctx_t */
typedef struct {
	crypto_key_and_signature_t* sigs[CONFIG_UPTANE_MAX_SIGS];
	uint32_t num_keys;
	uint32_t threshold;

	bool in_signed;
	bool sig_valid[CONFIG_UPTANE_MAX_SIGS];
	crypto_verify_ctx_t* crypto_ctx[CONFIG_UPTANE_MAX_SIGS];
} signatures_ctx_t;

typedef struct {
	readjson_read_t read;
	readjson_peek_t peek;
	void* priv;
} read_cb_t;

typedef struct {
	read_cb_t callbacks;	
	signatures_ctx_t sig_ctx;
} readjson_ctx_t;


/* Key set for a role */
typedef struct {
	crypto_key_t* keys;
	int key_num;
	int threshold;
} role_keys_t;

bool readjson_init(readjson_ctx_t* ctx, const role_keys_t* keys, const read_cb_t* callbacks);

readjson_ctx_t* readjson_ctx_new(void);

void readjson_ctx_free(readjson_ctx_t* ctx);

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define one_char(ctx, buf) one_char_s(buf)
#define skip_bytes(ctx, buf) skip_bytes_s(buf)
#define hex_string(ctx, data, max_len) hex_string_s(data, max_len)
#define text_string(ctx, data, max_len) text_string_s(data, max_len)
#define integer_number(ctx, num) integer_number_s(num)
#define time_string(ctx, time) time_string_s(time)
#define read_signatures(ctx) read_signatures_s()
#define read_verify_signed(ctx) read_verify_signed_s()

void one_char_s(uint8_t* buf);
void skip_bytes_s(int n);
int hex_string_s(uint8_t* data, int max_len);
bool text_string_s(uint8_t* data, int max_len);
bool integer_number_s(uint32_t* num);
bool time_string_s(uptane_time_t* time);
bool read_signatures_s(void);
bool read_verify_signed_s(void);
#else
void one_char(readjson_ctx_t* ctx, uint8_t* buf);
void skip_bytes(readjson_ctx_t* ctx, int n);
int hex_string(readjson_ctx_t* ctx, uint8_t* data, int max_len);
bool text_string(readjson_ctx_t* ctx, uint8_t* data, int max_len);
bool integer_number(readjson_ctx_t* ctx, uint32_t* num);
bool time_string(readjson_ctx_t *ctx, uptane_time_t* time);
bool read_signatures(readjson_ctx_t* ctx);
bool read_verify_signed(readjson_ctx_t* ctx);
#endif

#define ignore_string(ctx) text_string(ctx, 0, INT_MAX)

/* Don't check the parts of JSON that don't change, like field names. If JSON
 * itself is corrupted it won't pass signature checks anyway */
#define fixed_data(ctx, str) skip_bytes(ctx, sizeof(str))

static inline void read_enter_signed(readjson_ctx_t * ctx) {
	ctx->sig_ctx.in_signed = true;
}

static inline void read_exit_signed(readjson_ctx_t * ctx) {
	ctx->sig_ctx.in_signed = false;
}

#endif /*AKTUALIZR_PARTIAL_READJSON_H_*/
