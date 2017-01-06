// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSMESSAGE_HPP_
#define COMMONAPI_DBUS_DBUSMESSAGE_HPP_

#include <string>

#include <dbus/dbus.h>

#ifdef WIN32
#include <stdint.h>
#endif

#include <CommonAPI/Export.hpp>

namespace CommonAPI {
namespace DBus {

class DBusAddress;
class DBusConnection;

class COMMONAPI_EXPORT DBusMessage {
public:
    enum class Type: int {
        Invalid = DBUS_MESSAGE_TYPE_INVALID,
        MethodCall = DBUS_MESSAGE_TYPE_METHOD_CALL,
        MethodReturn = DBUS_MESSAGE_TYPE_METHOD_RETURN,
        Error = DBUS_MESSAGE_TYPE_ERROR,
        Signal = DBUS_MESSAGE_TYPE_SIGNAL
    };

    DBusMessage();
    DBusMessage(::DBusMessage* libdbusMessage);
    DBusMessage(::DBusMessage* libdbusMessage, bool _reference);
    DBusMessage(const DBusMessage &_source);
    DBusMessage(DBusMessage &&_source);

    ~DBusMessage();

    DBusMessage &operator=(const DBusMessage &_source);
    DBusMessage &operator=(DBusMessage &&_source);
    operator bool() const;

    static DBusMessage createOrgFreedesktopOrgMethodCall(const std::string &_method,
                                                         const std::string &_signature = "");

    static DBusMessage createMethodCall(const DBusAddress &_address,
                                        const std::string &_method, const std::string &_signature = "");

    DBusMessage createMethodReturn(const std::string &_signature) const;

    DBusMessage createMethodError(const std::string &_name, const std::string &_reason = "") const;

    static DBusMessage createSignal(const std::string& objectPath,
                                    const std::string& interfaceName,
                                    const std::string& signalName,
                                    const std::string& signature = "");

    const char* getSender() const;
    const char* getObjectPath() const;
    const char* getInterface() const;
    const char* getMember() const;
    const char* getSignature() const;
    const char* getError() const;
    const char* getDestination() const;
    uint32_t getSerial() const;

    bool hasObjectPath(const std::string& objectPath) const;

    bool hasObjectPath(const char* objectPath) const;
    bool hasInterfaceName(const char* interfaceName) const;
    bool hasMemberName(const char* memberName) const;
    bool hasSignature(const char* signature) const;

    Type getType() const;
    bool isInvalidType() const;
    bool isMethodCallType() const;
    bool isMethodReturnType() const;
    bool isErrorType() const;
    bool isSignalType() const;

    char* getBodyData() const;
    int getBodyLength() const;
    int getBodySize() const;

    bool setBodyLength(const int bodyLength);
    bool setDestination(const char* destination);

private:
    ::DBusMessage *message_;

    friend class DBusConnection;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSMESSAGE_HPP_
