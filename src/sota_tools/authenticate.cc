#include "authenticate.h"

#include "logging/logging.h"
#include "oauth2.h"
#include "server_credentials.h"

using std::string;

int authenticate(const string &cacerts, const ServerCredentials &creds, TreehubServer &treehub) {
  switch (creds.GetMethod()) {
    case AUTH_BASIC: {
      treehub.SetAuthBasic(creds.GetAuthUser(), creds.GetAuthPassword());
      break;
    }

    case OAUTH2: {
      OAuth2 oauth2(creds.GetAuthServer(), creds.GetClientId(), creds.GetClientSecret(), cacerts);

      if (!creds.GetClientId().empty()) {
        if (oauth2.Authenticate() != AUTHENTICATION_SUCCESS) {
          LOG_FATAL << "Authentication with oauth2 failed";
          return EXIT_FAILURE;
        }
        LOG_INFO << "Using oauth2 authentication token";
        treehub.SetToken(oauth2.token());

      } else {
        LOG_INFO << "Skipping Authentication";
      }
      // Set ca certificate path, because curl embeds the path to ca-certs that it was built with
      // and this breaks under bitbake when sharing sstate cache between machines
      treehub.ca_certs(cacerts);
      break;
    }
    case CERT: {
      treehub.SetCerts(creds.GetRootCert(), creds.GetClientCert(), creds.GetClientKey());
      break;
    }
    case AUTH_NONE:
      treehub.ca_certs(cacerts);  // Setup ca certificate because curl by default check ca certs
      break;

    default: {
      LOG_FATAL << "Unexpected authentication method value " << creds.GetMethod();
      return EXIT_FAILURE;
    }
  }
  treehub.root_url(creds.GetOSTreeServer());
  treehub.repo_url(creds.GetRepoUrl());

  return EXIT_SUCCESS;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
