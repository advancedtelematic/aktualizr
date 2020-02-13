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
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_URL, (server_ + "/token").c_str());
  if (ca_certs_ != "") {
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_CAINFO, ca_certs_.c_str());
    curlEasySetoptWrapper(curl_handle.get(), CURLOPT_CAPATH, NULL);
  }

  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_USERNAME, client_id_.c_str());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_PASSWORD, client_secret_.c_str());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_POST, 1);
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_COPYPOSTFIELDS, "grant_type=client_credentials");

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
    // TODO: be more specfic about the failure cases
    return AuthenticationResult::kFailure;
  }
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
