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

#include <boost/regex.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include <curl/curl.h>

#include "logger.hpp"

#include "servercon.hpp"

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. I handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t WriteCallback(void* contents, size_t size, size_t nmemb,
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
servercon::servercon(void) {
  // initialize curl
  curl_global_init(CURL_GLOBAL_ALL);

  // create a curl handler
  defaultCurlHndl = curl_easy_init();

  // let curl use our write function
  curl_easy_setopt(defaultCurlHndl, CURLOPT_WRITEFUNCTION, WriteCallback);
  // let curl write data to the location we want
  curl_easy_setopt(defaultCurlHndl, CURLOPT_WRITEDATA, (void*)&serverResp);

  if (logger_getSeverity() <= LVL_debug) {
    curl_easy_setopt(defaultCurlHndl, CURLOPT_VERBOSE, 1L);
  }

  token = new oauthToken();
  authserver = "";
  sotaserver = "";
  clientID = "";
  clientSecret = "";
  serverResp = "";
}
/*****************************************************************************/
servercon::~servercon(void) {
  curl_easy_cleanup(defaultCurlHndl);
  free(token);
}

/*****************************************************************************/
void servercon::setDevUUID(const std::string& uuid_in) { devUUID = uuid_in; }

/*****************************************************************************/
void servercon::setClientID(const std::string& ID_in) { clientID = ID_in; }

/*****************************************************************************/
void servercon::setClientSecret(const std::string& sec_in) {
  clientSecret = sec_in;
}

/*****************************************************************************/
void servercon::setAuthServer(const std::string& server_in) {
  authserver = server_in;
}

/*****************************************************************************/
void servercon::setSotaServer(const std::string& server_in) {
  sotaserver = server_in;
}

/*****************************************************************************/
unsigned int servercon::get_oauthToken(void) {
  unsigned int returnValue;

  std::stringstream
      curlstr; /**< a stringstream used to compose curl arguments */

  CURLcode result; /**< curl error code */

  returnValue = 0u;

  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  CURL* curlHndl = curl_easy_duphandle(defaultCurlHndl);

  if (curlHndl) {
    // compose the url to get the token
    curlstr << authserver << "/token";

    // set the url
    curl_easy_setopt(curlHndl, CURLOPT_URL, curlstr.str().c_str());
    // let curl put the username and password using HTTP basic authentication
    curl_easy_setopt(curlHndl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

    // set the data curl shoudl post
    // this tells curl to do a HTTP POST
    curl_easy_setopt(curlHndl, CURLOPT_POSTFIELDS,
                     "grant_type=client_credentials");

    // reset our temporary string
    curlstr.str("");
    curlstr.clear();

    // compose username and password
    curlstr << clientID << ":" << clientSecret;
    // forward username and password to curl
    curl_easy_setopt(curlHndl, CURLOPT_USERPWD, curlstr.str().c_str());

    LOGGER_LOG(LVL_debug,
               "servercon - requesting token from server: " << authserver);

    // let curl perform the configured HTTP action
    result = curl_easy_perform(curlHndl);

    // check if curl succeeded
    if (result != CURLE_OK) {
      LOGGER_LOG(LVL_debug,
                 "servercon - curl error: " << result << " "
                                                         "with server: "
                                            << authserver << " "
                                                             "and clientID: "
                                            << clientID)
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
      if (boost::regex_search(serverResp, regex_result, expr)) {
        // check if enough matches were found
        // boost::regex stores the whole string that was checked in <result>[0]
        if (regex_result.size() >= 3u) {
          // set up current token
          token =
              new oauthToken(regex_result[1], regex_result[2], regex_result[3]);
          returnValue = 1;
          if (token->stillValid())
            LOGGER_LOG(LVL_debug, "servercon - Oauth2 token received");
        } else {
          LOGGER_LOG(LVL_debug,
                     "servercon - no token found in server response:\n"
                         << serverResp);
        }
      }
    }
  }

  // clean up curl
  curl_easy_cleanup(curlHndl);

  return returnValue;
}

/*****************************************************************************/
unsigned int servercon::get_availableUpdates(void) {
  unsigned int returnValue = 0;
  bool tokenOK = false;
  std::stringstream
      curlstr; /**< a stringstream used to compose curl arguments */

  CURLcode result;

  if (!token->stillValid()) {
    if (get_oauthToken()) {
      tokenOK = true;
    }
  } else {
    tokenOK = true;
  }

  if (tokenOK) {
    // copy default handle
    CURL* curlHndl = curl_easy_duphandle(defaultCurlHndl);

    // compose server url
    curlstr << sotaserver << "/api/v1/mydevice/" << devUUID << "/updates";
    // set the url
    curl_easy_setopt(curlHndl, CURLOPT_URL, curlstr.str().c_str());
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
    curl_easy_setopt(curlHndl, CURLOPT_HTTPHEADER, slist);

    // let curl perform the configured HTTP action
    result = curl_easy_perform(curlHndl);

    // check if curl succeeded
    if (result != CURLE_OK) {
      LOGGER_LOG(LVL_debug, "servercon - curl error: "
                                << result << "with server: " << sotaserver
                                << "and clientID: " << clientID)
    } else {
      // for now, just log the resonse from the server
      LOGGER_LOG(LVL_debug, "get udpate list : cURL response\n" << serverResp);

      // TODO issue #23 process response data using a JSON parser

      // set the return value to success
      returnValue = 1;
    }

    // clean up used curl ressources
    curl_slist_free_all(slist); /* free the list again */
    curl_easy_cleanup(curlHndl);
  }

  return returnValue;
}

}  // namespace sota_server
