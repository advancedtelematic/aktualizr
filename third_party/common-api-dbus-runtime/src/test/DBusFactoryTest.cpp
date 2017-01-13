// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <utility>
#include <tuple>
#include <type_traits>

#include <CommonAPI/CommonAPI.hpp>

#define COMMONAPI_INTERNAL_COMPILATION

#include <CommonAPI/Utils.hpp>

#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusUtils.hpp>

#include "commonapi/tests/PredefinedTypeCollection.hpp"
#include "commonapi/tests/DerivedTypeCollection.hpp"
#include "v1/commonapi/tests/TestInterfaceProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceStubDefault.hpp"
#include "v1/commonapi/tests/TestInterfaceDBusStubAdapter.hpp"

#include "v1/commonapi/tests/TestInterfaceDBusProxy.hpp"

#define VERSION v1_0

static const char DBUS_CONFIG_SUFFIX[] = "_dbus.conf";

static const std::string fileString =
""
"[factory$session]\n"
"dbus_bustype=session\n"
"[factory$system]\n"
"dbus_bustype=system\n"
"";

class DBusProxyFactoryTest: public ::testing::Test {
 protected:
    virtual void SetUp() {
        runtime_ = CommonAPI::Runtime::get();
        ASSERT_TRUE((bool)runtime_);

#ifdef WIN32
        configFileName_ = _pgmptr;
#else
        char cCurrentPath[FILENAME_MAX];
        if(getcwd(cCurrentPath, sizeof(cCurrentPath)) == NULL) {
            std::perror("DBusProxyFactoryTest::SetUp");
        }
        configFileName_ = cCurrentPath;
#endif

        configFileName_ += DBUS_CONFIG_SUFFIX;
        std::ofstream configFile(configFileName_);
        ASSERT_TRUE(configFile.is_open());
        configFile << fileString;
        configFile.close();
    }

    virtual void TearDown() {
        usleep(30000);
        std::remove(configFileName_.c_str());
    }

    std::string configFileName_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;
};



namespace myExtensions {

template<typename _AttributeType>
class AttributeTestExtension: public CommonAPI::AttributeExtension<_AttributeType> {
    typedef CommonAPI::AttributeExtension<_AttributeType> __baseClass_t;

public:
    typedef typename _AttributeType::ValueType ValueType;
    typedef typename _AttributeType::AttributeAsyncCallback AttributeAsyncCallback;

    AttributeTestExtension(_AttributeType& baseAttribute) :
                    CommonAPI::AttributeExtension<_AttributeType>(baseAttribute) {}

   ~AttributeTestExtension() {}

   bool testExtensionMethod() const {
       return true;
   }
};

} // namespace myExtensions

//####################################################################################################################

TEST_F(DBusProxyFactoryTest, DBusFactoryCanBeCreated) {
}

TEST_F(DBusProxyFactoryTest, CreatesDefaultTestProxy) {
    auto defaultTestProxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>("local", "commonapi.tests.TestInterface");
    ASSERT_TRUE((bool)defaultTestProxy);
}

TEST_F(DBusProxyFactoryTest, CreatesDefaultExtendedTestProxy) {
    auto defaultTestProxy = runtime_->buildProxyWithDefaultAttributeExtension<VERSION::commonapi::tests::TestInterfaceProxy, myExtensions::AttributeTestExtension>("local", "commonapi.tests.TestInterface");
    ASSERT_TRUE((bool)defaultTestProxy);

    auto attributeExtension = defaultTestProxy->getTestDerivedArrayAttributeAttributeExtension();
    ASSERT_TRUE(attributeExtension.testExtensionMethod());
}

TEST_F(DBusProxyFactoryTest, CreatesIndividuallyExtendedTestProxy) {
    auto specificAttributeExtendedTestProxy = runtime_->buildProxy<
        VERSION::commonapi::tests::TestInterfaceProxy,
        VERSION::commonapi::tests::TestInterfaceExtensions::TestDerivedArrayAttributeAttributeExtension<myExtensions::AttributeTestExtension> >
            ("local", "commonapi.tests.TestInterface");

    ASSERT_TRUE((bool)specificAttributeExtendedTestProxy);

    auto attributeExtension = specificAttributeExtendedTestProxy->getTestDerivedArrayAttributeAttributeExtension();
    ASSERT_TRUE(attributeExtension.testExtensionMethod());
}

TEST_F(DBusProxyFactoryTest, CreateNamedFactory) {
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
