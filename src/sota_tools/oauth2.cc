#include <assert.h>

#include <iostream>
#include <sstream>

#include <curl/curl.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "logging/logging.h"
#include "oauth2.h"
#include "utilities/utils.h"

using boost::property_tree::ptree;
using boost::property_tree::json_parser::json_parser_error;
using std::stringstream;

/**
 * Handle CURL write callbacks by appending to a stringstream
 */
size_t curl_handle_write_sstream(void *buffer, size_t size, size_t nmemb, void *userp) {
  auto *body = static_cast<stringstream *>(userp);
  body->write(static_cast<const char *>(buffer), static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

AuthenticationResult OAuth2::Authenticate() {
  CurlEasyWrapper curl_handle;
  std::string token_suffix = "/token";
  std::string post_data = "grant_type=client_credentials";
  auto use_cognito = false;
  if (server_.length() >= token_suffix.length()) {
    use_cognito = (0 == server_.compare(server_.length() - token_suffix.length(), token_suffix.length(), token_suffix));
  }

  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
  // We need this check for backwards-compatibility with previous versions of treehub.json.
  // Previous versions have the server URL *without* the token path, so it needs to be hardcoded.
  // The new version has the full URL with the `/oauth2/token` path at the end, so nothing needs
  // to be appended. Also we can't send the `scope` to Auth+, it confuses it.
  // This check can be removed after we finish the migration to Cognito. At that point there's
  // no need to attempt to be backwards-compatible anymore, since old credentials will have a
  // client_id that will have been removed from user-profile, and a server URL to Auth+, which
  // will be in computer heaven.
  // check similar implementation in Scala for garage-sign here:
  // https://main.gitlab.in.here.com/olp/edge/ota/connect/back-end/ota-tuf/-/blob/master/cli/src/main/scala/com/advancedtelematic/tuf/cli/http/OAuth2Client.scala
  if (use_cognito) {
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_URL, (server_).c_str());
  } else {
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_URL, (server_ + token_suffix).c_str());
  }
  if (!ca_certs_.empty()) {
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_CAINFO, ca_certs_.c_str());
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_CAPATH, NULL);
  }

  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_USERNAME, client_id_.c_str());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_PASSWORD, client_secret_.c_str());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_POST, 1);
  if (use_cognito) {
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_COPYPOSTFIELDS, (post_data + "&scope=" + scope_).c_str());
  } else {
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_COPYPOSTFIELDS, post_data.c_str());
  }

  stringstream body;
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_WRITEFUNCTION, &curl_handle_write_sstream);
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_WRITEDATA, &body);

  curl_easy_perform(curl_handle.get());

  long rescode;  // NOLINT(google-runtime-int)
  curl_easy_getinfo(curl_handle.get(), CURLINFO_RESPONSE_CODE, &rescode);
  if (rescode == 200) {
    ptree pt;
    try {
      read_json(body, pt);
      token_ = pt.get("access_token", "");
      LOG_TRACE << "Got OAuth2 access token:" << token_;
      return AuthenticationResult::kSuccess;
    } catch (const json_parser_error &e) {
      token_ = "";
      return AuthenticationResult::kFailure;
    }
  } else {
    return AuthenticationResult::kFailure;
  }
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
