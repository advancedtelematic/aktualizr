// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __DBUS_TEST_UTILS__
#define __DBUS_TEST_UTILS__

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <dbus/dbus.h>
#include <CommonAPI/DBus/DBusServiceRegistry.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>

inline char eliminateZeroes(char val) {
    return !val ? '0' : val;
}

inline void printLibdbusMessageBody(char* data, uint32_t fromByteIndex, uint32_t toByteIndex) {
    for (unsigned int i = fromByteIndex; i < toByteIndex; i++) {
        std::cout << eliminateZeroes(data[i]);
        if (i % 8 == 7) {
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
}

inline void printLibdbusMessage(DBusMessage* libdbusMessage, uint32_t fromByteIndex, uint32_t toByteIndex) {
    char* data = dbus_message_get_body(libdbusMessage);
    printLibdbusMessageBody(data, fromByteIndex, toByteIndex);
}

inline void printLibdbusMessage(DBusMessage* libdbusMessage) {
    printLibdbusMessage(libdbusMessage, 0, dbus_message_get_body_length(libdbusMessage));
}


inline void printDBusMessage(CommonAPI::DBus::DBusMessage& dbusMessage) {
    printLibdbusMessageBody(dbusMessage.getBodyData(), 0, dbusMessage.getBodyLength());
}

inline std::string toString(CommonAPI::DBus::DBusServiceRegistry::DBusRecordState dbusRecordState) {
    switch(dbusRecordState) {
        case CommonAPI::DBus::DBusServiceRegistry::DBusRecordState::AVAILABLE:
            return "AVAILABLE";
        case CommonAPI::DBus::DBusServiceRegistry::DBusRecordState::NOT_AVAILABLE:
            return "NOT_AVAILABLE";
        case CommonAPI::DBus::DBusServiceRegistry::DBusRecordState::RESOLVED:
            return "RESOLVED";
        case CommonAPI::DBus::DBusServiceRegistry::DBusRecordState::RESOLVING:
            return "RESOLVING";
        case CommonAPI::DBus::DBusServiceRegistry::DBusRecordState::UNKNOWN:
            return "UNKNOWN";
    }
    return "";
}

inline std::string toString(CommonAPI::AvailabilityStatus state) {
    switch(state) {
        case CommonAPI::AvailabilityStatus::AVAILABLE:
            return "AVAILABLE";
        case CommonAPI::AvailabilityStatus::NOT_AVAILABLE:
            return "NOT_AVAILABLE";
        case CommonAPI::AvailabilityStatus::UNKNOWN:
            return "UNKNOWN";
    }
    return "";
}

inline std::string toString(CommonAPI::CallStatus state) {
    switch(state) {
        case CommonAPI::CallStatus::CONNECTION_FAILED:
            return "CONNECTION_FAILED";
        case CommonAPI::CallStatus::NOT_AVAILABLE:
            return "NOT_AVAILABLE";
        case CommonAPI::CallStatus::OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
        case CommonAPI::CallStatus::REMOTE_ERROR:
            return "REMOTE_ERROR";
        case CommonAPI::CallStatus::SUCCESS:
            return "SUCCESS";
        case CommonAPI::CallStatus::UNKNOWN:
            return "UNKNOWN";
    default:
        return "";
    }
    return "";
}
#endif //__DBUS_TEST_UTILS__
