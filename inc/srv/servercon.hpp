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
#include "oauthtoken.hpp"

namespace sota_server {

typedef struct {
  std::string UUID;
  std::string name;
  std::string version;
} server_updateInfo_t;

/*****************************************************************************/
class servercon {
  // Attributes
 private:
  std::string authserver; /**< the authentification server url*/
  std::string sotaserver;
  std::string
      clientID; /**< the client ID required for getting a OAuth2 token */
  std::string
      clientSecret; /**< the client secret associated with the client ID */
  std::string devUUID;

  std::string
      serverResp; /**< A string that is used to store curl HTTP response data */

  oauthToken* token; /**< An object that handles a received OAuth2 token */

  CURL* defaultCurlHndl; /**< store a default configuration for all curl
                            operations */

  server_updateInfo_t update;

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
  unsigned int get_oauthToken(void);

  /**
   * \par Description:
   *    Checks for a valid token and gets a new token if no valid token is
   *    available. The token is then used to get a list of available updates
   *    for the configured device (UUID).
   *
   * \return 1 if a valid response from the server was received
   */
  unsigned int get_availableUpdates(void);

  unsigned int download_update(void);

  servercon(void);

  ~servercon(void);
};

}  // namespace sota_server

#endif
