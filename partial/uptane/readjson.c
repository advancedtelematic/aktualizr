#include <limits.h>
#include "readjson.h"

#ifdef CONFIG_UPTANE_NOMALLOC
#  ifdef CONFIG_UPTANE_SINGLETHREADED
static readjson_ctx_t readjson_ctx_s;
#  else
static readjson_ctx_t readjson_ctxs[CONFIG_UPTANE_READJSON_NUM_CONTEXTS];
static bool readjson_ctx_busy[CONFIG_UPTANE_READJSON_NUM_CONTEXTS] = {0, }; /*could be reduce to a bitmask when new/free performance is an issue*/
#  endif
#endif

readjson_ctx_t* readjson_ctx_new() {
#if defined(CONFIG_UPTANE_SINGLETHREADED)
	return &readjson_ctx_s;
#elif defined(CONFIG_UPTANE_NOMALLOC)
	int i;

	for(i = 0; i < CONFIG_UPTANE_READJSON_NUM_CONTEXTS; i++)
		if(!readjson_ctx_busy[i])
			return &readjson_ctxs[i];
	return NULL;
#else
	return malloc(sizeof(readjson_ctx_t));
#endif
}

void readjson_ctx_free(readjson_ctx_t* ctx) {
	int i;
#if defined(CONFIG_UPTANE_NOMALLOC) && !defined(CONFIG_UPTANE_SINGLETHREADED)
	uintptr_t j = ((uintptr_t) ctx - (uintptr_t) readjson_ctxs)/sizeof(readjson_ctx_t); 
#endif

	for(i = 0; i < ctx->sig_ctx.num_keys; i++) {
		crypto_sig_free(ctx->sig_ctx.sigs[i]);
		if(ctx->sig_ctx.sig_valid[i]) 
			crypto_verify_ctx_free(ctx->sig_ctx.crypto_ctx[i]);
	}

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#elif defined(CONFIG_UPTANE_NOMALLOC)
	readjson_ctx_busy[j] = 0;
#else
	free(ctx);
#endif
}

static bool signatures_init(signatures_ctx_t* ctx, const role_keys_t* keys) {
	int i;

	for (i = 0; i < keys->key_num; i++) {
		if(i >= CONFIG_UPTANE_MAX_SIGS)
			break;
		ctx->sigs[i] = crypto_sig_new();
		if(!ctx->sigs[i])
			return false;
		ctx->sigs[i]->key = &keys->keys[i];
		ctx->sig_valid[i] = false;
	}
	ctx->num_keys = i;
	ctx->in_signed = false;
	ctx->threshold = keys->threshold;

	return true;
}

bool readjson_init(readjson_ctx_t* ctx, const role_keys_t* keys, const read_cb_t* callbacks) {
	ctx->callbacks = *callbacks;

	return signatures_init(&ctx->sig_ctx, keys);
}

static void signatures_on_read(signatures_ctx_t* ctx, uint8_t* buf, int len) {
	if(ctx->in_signed) {
		int i;
		for(i = 0; i < ctx->num_keys; i++)
			if(ctx->sig_valid[i])
				crypto_verify_feed(ctx->crypto_ctx[i], buf, len);
	}
}


#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define read_verify_wrapper(ctx, buf, len) read_verify_wrapper_s(buf, len)
static void read_verify_wrapper_s(uint8_t* buf, int len) {
	readjson_ctx_t* ctx = &readjson_ctx_s;
#else
static void read_verify_wrapper(readjson_ctx_t* ctx, uint8_t* buf, int len) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	ctx->callbacks.read(ctx->callbacks.priv, buf, len);

	signatures_on_read(&ctx->sig_ctx, buf, len);
}

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define peek_wrapper(ctx, buf) peek_wrapper_s(buf)
static void peek_wrapper_s(uint8_t* buf) {
	readjson_ctx_t* ctx = &readjson_ctx_s;
#else
static void peek_wrapper(readjson_ctx_t* ctx, uint8_t* buf) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	ctx->callbacks.peek(ctx->callbacks.priv, buf);
}
static bool is_hex(uint8_t c) {
	return (c >= '0' && c <= '9') ||
	       (c >= 'A' && c <= 'F') ||
	       (c >= 'a' && c <= 'f');
}
static uint8_t from_hex(uint8_t hi, uint8_t lo) {
	uint8_t res;

	if(hi >= '0' && hi <= '9')
		res = hi - '0';
	else if (hi >= 'A' && hi <= 'F')
		res = hi - 'A' + 10;
	else /* inputs are validated outside */
		res = hi - 'a' + 10;

	res <<= 4;

	if(lo >= '0' && lo <= '9')
		res |= lo - '0';
	else if (lo >= 'A' && lo <= 'F')
		res |= lo - 'A' + 10;
	else /* inputs are validated outside */
		res |= lo - 'a' + 10;

	return res;
}
/* TODO: consider having more return codes to differentiate between read errors
   and malformed JSON*/
#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define one_char(ctx, buf) one_char_s(buf)
void one_char_s(uint8_t* buf) {
#else
void one_char(readjson_ctx_t* ctx, uint8_t* buf) {
#endif
	read_verify_wrapper(ctx, buf, 1);
}

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define skip_bytes(ctx, buf) skip_bytes_s(buf)
void skip_bytes_s(int n) {
#else
void skip_bytes(readjson_ctx_t* ctx, int n) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	uint8_t b;
	while(--n)
		one_char(ctx, &b);
}
#define fixed_data(ctx, str) skip_bytes(ctx, sizeof(str))

/* Hex string including quotes*/
#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define hex_string(ctx, data, max_len) hex_string_s(data, max_len)
int hex_string_s(uint8_t* data, int max_len) {
#else
int hex_string(readjson_ctx_t* ctx, uint8_t* data, int max_len) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	int i;
	uint8_t hex[2];

	one_char(ctx, &hex[0]);

	if(hex[0] != '\"')
		return -1;

	for(i = 0; i < (max_len << 1) + 1; i++) {
		int ind = i&1;
		one_char(ctx, &hex[ind]);

		if(ind) {
			if(!is_hex(hex[1]))
				return -1;
			data[i>>1] = from_hex(hex[0], hex[1]);
		} else {
			if(hex[0] == '\"')
				return (i >> 1);
			if(!is_hex(hex[0]))
				return -1;
		}
	}

	/* didn't reach the closing quotation mark */
	return -1;
}

/* String including quotes*/
#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define text_string(ctx, data, max_len) text_string_s(data, max_len)
bool text_string_s(uint8_t* data, int max_len) {
#else
bool text_string(readjson_ctx_t* ctx, uint8_t* data, int max_len) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	uint8_t byte;
	int i;

	one_char(ctx, &byte);

	if(byte != '\"')
		return false;

	for(i = 0; i < max_len; i++) {
		one_char(ctx, &byte);
		if(byte == '\"') {
			if(data)
				data[i] = 0;
			return true;
		}
		else {
			if(data)
				data[i] = byte;
		}
	}

	/* Normal return due to end of string didn't happen, string is too long*/
	return false;
}

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define integer_number(ctx, num) integer_number_s(num)
bool integer_number_s(uint32_t* num) {
#else
bool integer_number(readjson_ctx_t* ctx, uint32_t* num) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	uint32_t res = 0;
	bool valid = false;
	uint8_t byte;

	for(;;){
		peek_wrapper(ctx, &byte);
		if(byte >= '0' && byte <= '9') {
			res = res*10 + (byte - '0');
			valid = true;
		}
		else {
			break;
		}
		one_char(ctx, &byte);
	}
	*num = res;
	return valid;
}

/* Expected format is yyyy-mm-ddThh:mm:ssZ*/
#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define time_string(ctx, time) time_string_s(time)
bool time_string_s(uptane_time_t* time) {
#else
bool time_string(readjson_ctx_t *ctx, uptane_time_t* time) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	uint32_t num;

	fixed_data(ctx, "\"");
	if(!integer_number(ctx, &num))
		return false;
	time->year = num;

	fixed_data(ctx, "-");
	if(!integer_number(ctx, &num))
		return false;
	time->month = num;

	fixed_data(ctx, "-");
	if(!integer_number(ctx, &num))
		return false;
	time->day = num;

	fixed_data(ctx, "T");
	if(!integer_number(ctx, &num))
		return false;
	time->hour = num;

	fixed_data(ctx, ":");
	if(!integer_number(ctx, &num))
		return false;
	time->minute = num;

	fixed_data(ctx, ":");
	if(!integer_number(ctx, &num))
		return false;
	time->second = num;

	fixed_data(ctx, "Z\"");
	return true;
}

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define read_signatures(ctx) read_signatures_s()
bool read_signatures_s() {
	readjson_ctx_t* ctx = &readjson_ctx_s;
#else
bool read_signatures(readjson_ctx_t* ctx) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/

#if CRYPTO_SIGANTURE_LEN > CRYPTO_KEYID_LEN
#  define BUFSIZE CRYPTO_SIGNATURE_LEN
#else
#  define BUFSIZE CRYPTO_KEYID_LEN
#endif
	uint8_t buf[BUFSIZE];
	int i;
	int j;

	fixed_data(ctx, "[");
	
	for(i = 0; i < CONFIG_UPTANE_MAX_SIGS; i++) {
		bool ignore_sig = true;
		int current_sig;
		fixed_data(ctx, "{\"keyid\":");
		if(hex_string(ctx, buf, CRYPTO_KEYID_LEN) != CRYPTO_KEYID_LEN)
			return false;

		/* Find matching key */
		for(j = 0; j < ctx->sig_ctx.num_keys; j++)
			if(!memcmp(ctx->sig_ctx.sigs[i]->key->keyid, buf, CRYPTO_KEYID_LEN)) {
				ignore_sig = false;
				current_sig = j;
				break;
			}
		fixed_data(ctx, ",\"method\":");
		
		if(!text_string(ctx, buf, BUFSIZE))
			return false;

		if(!crypto_keytype_supported((const char*) buf))
			ignore_sig = true;

		fixed_data(ctx, ",\"sig\":");
		if(ignore_sig) {
			if(!ignore_string(ctx))
				return false;
		} else {
			/* In theory we can support multiple kinds of signatures,
			 * let CRYPTO_SIGNATURE_LEN be the maximum */
			if(hex_string(read_ctx, ctx->sig_ctx.sigs[current_sig]->sig, CRYPTO_SIGNATURE_LEN) <= 0)
				return false;
			ctx->sig_ctx.sig_valid[current_sig] = true;
			ctx->sig_ctx.crypto_ctx[current_sig] = crypto_verify_ctx_new();
			if(!ctx->sig_ctx.crypto_ctx[current_sig])
				return false;
			crypto_verify_init(ctx->sig_ctx.crypto_ctx[current_sig], ctx->sig_ctx.sigs[current_sig]);
		}

		fixed_data(ctx, "}");

		one_char(ctx, buf);

		/* End of signature array*/
		if(*buf == ']')
			break;
		/* If array hasn't ended, expecting new element after a comma */
		else if(*buf != ',')
			return false;
	}

	/* Have we reached the end of array? */
	return (i != CONFIG_UPTANE_MAX_SIGS);
}

#if defined(CONFIG_UPTANE_SINGLETHREADED)
#define read_verify_signed(ctx) read_verify_signed_s()
bool read_verify_signed_s() {
	readjson_ctx_t* ctx = &readjson_ctx_s;
#else
bool read_verify_signed(readjson_ctx_t* ctx) {
#endif /*defined(CONFIG_UPTANE_SINGLETHREADED)*/
	int i;
	uint32_t valid_sigs = 0;

	for(i = 0; i < ctx->sig_ctx.num_keys; i++)
		if(ctx->sig_ctx.sig_valid[i] && crypto_verify_result(ctx->sig_ctx.crypto_ctx[i]))
			++valid_sigs;
	return (valid_sigs >= ctx->sig_ctx.threshold);

}
