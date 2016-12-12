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

#include <stdlib.h>

#include "logger.hpp"
#include "oauthtoken.hpp"

namespace sota_server {

/*****************************************************************************/
oauthToken::   oauthToken(const std::string& token_in, const std::string& type_in, const std::string& expire_in)
{
   // get current time
   stored = time(0);

   // store token and token type
   token = token_in;
   type = type_in;

   // store time
   // atoi is used here instead of stoi() which was introduced with C++11
   // to retain backwards compatibility to older C++ standards
   expire = atoi(expire_in.c_str());
}

/*****************************************************************************/
bool oauthToken::stillValid(void)
{
   bool returnValue;

   returnValue = false;

   returnValue = token.empty();
   returnValue &= type.empty();

   if (!returnValue)
   {
      time_t now = time(0);
      LOGGER_LOG(LVL_trace, stored << "-" << (stored + (time_t)expire));
      if( now < (stored + (time_t)expire))
      {
         returnValue = true;
      }
   }
   else
   {
      LOGGER_LOG(LVL_trace, "oauthtoken - called stillValid() for empty token");
   }

   return returnValue;
}


/*****************************************************************************/
std::string oauthToken::get(void)
{
   return token;
}

} // namespace sota_server
