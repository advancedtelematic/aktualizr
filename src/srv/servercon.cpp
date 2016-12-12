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

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
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
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
   // clear the provided string
   ((std::string*)userp)->clear();

   // append the writeback data to the provided string
   ((std::string*)userp)->append((char*)contents, size * nmemb);

   // return size of written data
   return size * nmemb;
}


/*****************************************************************************/
namespace sota_server{


void servercon::setServer(std::string& srv_in)
{
   server = srv_in;
}

/*****************************************************************************/
void servercon::setClientID(std::string& ID_in)
{
   clientID = ID_in;
}

/*****************************************************************************/
void servercon::setClientSecret(std::string& sec_in)
{
   clientSecret = sec_in;
}

/*****************************************************************************/
unsigned int servercon::get_oauthToken(void)
{
   unsigned int returnValue;

   std::stringstream curlstr; /**< a stringstream used to compose curl arguments */

   CURLcode result;  /**< curl error code */

   // compose the url to get the token
   curlstr << server << "/token";

   // initialize curl
   curl_global_init( CURL_GLOBAL_ALL );

   // create a curl handler
   CURL* curl = curl_easy_init();

   //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

   if(curl)
   {

      // let curl use our write function
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      // let curl write data to the location we want
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&serverResp);

      // set the url
      curl_easy_setopt( curl, CURLOPT_URL, curlstr.str().c_str());
      // let curl put the username and password using HTTP basic authentication
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

      // set the data curl shoudl post
      // this tells curl to do a HTTP POST
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "grant_type=client_credentials");

      // reset our temporary string
      curlstr.str("");
      curlstr.clear();

      // compose username and password
      curlstr << clientID << ":" << clientSecret;
      // forward username and password to curl
      curl_easy_setopt(curl, CURLOPT_USERPWD, curlstr.str().c_str());

      LOGGER_LOG(LVL_debug, "servercon - requesting token from server: " << server);

      // let curl perform the configured HTTP action
      result = curl_easy_perform( curl );

      // check if curl succeeded
      if(result != CURLE_OK)
      {
         LOGGER_LOG(LVL_warning, "servercon - curl error: " << result << " "
                    "with server: " << server << " "
                    "and clientID: " << clientID )
      }
      else
      {
         // TODO issue #23 process response data using a JSON parser
         // create a regex pattern that checks for the token itself, the token type and the expire time
         // "acces_token":"xyz","token_type":"xyz","expires_in":"0123","
         boost::regex expr("\"access_token\":\"(\\S+)\",\"token_type\":\"(\\w+)\",\"expires_in\":(\\d+),\"");

         // create an object for the regex results
         boost::smatch regex_result;

         // check if the regex matches the return from curl
         if (boost::regex_search(serverResp, regex_result, expr))
         {
            // check if enough matches were found
            // boost::regex stores the whole string that was checked in <result>[0]
            if ( regex_result.size() >= 3u )
            {
               // set up current token
               token = new oauthToken(regex_result[1], regex_result[2], regex_result[3]);
               returnValue = 1;
               if (token->stillValid()) LOGGER_LOG(LVL_debug, "servercon - Oauth2 token received");
            }
            else
            {
               LOGGER_LOG(LVL_warning, "servercon - no token found in server response:\n" << serverResp);
            }
         }
      }
   }

   // clean up curl
   curl_easy_cleanup( curl );

   return returnValue;
}

} // namespace sota_server

/*****************************************************************************/
void server_tryout(sota_server::servercon* sota_serverPtr)
{
   (void)sota_serverPtr->get_oauthToken();
}
