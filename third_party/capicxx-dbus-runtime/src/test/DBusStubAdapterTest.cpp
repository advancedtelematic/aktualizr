// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusFunctionalHash.hpp>
#include <CommonAPI/DBus/DBusStubAdapterHelper.hpp>

#include <cassert>
#include <iostream>
#include <tuple>

static uint32_t dispatchedMessageCount;

class TestStubRemoteEventHandler {
 public:
    virtual ~TestStubRemoteEventHandler() { }

    virtual bool onRemoteSetTestAttribute(int32_t testValue) = 0;

    virtual void onRemoteTestAttributeChanged() = 0;
};

class TestStubAdapter: virtual public CommonAPI::StubAdapter {
 public:
    virtual void fireTestAttributeChanged(const int32_t& testValue)  = 0;
};

class TestStub: public CommonAPI::Stub<TestStubAdapter, TestStubRemoteEventHandler> {
 public:
    TestStub(): remoteEventHandler_(this) {
    }

    virtual TestStubRemoteEventHandler* initStubAdapter(const std::shared_ptr<TestStubAdapter>& stubAdapter) {
        return &remoteEventHandler_;
    }

    void getEmptyResponse() {
        std::cout << "onGetEmptyResponse()\n";
        dispatchedMessageCount++;
    }

    void getDeepCopy(int32_t inValue, int32_t& outValue) {
        outValue = inValue;
        std::cout << "getDeepCopy(inValue=" << inValue << ", outValue=" << outValue << ")\n";
        dispatchedMessageCount++;
    }

    void getDeepCopies(std::vector<int32_t> inValues, std::vector<int32_t>& outValues) {
        outValues = inValues;
        std::cout << "getDeepCopies(inValues=" << inValues.size() << ", outValues=" << outValues.size() << ")\n";
        dispatchedMessageCount++;
    }

    void getShallowCopy(int32_t inValue, int32_t& outValue) {
        outValue = inValue;
        std::cout << "getShallowCopy(inValue=" << inValue << ", outValue=" << outValue << ")\n";
        dispatchedMessageCount++;
    }

    void getShallowCopies(std::vector<int32_t> inValues, std::vector<int32_t>& outValues) {
        outValues = inValues;
        std::cout << "getShallowCopies(inValues=" << inValues.size() << ", outValues=" << outValues.size() << ")\n";
        dispatchedMessageCount++;
    }

    const int32_t& getTestAttribute() {
        return testAttributeValue_;
    }

 private:
    class RemoteEventHandler: public TestStubRemoteEventHandler {
     public:
        RemoteEventHandler(TestStub* stub): stub_(stub) {
        }

        virtual bool onRemoteSetTestAttribute(int32_t testValue) {
            std::cout << "RemoteEventHandler::onRemoteSetTestAttribute(" << testValue << "): oldValue = " << stub_->testAttributeValue_ << "\n";
            const bool valueChanged = (stub_->testAttributeValue_ != testValue);
            stub_->testAttributeValue_ = testValue;
            return valueChanged;
        }

        virtual void onRemoteTestAttributeChanged() {
            std::cout << "RemoteEventHandler::onRemoteTestAttributeChanged()\n";
        }

     private:
        TestStub* stub_;
    };

    RemoteEventHandler remoteEventHandler_;
    int32_t testAttributeValue_;
};

typedef CommonAPI::DBus::DBusStubAdapterHelper<TestStub> TestStubAdapterHelper;

class TestDBusStubAdapter: public TestStubAdapter,  public TestStubAdapterHelper {
 public:
    TestDBusStubAdapter(const std::string& commonApiAddress,
                        const std::string& dbusBusName,
                        const std::string& dbusObjectPath,
                        const std::shared_ptr<CommonAPI::DBus::DBusConnection>& dbusConnection,
                        const std::shared_ptr<TestStub>& testStub) :
                    TestStubAdapterHelper(
                                    CommonAPI::DBus::DBusAddress(dbusBusName, dbusObjectPath, "org.genivi.CommonAPI.DBus.TestInterface"),
                                    dbusConnection,
                                    testStub,
                                    false) {
    }

    virtual void fireTestAttributeChanged(const int32_t& testValue) {
        std::cout << "TestDBusStubAdapter::fireTestAttributeChanged(" << testValue << ")\n";
    }

 protected:
    virtual const char* getMethodsDBusIntrospectionXmlData() const;
};

const char* TestDBusStubAdapter::getMethodsDBusIntrospectionXmlData() const {
    return "<method name=\"GetEmptyResponse\">\n"
           "</method>\n"
           "<method name=\"GetDeepCopy\">\n"
                    "<arg type=\"i\" name=\"int32InValue\" direction=\"in\"/>\n"
                    "<arg type=\"i\" name=\"int32OutValue\" direction=\"out\"/>\n"
           "</method>\n"
           "<method name=\"GetDeepCopies\">\n"
                    "<arg type=\"ai\" name=\"int32InValues\" direction=\"in\"/>\n"
                    "<arg type=\"ai\" name=\"int32OutValues\" direction=\"out\"/>\n"
           "</method>\n"
           "<method name=\"GetShallowCopy\">\n"
                    "<arg type=\"i\" name=\"int32InValue\" direction=\"in\"/>\n"
                    "<arg type=\"i\" name=\"int32OutValue\" direction=\"out\"/>\n"
           "</method>\n"
           "<method name=\"GetShallowCopies\">\n"
                    "<arg type=\"ai\" name=\"int32InValues\" direction=\"in\"/>\n"
                    "<arg type=\"ai\" name=\"int32OutValues\" direction=\"out\"/>\n"
           "</method>"
           "<method name=\"SetTestAttribute\">\n"
                    "<arg type=\"i\" name=\"value\" direction=\"in\"/>\n"
                    "<arg type=\"i\" name=\"value\" direction=\"out\"/>\n"
           "</method>"
           ;
}

namespace {
static CommonAPI::DBus::DBusMethodWithReplyStubDispatcher<
                    TestStub,
                    std::tuple<>,
                    std::tuple<> > getEmptyResponseStubDispatcher(&TestStub::getEmptyResponse, "");
static CommonAPI::DBus::DBusMethodWithReplyStubDispatcher<
                    TestStub,
                    std::tuple<int32_t>,
                    std::tuple<int32_t> > getDeepCopyStubDispatcher(&TestStub::getDeepCopy, "i");
static CommonAPI::DBus::DBusMethodWithReplyStubDispatcher<
                    TestStub,
                    std::tuple<std::vector<int32_t>>,
                    std::tuple<std::vector<int32_t>> > getDeepCopiesStubDispatcher(&TestStub::getDeepCopies, "ai");
static CommonAPI::DBus::DBusMethodWithReplyStubDispatcher<
                    TestStub,
                    std::tuple<int32_t>,
                    std::tuple<int32_t> > getShallowCopyStubDispatcher(&TestStub::getShallowCopy, "i");
static CommonAPI::DBus::DBusMethodWithReplyStubDispatcher<
                    TestStub,
                    std::tuple<std::vector<int32_t>>,
                    std::tuple<std::vector<int32_t>> > getShallowCopiesStubDispatcher(&TestStub::getShallowCopies, "ai");
static CommonAPI::DBus::DBusSetObservableAttributeStubDispatcher<TestStub, int32_t> setTestAttributeStubDispatcher(
                                    &TestStub::getTestAttribute,
                                    &TestStubRemoteEventHandler::onRemoteSetTestAttribute,
                                    &TestStubRemoteEventHandler::onRemoteTestAttributeChanged,
                                    &TestStubAdapter::fireTestAttributeChanged,
                                    "i");
}

template<>
const TestStubAdapterHelper::StubDispatcherTable TestStubAdapterHelper::stubDispatcherTable_ = {
     { {"GetEmptyResponse", ""}, &getEmptyResponseStubDispatcher },
     { {"GetDeepCopy", "i"}, &getDeepCopyStubDispatcher },
     { {"GetDeepCopies", "ai"}, &getDeepCopiesStubDispatcher },
     { {"GetShallowCopy", "i"}, &getShallowCopyStubDispatcher },
     { {"GetShallowCopies", "ai"}, &getShallowCopiesStubDispatcher },
     { {"SetTestAttribute", "i"}, &setTestAttributeStubDispatcher }
};

int main(void) {
    auto dbusConnection = CommonAPI::DBus::DBusConnection::getBus(CommonAPI::DBus::DBusType_t::SESSION);

    if (!dbusConnection->isConnected())
        dbusConnection->connect();

    assert(dbusConnection->isConnected());

    const bool serviceNameAcquired = dbusConnection->requestServiceNameAndBlock(
                    "org.genivi.CommonAPI.DBus.TestDBusInterfaceAdapter");
    assert(serviceNameAcquired);

    auto testStub = std::make_shared<TestStub>();
    auto testStubAdapter = std::make_shared<TestDBusStubAdapter>(
                    "my:common.api:address.for.dbus",
                    "org.genivi.CommonAPI.DBus.TestDBusInterfaceAdapter",
                    "/common/api/dbus/TestDBusInterfaceAdapter",
                    dbusConnection,
                    testStub);
    testStubAdapter->init(testStubAdapter);

    auto dbusMessageCall = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress("org.genivi.CommonAPI.DBus.TestDBusInterfaceAdapter", testStubAdapter->getDBusAddress().getObjectPath().c_str(), testStubAdapter->getDBusAddress().getService().c_str()), "GetEmptyResponse");

    const bool messageSent = dbusConnection->sendDBusMessage(dbusMessageCall);
    assert(messageSent);

    for (int i = 0; i < 10; i++) {
        dbusConnection->readWriteDispatch(100);
    }

    assert(dispatchedMessageCount > 0);
    testStubAdapter->deinit();

    return 0;
}
