#ifndef SOTA_CLIENT_TOOLS_OAUTH2_H_
#define SOTA_CLIENT_TOOLS_OAUTH2_H_

#include <string>
#include <utility>

enum class AuthenticationResult { kSuccess = 0, kFailure };

class OAuth2 {
 public:
  /**
   * Doesn't perform any authentication
   */
  OAuth2(std::string server, std::string client_id, std::string client_secret, std::string scope, std::string ca_certs)
      : server_(std::move(server)),
        client_id_(std::move(client_id)),
        client_secret_(std::move(client_secret)),
        scope_(std::move(scope)),
        ca_certs_(std::move(ca_certs)) {}

  /**
   * Synchronously attempt to get an access token from Auth+
   */
  AuthenticationResult Authenticate();

  std::string token() const { return token_; }

 private:
  const std::string server_;
  const std::string client_id_;
  const std::string client_secret_;
  const std::string scope_;
  const std::string ca_certs_;
  std::string token_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OAUTH2_H_
