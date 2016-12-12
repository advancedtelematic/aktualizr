/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file oauthtoken.hpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  A class handling OAuth2 tokens
 *
 * \par Purpose
 *      This module implements a class oauthtoken that provides simple
 *      handling of OAuth2 tokens.
 *
 ******************************************************************************
 *
 * \endcond
 */

/*****************************************************************************/

#ifndef OAUTHTOKEN_H_
#define OAUTHTOKEN_H_

#include <string>

namespace sota_server {

class oauthToken{

// Attributes
private:
   std::string token;   /**< the OAuth2 token stored as string */
   std::string type;    /**< the token type as provided by the server */
   unsigned int expire; /**< the expire integer as provided by the server */
   time_t stored;       /**< the time when the token was stored */

// Operations
public:
   /**
    * \par Description
    *    This constructor stores a OAuth2 token. It parses a JSON server response.
    *
    * \param[in] response - a string containing the server response.
    */
   oauthToken(const std::string& token_in, const std::string& type_in, const std::string& expire_in);

   /**
    * \par Description:
    *    A getter for the stored token.
    *
    * \return OAuth2 token as string
    */
   std::string get(void);

   /**
    * \par Description:
    *    Checks if the stored token is still valid by evaluating the time when
    *    the token was stored an the expire integer provided by the server.
    *    Before checking the expire time the functions checks if the
    *    members are empty, so this function can also be used to check if
    *    an object contains a token.
    *
    * \return true if the token is still valid, false otherwise.
    */
   bool stillValid(void);
};

} // namepsace sota_server

#endif // OAUTHTOKEN_H_
