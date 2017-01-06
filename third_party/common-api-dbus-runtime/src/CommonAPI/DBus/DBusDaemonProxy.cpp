// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Address.hpp>
#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusDaemonProxy.hpp>
#include <CommonAPI/DBus/DBusProxyHelper.hpp>

namespace CommonAPI {
namespace DBus {

StaticInterfaceVersionAttribute::StaticInterfaceVersionAttribute(const uint32_t& majorValue, const uint32_t& minorValue):
                version_(majorValue, minorValue) {
}

void StaticInterfaceVersionAttribute::getValue(CallStatus &_status, Version &_version,
                                               const CommonAPI::CallInfo *_info) const {
    (void)_info;
    _version = version_;
    _status = CallStatus::SUCCESS;
}

std::future<CallStatus> StaticInterfaceVersionAttribute::getValueAsync(AttributeAsyncCallback attributeAsyncCallback,
                                                                       const CommonAPI::CallInfo *_info) {
    (void)_info;

    attributeAsyncCallback(CallStatus::SUCCESS, version_);

    std::promise<CallStatus> versionPromise;
    versionPromise.set_value(CallStatus::SUCCESS);

    return versionPromise.get_future();
}

//static const char *DAEMON_COMMONAPI_ADDRESS = "org.freedesktop.DBus-/org/freedesktop/DBus";
static const char *DAEMON_DBUS_INTERFACE = DBusDaemonProxy::getInterfaceId();
static const char *DAEMON_DBUS_OBJECT_PATH = "/org/freedesktop/DBus";
static const char *DAEMON_DBUS_BUS = "org.freedesktop.DBus";
static DBusAddress dbusProxyAddress(DAEMON_DBUS_INTERFACE, DAEMON_DBUS_OBJECT_PATH, DAEMON_DBUS_BUS);
static CommonAPI::CallInfo daemonProxyInfo(30000);

DBusDaemonProxy::DBusDaemonProxy(const std::shared_ptr<DBusProxyConnection>& dbusConnection):
                DBusProxyBase(dbusProxyAddress, dbusConnection),
                nameOwnerChangedEvent_(*this,
                                       "NameOwnerChanged", "sss",
                                       std::tuple<std::string, std::string, std::string>()),
                interfaceVersionAttribute_(1, 0) {
}

void DBusDaemonProxy::init() {
}

bool DBusDaemonProxy::isAvailable() const {
    return getDBusConnection()->isConnected();
}

bool DBusDaemonProxy::isAvailableBlocking() const {
    return isAvailable();
}

ProxyStatusEvent& DBusDaemonProxy::getProxyStatusEvent() {
    return getDBusConnection()->getConnectionStatusEvent();
}

InterfaceVersionAttribute& DBusDaemonProxy::getInterfaceVersionAttribute() {
    return interfaceVersionAttribute_;
}

DBusDaemonProxy::NameOwnerChangedEvent& DBusDaemonProxy::getNameOwnerChangedEvent() {
    return nameOwnerChangedEvent_;
}

void DBusDaemonProxy::listNames(CommonAPI::CallStatus& callStatus, std::vector<std::string>& busNames) const {
    DBusMessage dbusMethodCall = createMethodCall("ListNames", "");

    DBusError dbusError;
    DBusMessage dbusMessageReply
        = getDBusConnection()->sendDBusMessageWithReplyAndBlock(dbusMethodCall, dbusError, &daemonProxyInfo);

    if (dbusError || !dbusMessageReply.isMethodReturnType()) {
        callStatus = CallStatus::REMOTE_ERROR;
        return;
    }

    DBusInputStream inputStream(dbusMessageReply);
    const bool success = DBusSerializableArguments<std::vector<std::string>>::deserialize(inputStream, busNames);
    if (!success) {
        callStatus = CallStatus::REMOTE_ERROR;
        return;
    }

    callStatus = CallStatus::SUCCESS;
}

std::future<CallStatus> DBusDaemonProxy::listNamesAsync(ListNamesAsyncCallback listNamesAsyncCallback) const {
    DBusMessage dbusMessage = createMethodCall("ListNames", "");
    return getDBusConnection()->sendDBusMessageWithReplyAsync(
                    dbusMessage,
                    DBusProxyAsyncCallbackHandler<std::vector<std::string>>::create(listNamesAsyncCallback, std::tuple<std::vector<std::string>>()),
                    &daemonProxyInfo);
}

void DBusDaemonProxy::nameHasOwner(const std::string& busName, CommonAPI::CallStatus& callStatus, bool& hasOwner) const {
    DBusMessage dbusMethodCall = createMethodCall("NameHasOwner", "s");

    DBusOutputStream outputStream(dbusMethodCall);
    bool success = DBusSerializableArguments<std::string>::serialize(outputStream, busName);
    if (!success) {
        callStatus = CallStatus::OUT_OF_MEMORY;
        return;
    }
    outputStream.flush();

    DBusError dbusError;
    DBusMessage dbusMessageReply = getDBusConnection()->sendDBusMessageWithReplyAndBlock(
                    dbusMethodCall,
                    dbusError,
                    &daemonProxyInfo);
    if (dbusError || !dbusMessageReply.isMethodReturnType()) {
        callStatus = CallStatus::REMOTE_ERROR;
        return;
    }

    DBusInputStream inputStream(dbusMessageReply);
    success = DBusSerializableArguments<bool>::deserialize(inputStream, hasOwner);
    if (!success) {
        callStatus = CallStatus::REMOTE_ERROR;
        return;
    }
    callStatus = CallStatus::SUCCESS;
}

std::future<CallStatus> DBusDaemonProxy::nameHasOwnerAsync(const std::string& busName, NameHasOwnerAsyncCallback nameHasOwnerAsyncCallback) const {
    DBusMessage dbusMessage = createMethodCall("NameHasOwner", "s");

    DBusOutputStream outputStream(dbusMessage);
    const bool success = DBusSerializableArguments<std::string>::serialize(outputStream, busName);
    if (!success) {
        std::promise<CallStatus> promise;
        promise.set_value(CallStatus::OUT_OF_MEMORY);
        return promise.get_future();
    }
    outputStream.flush();

    return getDBusConnection()->sendDBusMessageWithReplyAsync(
                    dbusMessage,
                    DBusProxyAsyncCallbackHandler<bool>::create(nameHasOwnerAsyncCallback, std::tuple<bool>()),
                    &daemonProxyInfo);
}

std::future<CallStatus> DBusDaemonProxy::getManagedObjectsAsync(const std::string& forDBusServiceName, GetManagedObjectsAsyncCallback callback) const {
    static DBusAddress address(forDBusServiceName, "/", "org.freedesktop.DBus.ObjectManager");
    auto dbusMethodCallMessage = DBusMessage::createMethodCall(address, "GetManagedObjects", "");

    return getDBusConnection()->sendDBusMessageWithReplyAsync(
                    dbusMethodCallMessage,
                    DBusProxyAsyncCallbackHandler<DBusObjectToInterfaceDict>::create(
                        callback, std::tuple<DBusObjectToInterfaceDict>()
                    ),
                    &daemonProxyInfo);
}

std::future<CallStatus> DBusDaemonProxy::getNameOwnerAsync(const std::string& busName, GetNameOwnerAsyncCallback getNameOwnerAsyncCallback) const {
    DBusMessage dbusMessage = createMethodCall("GetNameOwner", "s");

    DBusOutputStream outputStream(dbusMessage);
    const bool success = DBusSerializableArguments<std::string>::serialize(outputStream, busName);
    if (!success) {
        std::promise<CallStatus> promise;
        promise.set_value(CallStatus::OUT_OF_MEMORY);
        return promise.get_future();
    }
    outputStream.flush();

    return getDBusConnection()->sendDBusMessageWithReplyAsync(
                    dbusMessage,
                    DBusProxyAsyncCallbackHandler<std::string>::create(getNameOwnerAsyncCallback, std::tuple<std::string>()),
                    &daemonProxyInfo);
}

const char* DBusDaemonProxy::getInterfaceId() {
    static const char interfaceId[] = "org.freedesktop.DBus";
    return interfaceId;
}

} // namespace DBus
} // namespace CommonAPI
