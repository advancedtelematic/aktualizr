/* 
 * Test suite for initialization and teardown of RVI context
 */

#include "rvi.h"

#include <check.h>

#include <errno.h>
#include <stdlib.h>

#define CONFFILE "conf.json"

START_TEST(test_rvi_init)
{
    TRviHandle handle;
    handle = rviInit(CONFFILE);

    ck_assert_msg(handle, "Failed to initialize.");

    int stat;
    stat = rviCleanup(handle);

    ck_assert_int_eq(stat, 0);
}
END_TEST

START_TEST(test_rvi_init_nofile)
{
    TRviHandle handle;
    handle = rviInit("nope");

    ck_assert_msg(!handle, "Got a pointer for a file that doesn't exist.");

    int stat;
    stat = rviCleanup(handle);

    ck_assert_int_eq(stat, EINVAL);
}
END_TEST

START_TEST(test_rvi_init_randfile)
{
    TRviHandle handle;
    handle = rviInit("/dev/urandom");

    ck_assert_msg(!handle, "Initialized with bad file");
}
END_TEST

Suite *init_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s= suite_create("Initialization");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test( tc_core, test_rvi_init );
    tcase_add_test(tc_core, test_rvi_init_nofile);
    tcase_add_test(tc_core, test_rvi_init_randfile);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = init_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return ( number_failed == 0 ) ? EXIT_SUCCESS : EXIT_FAILURE;
}
