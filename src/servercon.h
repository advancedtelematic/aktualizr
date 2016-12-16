/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file servercon.hpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  A class handling connections to a sota server
 *
 * \par Purpose
 *      This module implements a class servercon that manages connection
 *      to a sota server.
 *
 ******************************************************************************
 *
 * \endcond
 */

#ifndef SERVERCON_H_
#define SERVERCON_H_

#include <curl/curl.h>

#include "oauthtoken.h"

namespace sota_server {

/*****************************************************************************/
class ServerCon {
  // Operations
 public:
  /**
   * \par Description:
   *    Set the server URL.
   */
  void setAuthServer(const std::string& srv_in);

  /**
   * \par Description:
   *    Set the server URL.
   */
  void setSotaServer(const std::string& srv_in);

  /**
   * \par Description:
   *    Set the clienID which is also used as user when
   *    connecting to the server.
   */
  void setClientID(const std::string& ID_in);

  /**
   * \par Description:
   *    Set the clienID which is also used as user when
   *    connecting to the server.
   */
  void setClientSecret(const std::string& sec_in);

  void setDevUUID(const std::string& uuid_in);

  /**
   * \par Description:
   *    Ask the configured server to get an OAuth2 token.
   *
   * \return 1 if a token was received from the server
   */
  unsigned int getOAuthToken(void);

  /**
   * \par Description:
   *    Checks for a valid token and gets a new token if no valid token is
   *    available. The token is then used to get a list of available updates
   *    for the configured device (UUID).
   *
   * \return 1 if a valid response from the server was received
   */
  unsigned int getAvailableUpdates(void);

  ServerCon(void);

  ~ServerCon(void);

  // Attributes
 private:
  std::string authserver; /**< the authentification server url*/
  std::string sotaserver;
  std::string
      client_ID; /**< the client ID required for getting a OAuth2 token */
  std::string
      client_secret; /**< the client secret associated with the client ID */
  std::string device_UUID;

  std::string server_response; /**< A string that is used to store curl HTTP
                                  response data */

  OAuthToken* token; /**< An object that handles a received OAuth2 token */

  CURL* default_curl_hndl; /**< store a default configuration for all curl
                            operations */
};

}  // namespace sota_server

#endif
