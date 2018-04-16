#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "targets.h"
#include "uptane_time.h"
#include "utilities/crypto.h"

#define BUF_SIZE 2048
#define NUM_KEYS 16

bool eof_reached = false;

void peek_char(void* priv, uint8_t* buf) {
	FILE* inp = (FILE*) priv;
	char c = eof_reached ? EOF : fgetc(inp);
	if(c == EOF) {
		eof_reached = true;
		*buf = 0;
		return;
	}

	*buf = c;
	ungetc(c, inp);
	return;
}

void read_data(void* priv, uint8_t* buf, int len) {
	FILE* inp = (FILE*) priv;
	for(int i = 0; i < len; i++) {
		char c = eof_reached ? EOF : fgetc(inp);
		if(c == EOF)
			buf[i] = 0;
		else
			buf[i] = (uint8_t) c;
	}
	return;
}

crypto_key_t keys[NUM_KEYS];

const char* code_to_string(targets_result_t res) {
	switch(res) {
		case TARGETS_OK_NOIMAGE: return "Valid: no image";
		case TARGETS_OK_NOUPDATE: return "Valid: no update";
		case TARGETS_OK_UPDATE: return "Valid: update found";
		case TARGETS_NOMEM: return "Error: out of memory";
		case TARGETS_ECUDUPLICATE: return "Error: ECU ID mentioned twice";
		case TARGETS_NOHASH: return "Error: no valid hash";
		case TARGETS_SIGFAIL: return "Error: signature verification failed";
		case TARGETS_JSONERR: return "Error: malformed input";
		case TARGETS_WRONGTYPE: return "Error: wrong type of input";
		case TARGETS_EXPIRED: return "Error: object has expired";
		case TARGETS_DOWNGRADE: return "Error: new version is lower than previous one";
		default: return "Unknown error";
	}
}

static bool hash_matches(const uint8_t* bin_hash, const char* text_hash) {
	if(strlen(text_hash) != 2*SHA512_HASH_SIZE)
		return false;

	for(int i = 0; i < SHA512_HASH_SIZE; i++) {
		char hex[3];
		hex[0] = text_hash[2*i];
		hex[1] = text_hash[2*i + 1];
		hex[2] = 0;

		if(bin_hash[i] != strtoul(hex, NULL, 16))
			return false;
	}
	return true;
}

/* verify_targets targets.json keys.txt <threshold> <prev_version> <ecu_id> <hardware_id> <expected_hash>*/
int main(int argc, const char** argv) {
	FILE* keys_file = fopen(argv[2], "r");

	char buf[BUF_SIZE];

	int n = 0;
	while(fgets(buf, BUF_SIZE, keys_file)) {
		for(int i = 0; i < CRYPTO_KEYID_LEN; i++) {
			char hex[3];

			hex[0] = buf[2*i];
			hex[1] = buf[2*i + 1];
			hex[2] = 0;
			keys[n].keyid[i] = strtol(hex, NULL, 16);
		}

		for(int i = 0; i < CRYPTO_KEYVAL_LEN; i++) {
			char hex[3];

			hex[0] = buf[1+2*CRYPTO_KEYID_LEN+2*i];
			hex[1] = buf[1+2*CRYPTO_KEYID_LEN+2*i + 1];
			hex[2] = 0;
			keys[n].keyval[i] = strtol(hex, NULL, 16);
		}
		keys[n].key_type = ED25519;
		++n;
	}

	targets_ctx_t* ctx = targets_ctx_new();
	if(!ctx) {
		fprintf(stderr, "Couldn't allocate targets context\n");
		return 2;
	}
	FILE* targets_file = fopen(argv[1], "r");
	int threshold = strtol(argv[3], NULL, 10);
	int prev_version = strtol(argv[4], NULL, 10);
	struct timeval epoch_time;
	gettimeofday(&epoch_time, NULL);
	struct tm* time = gmtime(&epoch_time.tv_sec);
	uptane_time_t uptane_time = {1900 + time->tm_year, time->tm_mon, time->tm_mday,
				     time->tm_hour, time->tm_min, time->tm_sec};

	role_keys_t keyset = {keys, n, threshold};
	read_cb_t callbacks = {read_data, peek_char, targets_file};
	targets_init(ctx, prev_version, uptane_time, (const uint8_t*) argv[5],
		     (const uint8_t*) argv[6], &keyset, &callbacks);

	targets_result_t res = targets_process(ctx);
	int res_len;
	int res_version;
	uint8_t res_hash[SHA512_HASH_SIZE];
	targets_get_result(ctx, res_hash, &res_len, &res_version);

	printf("Result: %s\n", code_to_string(res));

	if((res == TARGETS_OK_NOIMAGE || res == TARGETS_OK_NOUPDATE || res == TARGETS_OK_UPDATE) && hash_matches(res_hash, argv[7]))
		return 0;
	else
		return 1;
}
