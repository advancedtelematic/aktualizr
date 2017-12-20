#ifdef ECUFLASHER_EXAMPLE
#include "example_flasher.h"
#endif

#ifdef ECUFLASHER_TEST_ISOTP
#include "test_isotp_interface.h"
#endif

ECUInterface* make_ecu(unsigned int loglevel) {
#if defined(ECUFLASHER_EXAMPLE)
  return (new ExampleFlasher(loglevel));
#elif defined(ECUFLASHER_TEST_ISOTP)
  return (new TestIsotpInterface(loglevel));
#else
  return NULL;
#endif
}
