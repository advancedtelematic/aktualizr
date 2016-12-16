/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file test_oauthtoken.cpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  Testcases for the servercon class and its member
 *         oauthtoken. Oauthtoken is reimplemented/stubbed here. It is always
 *         valid.
 *
 *
 ******************************************************************************
 *
 * \endcond
 */
 

#include <stdlib.h>
#include <iostream>

#include "oauthtoken.h"
#include "logger.h"

namespace sota_server {

/*****************************************************************************/
OAuthToken::OAuthToken(const std::string& token_in, const std::string& type_in,
                       const std::string& expire_in) {
  // get current time
  stored = time(0);
  // store token and token type
  token = token_in;
  type = type_in;
  expire = atoi(expire_in.c_str());
}

OAuthToken::OAuthToken(void) {
  stored = (time_t)0;
  token = "";
  type = "";
  expire = 0;
}

/*****************************************************************************/
bool OAuthToken::stillValid(void) {
  return true;
}

/*****************************************************************************/
const std::string& OAuthToken::get(void) { return token; }

} // namespace sota_server

/*****************************************************************************/

#define BOOST_TEST_MODULE server

#include <string>
#include <boost/test/unit_test.hpp>

#include "servercon.h"

BOOST_AUTO_TEST_SUITE(servercon)

BOOST_AUTO_TEST_CASE(servercon_oauthtokenValid) {
  sota_server::ServerCon obj1;
  
  // make an API request which will be executed
  // as the token is always valid.
  obj1.getAvailableUpdates();
}

BOOST_AUTO_TEST_SUITE_END()


