#include "sslinit.h"
#include <crypto/openssl_compat.h>
#include <openssl/ssl.h>
#include <pthread.h>

#if AKTUALIZR_OPENSSL_PRE_11
static pthread_mutex_t *lock_cs;
static long *lock_count;

// static void pthreads_locking_callback(int mode, int type, const char *, int);
// static unsigned long pthreads_thread_id(void);

void pthreads_locking_callback(int mode, int type, const char *, int) {
#if 0
  fprintf(stderr, "thread=%4d mode=%s lock=%s %s:%d\n",
            CRYPTO_thread_id(),
            (mode & CRYPTO_LOCK) ? "l" : "u",
            (type & CRYPTO_READ) ? "r" : "w", file, line);
#endif
#if 0
  if (CRYPTO_LOCK_SSL_CERT == type)
        fprintf(stderr, "(t,m,f,l) %ld %d %s %d\n",
                CRYPTO_thread_id(), mode, file, line);
#endif
  if (mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(lock_cs[type]));
    lock_count[type]++;
  } else {
    pthread_mutex_unlock(&(lock_cs[type]));
  }
}

unsigned long pthreads_thread_id(void) {
  unsigned long ret;

  ret = (unsigned long)pthread_self();
  return (ret);
}

void openssl_callbacks_setup(void) {
  int i;

  lock_cs =
      (pthread_mutex_t *)OPENSSL_malloc(static_cast<int>((unsigned long)CRYPTO_num_locks() * sizeof(pthread_mutex_t)));
  lock_count = (long *)OPENSSL_malloc(static_cast<int>((unsigned long)CRYPTO_num_locks() * sizeof(long)));
  if (!lock_cs || !lock_count) {
    /* Nothing we can do about this...void function! */
    if (lock_cs) OPENSSL_free(lock_cs);
    if (lock_count) OPENSSL_free(lock_count);
    return;
  }
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    lock_count[i] = 0;
    pthread_mutex_init(&(lock_cs[i]), NULL);
  }

  CRYPTO_set_id_callback((unsigned long (*)())pthreads_thread_id);
  CRYPTO_set_locking_callback(pthreads_locking_callback);
}

void openssl_callbacks_cleanup(void) {
  int i;

  CRYPTO_set_locking_callback(NULL);
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    pthread_mutex_destroy(&(lock_cs[i]));
  }
  OPENSSL_free(lock_cs);
  OPENSSL_free(lock_count);
}
#else
void openssl_callbacks_setup(void){};
void openssl_callbacks_cleanup(void){};
#endif
