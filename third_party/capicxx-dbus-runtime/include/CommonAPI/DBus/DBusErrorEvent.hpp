// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSERROREVENT_HPP_
#define COMMONAPI_DBUS_DBUSERROREVENT_HPP_

#include <CommonAPI/Event.hpp>

#include <CommonAPI/DBus/DBusInputStream.hpp>

namespace CommonAPI {
namespace DBus {

template <class...>
class DBusErrorEvent;

template<>
class DBusErrorEvent<> : public Event<std::string> {
public:
    DBusErrorEvent(const std::string &_errorName) :
        errorName_(_errorName) {}

    DBusErrorEvent(const DBusErrorEvent &_source) :
        errorName_(_source.errorName_) {}

    virtual ~DBusErrorEvent() {}

    inline const std::string & getErrorName() const {
        return errorName_;
    }

    void notifyErrorEventListeners(const DBusMessage &_reply) {
        (void)_reply;
        this->notifyListeners(errorName_);
    }

private:
    std::string errorName_;
};

template <
    template <class...> class In_, class... InArgs_,
    template <class...> class DeplIn_, class... DeplIn_Args>
class DBusErrorEvent<
        In_<InArgs_...>,
        DeplIn_<DeplIn_Args...>> : public Event<std::string, InArgs_...> {
public:
    DBusErrorEvent(const std::string &_errorName,
                   const std::tuple<DeplIn_Args*...> &_in) :
        errorName_(_errorName) {
        initialize(typename make_sequence_range<sizeof...(DeplIn_Args), 0>::type(), _in);
    }

    DBusErrorEvent(const DBusErrorEvent &_source) :
        errorName_(_source.errorName_),
        in_(_source.in_) {}

    virtual ~DBusErrorEvent() {}

    inline const std::string & getErrorName() const {
        return errorName_;
    }

    void notifyErrorEventListeners(const DBusMessage &_reply) {
        deserialize(_reply, typename make_sequence_range<sizeof...(InArgs_), 0>::type());
    }

private:

    template <int... DeplIn_ArgIndices>
    inline void initialize(index_sequence<DeplIn_ArgIndices...>, const std::tuple<DeplIn_Args*...> &_in) {
        in_ = std::make_tuple(std::get<DeplIn_ArgIndices>(_in)...);
    }

    template <int... InArgIndices_>
    void deserialize(const DBusMessage &_reply,
                     index_sequence<InArgIndices_...>) {
        if (sizeof...(InArgs_) > 0) {
            DBusInputStream dbusInputStream(_reply);
            const bool success = DBusSerializableArguments<CommonAPI::Deployable<InArgs_, DeplIn_Args>...>::deserialize(dbusInputStream, std::get<InArgIndices_>(in_)...);
            if (!success)
                return;
            this->notifyListeners(errorName_, std::move(std::get<InArgIndices_>(in_).getValue())...);
        }
    }

    std::string errorName_;
    std::tuple<CommonAPI::Deployable<InArgs_, DeplIn_Args>...> in_;
};

class DBusErrorEventHelper {
public:
    template <int... ErrorEventsIndices_, class... ErrorEvents_>
    static void notifyListeners(const DBusMessage &_reply,
                                const std::string &_errorName,
                                index_sequence<ErrorEventsIndices_...>,
                                const std::tuple<ErrorEvents_*...> &_errorEvents) {

        notifyListeners(_reply, _errorName, std::get<ErrorEventsIndices_>(_errorEvents)...);
    }

    template <class ErrorEvent_>
    static void notifyListeners(const DBusMessage &_reply,
                                const std::string &_errorName,
                                ErrorEvent_ *_errorEvent) {
        if(_errorEvent->getErrorName() == _errorName)
            _errorEvent->notifyErrorEventListeners(_reply);

    }

    template <class ErrorEvent_, class... Rest_>
    static void notifyListeners(const DBusMessage &_reply,
                                const std::string &_errorName,
                                ErrorEvent_ *_errorEvent,
                                Rest_&... _rest) {
        if(_errorEvent->getErrorName() == _errorName)
            _errorEvent->notifyErrorEventListeners(_reply);
        notifyListeners(_reply, _errorName, _rest...);
    }
};

} // namespace DBus
} // namespace CommonAPI

#endif /* COMMONAPI_DBUS_DBUSERROREVENT_HPP_ */
