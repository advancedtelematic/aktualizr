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
#include "servercon.hpp"

#include <stdlib.h>
#include <iostream>

#include "logger.hpp"

namespace sota_server {

/*****************************************************************************/
// LCOV_EXCL_START
oauthToken::oauthToken(const std::string& token_in, const std::string& type_in,
                       const std::string& expire_in) {
  // get current time
  stored = time(0);
  // store token and token type
  token = token_in;
  type = type_in;
  expire = atoi(expire_in.c_str());
}
// LCOV_EXCL_STOP

oauthToken::oauthToken(void) {
  stored = (time_t)0;
  token = "";
  type = "";
  expire = 0;
}

#if (TEST_ENABLE == 1)
void servercon::tst_setUpdate(const server_updateInfo_t& update_info) {
  update = update_info;
}
#endif

/*****************************************************************************/
bool oauthToken::stillValid(void) { return true; }

/*****************************************************************************/
const std::string& oauthToken::get(void) { return token; }

}  // namespace sota_server

/*****************************************************************************/

#define BOOST_TEST_MODULE server

#include <string>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(servercon)

BOOST_AUTO_TEST_CASE(servercon_oauthtokenValid) {
  sota_server::servercon obj1;

  // make an API request which will be executed
  // as the token is always valid.
  unsigned int check = obj1.get_availableUpdates();
  if (check != 0u) {
    // LCOV_EXCL_START
    BOOST_FAIL(
        "servercon - get_availableUpdates() returned not fail using test "
        "data.");
    // LCOV_EXCL_STOP
  }
}

#if (TEST_ENABLE == 1)
BOOST_AUTO_TEST_CASE(servercon_getupdate) {
  sota_server::servercon obj1;
  sota_server::server_updateInfo_t update;
  update.UUID = "tstUUID";
  update.name = "tstupdate";
  update.version = "0.0";

  obj1.tst_setUpdate(update);
  unsigned int check = obj1.download_update();
  if (check != 0u) {
    // LCOV_EXCL_START
    BOOST_FAIL(
        "servercon - download_update() returned not fail using test data");
    // LCOV_EXCL_STOP
  }
}
#endif  // TEST_ENABLE == 1
BOOST_AUTO_TEST_SUITE_END()
