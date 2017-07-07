typedef enum {
	ROOT_OK,		/* Valid root.json			      */
	ROOT_MISSINGROLE,	/* Dynamic memory error			      */
	ROOT_NOMEM,		/* Dynamic memory error			      */
	ROOT_SIGFAIL,		/* Signature verification failed	      */
	ROOT_JSONERR,		/* Malformed JSON			      */
	ROOT_WRONGTYPE,		/* _type field is not "Root"		      */
	ROOT_EXPIRED,		/* targets.json has expired		      */
	ROOT_DOWNGRADE		/* New version is lower that the previous one */
} root_result_t;

struct root_ctx;
typedef struct root_ctx root_ctx_t;

root_ctx_t* root_ctx_new(void);
void root_ctx_free(root_ctx_t* ctx);

bool root_init(root_ctx_t* ctx, int version_prev, uptane_time_t time,
	       const role_keys_t* root_keys, read_cb_t callbacks);

root_result_t root_process(root_ctx_t* ctx);
void root_get_result(const root_ctx_t* ctx, role_keys_t* root_keys,
		     role_keys_t* targets_keys, root_keys_t* time_keys,
		     int* version);
