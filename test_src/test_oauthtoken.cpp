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
 
#include "oauthtoken.hpp"
#include <stdlib.h>
#include <iostream>

#include "logger.hpp"

namespace sota_server {

/*****************************************************************************/
oauthToken::oauthToken(const std::string& token_in, const std::string& type_in,
                       const std::string& expire_in) {
  // get current time
  stored = time(0);
  // store token and token type
  token = token_in;
  type = type_in;
  expire = atoi(expire_in.c_str());
}

oauthToken::oauthToken(void) {
  stored = (time_t)0;
  token = "";
  type = "";
  expire = 0;
}

/*****************************************************************************/
bool oauthToken::stillValid(void) {
  return true;
}

/*****************************************************************************/
const std::string& oauthToken::get(void) { return token; }

} // namespace sota_server

/*****************************************************************************/

#define BOOST_TEST_MODULE server

#include <string>

#include "servercon.hpp"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(servercon)

BOOST_AUTO_TEST_CASE(servercon_oauthtokenValid) {
  sota_server::servercon obj1;
  
  // make an API request which will be executed
  // as the token is always valid.
  obj1.get_availableUpdates();
}

BOOST_AUTO_TEST_SUITE_END()


