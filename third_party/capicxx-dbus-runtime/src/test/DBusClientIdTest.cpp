// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include "CommonAPI/DBus/DBusClientId.hpp"
#include "CommonAPI/Types.hpp"

using namespace CommonAPI::DBus;

class DBusClientIdTest: public ::testing::Test {

public:
    void SetUp() {

    }

    void TearDown() {

    }
};

class TestClientId: public CommonAPI::ClientId {
public:
    bool operator==(CommonAPI::ClientId& clientIdToCompare) {
    (void)clientIdToCompare;
        return false; // doesn't matter, as we are just comparing this class with DBusClientId;
    }

    std::size_t hashCode() {
        return 0;
    }

};

TEST_F(DBusClientIdTest, TestClientIdImplementation) {
    DBusClientId dbusId1("test1");
    DBusClientId dbusId2("test2");
    TestClientId testId;


    ASSERT_TRUE(dbusId1 == dbusId1);
    ASSERT_FALSE(dbusId1 == dbusId2);
    ASSERT_FALSE(dbusId2 == dbusId1);

    ASSERT_FALSE(dbusId1 == testId);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
