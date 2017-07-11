#include "targets.h"
#include <string.h>
#include <limits.h>

#include "uptane_time.h"
#include "uptane_config.h"
#include "crypto.h"
#include "readjson.h"
/* Grammar of director/targets.json
 *
 
   {"signatures":\[({"keyid":"<hexstring>","method":"<string>","sig":"<hexstring>"},?)+\],"signed":{"_type":"<string>","expires":"<time>","targets":{("<string>":{"custom":{"ecu_identifier":"<string>","hardware_identifier":"<string>","release_counter":<number>},"hashes":{("<string>":"<hexstring>")+},"length":<number>},?)+},"version":<number>}}
 
*/

struct targets_ctx {
	/* Read/verify context */
	readjson_ctx_t* read_ctx;

	/* Inputs */
	uint32_t version_prev;
	uptane_time_t time;
	const uint8_t* ecu_id;
	const uint8_t* hardware_id;

	/* Outputs */
	uint8_t sha512_hash[SHA512_HASH_SIZE]; /* Only one hash is currently supported*/
	uint32_t version;
	uint32_t length;
	targets_result_t res;
};

#ifdef CONFIG_UPTANE_NOMALLOC
#  ifdef CONFIG_UPTANE_SINGLETHREADED
static targets_ctx_t targets_ctx_s;
#  else
static targets_ctx_t targets_ctxs[CONFIG_UPTANE_TARGETS_NUM_CONTEXTS];
static bool targets_ctx_busy[CONFIG_UPTANE_TARGETS_NUM_CONTEXTS] = {0, }; /*could be reduce to a bitmask when new/free performance is an issue*/
#  endif
#endif

targets_ctx_t* targets_ctx_new() {
#if defined(CONFIG_UPTANE_SINGLETHREADED)
	return &targets_ctx_s;
#elif defined(CONFIG_UPTANE_NOMALLOC)
	int i;

	for(i = 0; i < CONFIG_UPTANE_TARGETS_NUM_CONTEXTS; i++)
		if(!targets_ctx_busy[i])
			return &targets_ctxs[i];
	return NULL;
#else
	return malloc(sizeof(targets_ctx_t));

#endif
}

void targets_ctx_free(targets_ctx_t* ctx) {
	int i;
#if defined(CONFIG_UPTANE_NOMALLOC) && !defined(CONFIG_UPTANE_SINGLETHREADED)
	uintptr_t j = ((uintptr_t) ctx - (uintptr_t) targets_ctxs)/sizeof(targets_ctx_t); 
#endif

	readjson_ctx_free(ctx->read_ctx);
#if defined(CONFIG_UPTANE_SINGLETHREADED)
#elif defined(CONFIG_UPTANE_NOMALLOC)
	targets_ctx_busy[j] = 0;
#else
	free(ctx);
#endif
}

bool targets_init(targets_ctx_t* ctx, int version_prev, uptane_time_t time,
		  const uint8_t* ecu_id, const uint8_t* hardware_id,
		  const role_keys_t* targets_keys, const read_cb_t* callbacks) {
	int i;

	ctx->version_prev = version_prev;
	ctx->time = time;

	ctx->ecu_id = ecu_id;
	ctx->hardware_id = hardware_id;

	ctx->read_ctx = readjson_ctx_new();
	if(!ctx->read_ctx)
		return false;
	return readjson_init(ctx->read_ctx, targets_keys, callbacks);
}


targets_result_t targets_process(targets_ctx_t* ctx) {
	uint8_t buf[CONFIG_UPTANE_TARGETS_BUF_SIZE];
	uptane_time_t time;
	uint32_t number = 0;
	bool got_image = false;
	bool got_hash = false;

	fixed_data(ctx->read_ctx, "{\"signatures\":");

	if(!read_signatures(&ctx->read_ctx))
		return TARGETS_JSONERR;

	
	fixed_data(ctx->read_ctx, ",\"signed\":");

	/* signed section started, verification is performed in read_verify_wrapper */
	read_enter_signed(ctx->read_ctx);
	fixed_data(ctx->read_ctx, "{\"_type\":");

	if(!text_string(ctx->read_ctx, buf, CONFIG_UPTANE_TARGETS_BUF_SIZE))
		return TARGETS_JSONERR;

	if(strcmp((const char*) buf, "Targets"))
		return TARGETS_WRONGTYPE;

	fixed_data(ctx->read_ctx, ",\"expires\":");

	if(!time_string(ctx, &time))
		return TARGETS_JSONERR;

	if(uptane_time_greater(ctx->time, time))
		return TARGETS_EXPIRED;

	fixed_data(ctx->read_ctx, ",\"targets\":{");

	/* Iterate over targets */
	for(;;) {
		bool ignore_image = false;

		/* Target path */
		if(!ignore_string(ctx))
			return TARGETS_JSONERR;

		fixed_data(ctx->read_ctx, ":{\"custom\":{\"ecu_identifier\":");

		if(!text_string(ctx->read_ctx, buf, CONFIG_UPTANE_TARGETS_BUF_SIZE))
			return TARGETS_JSONERR;

		if(strcmp((const char*) buf, (const char*) ctx->ecu_id))
			ignore_image = true;

		fixed_data(ctx->read_ctx, ",\"hardware_identifier\":");

		if(!text_string(ctx->read_ctx, buf, CONFIG_UPTANE_TARGETS_BUF_SIZE))
			return TARGETS_JSONERR;

		if(strcmp((const char*) buf, (const char*) ctx->hardware_id))
			ignore_image = true;

		fixed_data(ctx->read_ctx, ",\"release_counter\":");

		/* Ignore release counter */
		if(!integer_number(ctx, &number))
			return TARGETS_JSONERR;

		fixed_data(ctx->read_ctx, "},\"hashes\":{");

		/* Iterate over hashes */
		for(;;) {
			if(!text_string(ctx->read_ctx, buf, CONFIG_UPTANE_TARGETS_BUF_SIZE))
				return TARGETS_JSONERR;
			fixed_data(ctx->read_ctx, ":");

			if(!ignore_image && !strcmp((const char*) buf, "sha512")) {
				if(hex_string(ctx->read_ctx, ctx->sha512_hash,
					      SHA512_HASH_SIZE) != SHA512_HASH_SIZE)
					return TARGETS_JSONERR;
				got_hash = true;
			} else {
				if(!ignore_string(ctx))
					return TARGETS_JSONERR;
			}

			one_char(ctx->read_ctx, buf);

			if(*buf == '}')
				break;
			else if(*buf != ',')
				return TARGETS_JSONERR;
		} /* iterate over hashes */

		fixed_data(ctx->read_ctx, ",\"length\":");

		if(!integer_number(ctx, &ctx->length))
			return TARGETS_JSONERR;

		if(!ignore_image) {
			if(got_image)
				return TARGETS_ECUDUPLICATE;
			else
				got_image = true;
		}
		fixed_data(ctx->read_ctx, "}");

		one_char(ctx->read_ctx, buf);
		if(*buf == '}')
			break;
		else if(*buf != ',')
			return TARGETS_JSONERR;

	} /* iterate over targets*/

	fixed_data(ctx->read_ctx, ",\"version\":");

	if(!integer_number(ctx, &ctx->version))
		return TARGETS_JSONERR;

	if(ctx->version < ctx->version_prev)
		return TARGETS_DOWNGRADE;

	fixed_data(ctx->read_ctx, "}");

	/* signed section ended, verify */
	read_exit_signed(ctx->read_ctx);

	if(!read_verify_signed(ctx->read_ctx))
		return TARGETS_SIGFAIL;
	
	/* trailing '}', EOF */
	fixed_data(ctx->read_ctx, "}");

	if(!got_image)
		return TARGETS_OK_NOIMAGE;

	if(!got_hash)
		return TARGETS_NOHASH;

	if(ctx->version == ctx->version_prev)
		return TARGETS_OK_NOUPDATE;

	return TARGETS_OK_UPDATE;
}

void targets_get_result(const targets_ctx_t* ctx, uint8_t* sha512_hash, int* length, int* version) {
	memcpy(sha512_hash, ctx->sha512_hash, SHA512_HASH_SIZE);
	*length = ctx->length;
	*version = ctx->version;
}
