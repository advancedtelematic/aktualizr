#ifndef SOTA_CLIENT_TOOLS_OAUTH2_H_
#define SOTA_CLIENT_TOOLS_OAUTH2_H_

#include <string>

enum AuthenticationResult {
  AUTHENTICATION_SUCCESS = 0,
  AUTHENTICATION_FAILURE
};

class OAuth2 {
 public:
  /**
   * Doesn't perform any authentication
   */
  OAuth2(const std::string server, const std::string client_id,
         const std::string client_secret, const std::string &ca_certs)
      : server_(server),
        client_id_(client_id),
        client_secret_(client_secret),
        ca_certs_(ca_certs) {}

  /**
   * Synchronously attempt to get an access token from Auth+
   */
  AuthenticationResult Authenticate();

  std::string token() const { return token_; }

 private:
  const std::string server_;
  const std::string client_id_;
  const std::string client_secret_;
  const std::string ca_certs_;
  std::string token_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OAUTH2_H_
