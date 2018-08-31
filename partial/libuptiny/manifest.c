#include "manifest.h"
#include "base64.h"
#include "crypto_api.h"
#include "state_api.h"
#include "utils.h"

#include <string.h>

static const char* attack_to_string(uptane_attack_t attack) {
  switch (attack) {
    case ATTACK_NONE:
      return "";
    case ATTACK_ROOT_THRESHOLD:
      return "Failed threshold for root metadata";
    case ATTACK_TARGETS_THRESHOLD:
      return "Failed threshold for targets metadata";
    case ATTACK_ROOT_VERSION:
      return "Root rollback attempted";
    case ATTACK_TARGETS_VERSION:
      return "Targets rollback attempted";
    case ATTACK_ROOT_EXPIRED:
      return "Root metadata has expired";
    case ATTACK_TARGETS_EXPIRED:
      return "Targets metadata has expired";
    case ATTACK_ROOT_LARGE:
      return "Root metadata size exceeds the limit";
    case ATTACK_TARGETS_LARGE:
      return "Targets metadata size exceeds the limit";
    case ATTACK_IMAGE_HASH:
      return "Firmware image hash verification failed";
    case ATTACK_IMAGE_LARGE:
      return "Firmware image length mismatch";
    default:
      return "Unknown";
  }
}

static const char* hash_alg_to_string(crypto_hash_algorithm_t alg) {
  switch (alg) {
    case CRYPTO_HASH_SHA512:
      return "sha512";
    default:
      return "Unknown";
  }
}

static const char* crypto_alg_to_method(crypto_algorithm_t alg) {
  switch (alg) {
    case CRYPTO_ALG_ED25519:
      return "ed25519";
    default:
      return "Unknown";
  }
}

void uptane_write_manifest(char* signed_part, char* signatures_part) {
  /* All strings are either literals or taken from trusted environment, use 'unsafe' functions */
  uptane_installation_state_t* state = state_get_installation_state();
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcpy(signed_part, "{\"attacks_detected\":\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, (state) ? attack_to_string(state->attack) : "");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\",\"ecu_serial\":\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, state_get_ecuid());
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\",\"installed_image\":{\"fileinfo\":{\"hashes\":{\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, (state) ? hash_alg_to_string(state->firmware_hash.alg) : "nohash");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\":\"");
  if (state) {
    bin2hex(state->firmware_hash.hash, (int)crypto_get_hashlen(state->firmware_hash.alg),
            signed_part + strlen(signed_part));
  }
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\"},\"length\":");
  int2dec((state) ? (int32_t)state->firmware_length : 0, signed_part + strlen(signed_part));
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "},\"filepath\":\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, (state) ? state->firmware_name : "noimage");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\"},\"previous_timeserver_time\":\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "1970-01-01T00:00:00Z");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\",\"timeserver_time\":\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "1970-01-01T00:00:00Z");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signed_part, "\"}");

  crypto_key_and_signature_t sig;
  const uint8_t* priv;
  state_get_device_key(&sig.key, &priv);

  crypto_sign_data(signed_part, strlen(signed_part), &sig, priv);
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcpy(signatures_part, "[{\"keyid\":\"");
  bin2hex(sig.key->keyid, CRYPTO_KEYID_LEN, signatures_part + strlen(signatures_part));
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signatures_part, "\",\"method\":\"");
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signatures_part, crypto_alg_to_method(sig.key->key_type));
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signatures_part, "\",\"sig\":\"");
  base64_encode(sig.sig, crypto_get_siglen(sig.key->key_type), signatures_part + strlen(signatures_part));
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcat(signatures_part, "\"}]");
}
