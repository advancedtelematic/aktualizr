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

#include "oauthtoken.h"
#include <stdlib.h>
#include <iostream>

#include "logger.h"

namespace sota_server {

/*****************************************************************************/
OAuthToken::OAuthToken(const std::string& token_in, const std::string& type_in,
                       const std::string& expire_in)
    : token(token_in), type(type_in) {
  // get current time
  stored = time(0);

  // store time
  // atoi is used here instead of stoi() which was introduced with C++11
  // to retain backwards compatibility to older C++ standards
  expire = atoi(expire_in.c_str());

  LOGGER_LOG(LVL_trace, "stored token at "
                            << (time_t)stored << " and expires at "
                            << (stored + (time_t)expire) << ""
                                                            " and time now is "
                            << time(0));
  std::cout << "stored token at " << stored << " and expires at "
            << (stored + (time_t)expire) << ""
                                            " and time now is "
            << time(0);
}

OAuthToken::OAuthToken(void) {
  stored = (time_t)0;
  token = "";
  type = "";
  expire = 0;
}

/*****************************************************************************/
bool OAuthToken::stillValid(void) {
  bool return_value;

  return_value = token.empty();
  return_value &= type.empty();

  if (!return_value) {
    time_t now = time(0);
    if (now < (stored + (time_t)expire)) {
      return_value = true;
    }
  } else {
    LOGGER_LOG(LVL_trace, "oauthtoken - called stillValid() for empty token");
    return_value = false;
  }

  return return_value;
}

/*****************************************************************************/
const std::string& OAuthToken::get(void) { return token; }

}  // namespace sota_server
