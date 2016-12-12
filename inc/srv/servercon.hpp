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

#include "oauthtoken.hpp"


namespace sota_server{

/*****************************************************************************/
class servercon{

   // Attributes
private:
   std::string server;     /**< the server url*/
   std::string clientID;   /**< the client ID required for getting a OAuth2 token */
   std::string clientSecret;  /**< the client secret associated with the client ID */

   std::string serverResp; /**< A string that is used to store curl HTTP response data */

   oauthToken* token; /**< An object that handles a received OAuth2 token */

   // Operations
public:
   /**
    * \par Description:
    *    Set the server URL.
    */
   void setServer(std::string& srv_in);

   /**
    * \par Description:
    *    Set the clienID which is also used as user when
    *    connecting to the server.
    */
   void setClientID(std::string& ID_in);

   /**
    * \par Description:
    *    Set the clienID which is also used as user when
    *    connecting to the server.
    */
   void setClientSecret(std::string& sec_in);

   /**
    * \par Description:
    *    Ask the configured server to get an OAuth2 token.
    *
    * \return 1 if a token was received from the server
    */
   unsigned int get_oauthToken(void);

};

}  // namespace sota_server

/**
 * \par Description:
 *    Intermediate function to test servercon functionality.
 */
extern void server_tryout(sota_server::servercon* sota_serverPtr);

#endif
