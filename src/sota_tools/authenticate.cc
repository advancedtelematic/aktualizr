#include "authenticate.h"

#include "logging/logging.h"
#include "oauth2.h"

using std::string;

int authenticate(const string &cacerts, const ServerCredentials &creds, TreehubServer &treehub) {
  switch (creds.GetMethod()) {
    case AuthMethod::kBasic: {
      treehub.SetAuthBasic(creds.GetAuthUser(), creds.GetAuthPassword());
      break;
    }
    case AuthMethod::kOauth2: {
      OAuth2 oauth2(creds.GetAuthServer(), creds.GetClientId(), creds.GetClientSecret(), creds.GetScope(), cacerts);

      if (!creds.GetClientId().empty()) {
        if (oauth2.Authenticate() != AuthenticationResult::kSuccess) {
          LOG_FATAL << "Authentication with oauth2 failed";
          return EXIT_FAILURE;
        }
        LOG_INFO << "Using oauth2 authentication token";
        treehub.SetToken(oauth2.token());

      } else {
        LOG_INFO << "Skipping Authentication";
      }
      break;
    }
    case AuthMethod::kTls: {
      treehub.SetCerts(creds.GetClientP12());
      break;
    }
    case AuthMethod::kNone:
      break;
    default: {
      LOG_FATAL << "Unexpected authentication method value " << static_cast<int>(creds.GetMethod());
      return EXIT_FAILURE;
    }
  }
  // Setup ca certificates in all cases. Even with no authentication, curl
  // checks ca certs by default. Furthermore, curl embeds the path to ca certs
  // that it was built with and this breaks under bitbake when sharing sstate
  // cache between machines.
  treehub.ca_certs(cacerts);
  treehub.root_url(creds.GetOSTreeServer());
  treehub.repo_url(creds.GetRepoUrl());

  return EXIT_SUCCESS;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
