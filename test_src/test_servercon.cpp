#define BOOST_TEST_MODULE server

#include <string>

#include "servercon.hpp"

#include <boost/test/unit_test.hpp>

const std::string TSTURL = "https://t.est/url";
const std::string TSTID = "testid";
const std::string TSTSECRET = "testsecret";

const std::string TSTTOKEN = "testtoken345";
const std::string TSTTYPE = "bearer";
const std::string TSTEXPIRE = "1";

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE( servercon )

BOOST_AUTO_TEST_CASE( servercon_constructor )
{
   sota_server::servercon obj1, obj2;

   if (sizeof(obj1) != sizeof(obj2))
   {
      BOOST_FAIL("servercon constructor leads to different sizes.");
   }
}

/*****************************************************************************/

BOOST_AUTO_TEST_CASE( servercon_setmemb )
/* Compare with void free_test_function() */
{
   sota_server::servercon obj1;

   obj1.setServer( const_cast<std::string&>(TSTURL) );
   obj1.setClientID( const_cast<std::string&>(TSTID) );
   obj1.setClientSecret( const_cast<std::string&>(TSTSECRET) );

   if(obj1.get_oauthToken() != 0u)
   {
      BOOST_FAIL("servercon:get_oauthToken returns  success when usin invalid data.");
   }
}

BOOST_AUTO_TEST_SUITE_END()

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE( oauthtoken )


BOOST_AUTO_TEST_CASE( oauthtoken_constructor )
{
   sota_server::oauthToken obj1("a", "b", "1"), obj2("a", "b", "1");

   if (sizeof(obj1) != sizeof(obj2))
   {
      BOOST_FAIL("oauthToken constructor leads to different sizes.");
   }

}

BOOST_AUTO_TEST_CASE( oauthtoken_setmemb )
{
   sota_server::oauthToken obj1( const_cast<std::string&>(TSTTOKEN),
         const_cast<std::string&>(TSTTYPE),
         const_cast<std::string&>(TSTEXPIRE) );

   if(obj1.stillValid() == false)
   {
      BOOST_FAIL("oauthtoken - stillValid() returns false with expire still in time.");
   }

   sleep(2);

   if(obj1.stillValid() == true)
   {
      BOOST_FAIL("oauthtoken - stillValid() returns true with expire out of time.");
   }

   if( TSTTOKEN.compare(obj1.get())  != 0)
   {
      BOOST_FAIL("oauthtoken - get() returns wrong token.");
   }
}

BOOST_AUTO_TEST_SUITE_END()
