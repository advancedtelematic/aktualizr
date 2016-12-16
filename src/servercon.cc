/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file oauthtoken.cpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  Methods for the class oauthtoken. Refer to the class header file
 *         oauthtoken.hpp
 *
 *
 ******************************************************************************
 *
 * \endcond
 */
#include "servercon.h"

#include <boost/regex.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <curl/curl.h>

#include "logger.h"

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. I handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t writeCallback(void* contents, size_t size, size_t nmemb,
                            void* userp) {
  // clear the provided string
  ((std::string*)userp)->clear();

  // append the writeback data to the provided string
  ((std::string*)userp)->append((char*)contents, size * nmemb);

  // return size of written data
  return size * nmemb;
}

/*****************************************************************************/
namespace sota_server {

/*****************************************************************************/
ServerCon::ServerCon(void) {
  // initialize curl
  curl_global_init(CURL_GLOBAL_ALL);

  // create a curl handler
  default_curl_hndl = curl_easy_init();

  // let curl use our write function
  curl_easy_setopt(default_curl_hndl, CURLOPT_WRITEFUNCTION, writeCallback);
  // let curl write data to the location we want
  curl_easy_setopt(default_curl_hndl, CURLOPT_WRITEDATA,
                   (void*)&server_response);

  if (loggerGetSeverity() <= LVL_debug) {
    curl_easy_setopt(default_curl_hndl, CURLOPT_VERBOSE, 1L);
  }

  token = new OAuthToken();
  authserver = "";
  sotaserver = "";
  client_ID = "";
  client_secret = "";
  server_response = "";
}
/*****************************************************************************/
ServerCon::~ServerCon(void) {
  curl_easy_cleanup(default_curl_hndl);
  free(token);
}

/*****************************************************************************/
void ServerCon::setDevUUID(const std::string& uuid_in) {
  device_UUID = uuid_in;
}

/*****************************************************************************/
void ServerCon::setClientID(const std::string& ID_in) { client_ID = ID_in; }

/*****************************************************************************/
void ServerCon::setClientSecret(const std::string& sec_in) {
  client_secret = sec_in;
}

/*****************************************************************************/
void ServerCon::setAuthServer(const std::string& server_in) {
  authserver = server_in;
}

/*****************************************************************************/
void ServerCon::setSotaServer(const std::string& server_in) {
  sotaserver = server_in;
}

/*****************************************************************************/
unsigned int ServerCon::getOAuthToken(void) {
  unsigned int return_value = 0u;

  std::stringstream
      curlstr; /**< a stringstream used to compose curl arguments */

  CURLcode result; /**< curl error code */

  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  CURL* curl_hndl = curl_easy_duphandle(default_curl_hndl);

  if (curl_hndl) {
    // compose the url to get the token
    curlstr << authserver << "/token";

    // set the url
    curl_easy_setopt(curl_hndl, CURLOPT_URL, curlstr.str().c_str());
    // let curl put the username and password using HTTP basic authentication
    curl_easy_setopt(curl_hndl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

    // set the data curl shoudl post
    // this tells curl to do a HTTP POST
    curl_easy_setopt(curl_hndl, CURLOPT_POSTFIELDS,
                     "grant_type=client_credentials");

    // reset our temporary string
    curlstr.str("");
    curlstr.clear();

    // compose username and password
    curlstr << client_ID << ":" << client_secret;
    // forward username and password to curl
    curl_easy_setopt(curl_hndl, CURLOPT_USERPWD, curlstr.str().c_str());

    LOGGER_LOG(LVL_debug,
               "servercon - requesting token from server: " << authserver);

    // let curl perform the configured HTTP action
    result = curl_easy_perform(curl_hndl);

    // check if curl succeeded
    if (result != CURLE_OK) {
      LOGGER_LOG(LVL_warning, "servercon - curl error: "
                                  << result << "with server: " << authserver
                                  << "and clientID: " << client_ID)
    } else {
      // TODO issue #23 process response data using a JSON parser
      // create a regex pattern that checks for the token itself, the token type
      // and the expire time
      // "acces_token":"xyz","token_type":"xyz","expires_in":"0123","
      boost::regex expr(
          "\"access_token\":\"(\\S+)\",\"token_type\":\"(\\w+)\",\"expires_"
          "in\":(\\d+),\"");

      // create an object for the regex results
      boost::smatch regex_result;

      // check if the regex matches the return from curl
      if (boost::regex_search(server_response, regex_result, expr)) {
        // check if enough matches were found
        // boost::regex stores the whole string that was checked in <result>[0]
        if (regex_result.size() >= 3u) {
          // set up current token
          token =
              new OAuthToken(regex_result[1], regex_result[2], regex_result[3]);
          return_value = 1;
          if (token->stillValid())
            LOGGER_LOG(LVL_debug, "servercon - Oauth2 token received");
        } else {
          LOGGER_LOG(LVL_warning,
                     "servercon - no token found in server response:\n"
                         << server_response);
        }
      }
    }
  }

  // clean up curl
  curl_easy_cleanup(curl_hndl);

  return return_value;
}

/*****************************************************************************/
unsigned int ServerCon::getAvailableUpdates(void) {
  unsigned int return_value = 0;
  bool token_OK = false;
  std::stringstream
      curlstr; /**< a stringstream used to compose curl arguments */

  CURLcode result;

  if (!token->stillValid()) {
    if (getOAuthToken()) {
      token_OK = true;
    }
  } else {
    token_OK = true;
  }

  if (token_OK) {
    // copy default handle
    CURL* curl_hndl = curl_easy_duphandle(default_curl_hndl);

    // compose server url
    curlstr << sotaserver << "/api/v1/mydevice/" << device_UUID << "/updates";
    // set the url
    curl_easy_setopt(curl_hndl, CURLOPT_URL, curlstr.str().c_str());
    // reset our temporary string
    curlstr.str("");
    curlstr.clear();

    // get token
    std::string tokenstr = token->get();

    // compose authorization header using token
    curlstr << "Authorization : Bearer " << tokenstr;

    // create a header list for curl
    struct curl_slist* slist = NULL;

    // append authorization header to the header list
    slist = curl_slist_append(slist, curlstr.str().c_str());
    // append accecpt header (as shown in the sota api example)
    slist = curl_slist_append(slist, "Accept: application/json");

    // tell curl to use the header list
    curl_easy_setopt(curl_hndl, CURLOPT_HTTPHEADER, slist);

    // let curl perform the configured HTTP action
    result = curl_easy_perform(curl_hndl);

    // check if curl succeeded
    if (result != CURLE_OK) {
      LOGGER_LOG(LVL_warning, "servercon - curl error: "
                                  << result << "with server: " << sotaserver
                                  << "and clientID: " << client_ID)
    } else {
      // for now, just log the resonse from the server
      LOGGER_LOG(LVL_debug, "get udpate list : cURL response\n"
                                << server_response);

      // TODO issue #23 process response data using a JSON parser

      // set the return value to success
      return_value = 1;
    }

    // clean up used curl ressources
    curl_slist_free_all(slist); /* free the list again */
    curl_easy_cleanup(curl_hndl);
  }

  return return_value;
}

}  // namespace sota_server
