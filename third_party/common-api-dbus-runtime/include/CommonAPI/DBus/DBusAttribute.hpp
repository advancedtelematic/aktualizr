// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_ATTRIBUTE_HPP_
#define COMMONAPI_DBUS_DBUS_ATTRIBUTE_HPP_

#include <cassert>
#include <cstdint>
#include <tuple>

#include <CommonAPI/DBus/DBusConfig.hpp>
#include <CommonAPI/DBus/DBusEvent.hpp>
#include <CommonAPI/DBus/DBusProxyHelper.hpp>

namespace CommonAPI {
namespace DBus {

template <typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusReadonlyAttribute: public AttributeType_ {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef AttributeDepl_ ValueTypeDepl;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;

    DBusReadonlyAttribute(DBusProxy &_proxy,
                          const char *setMethodSignature, const char *getMethodName,
                          AttributeDepl_ *_depl = nullptr)
        : proxy_(_proxy),
          getMethodName_(getMethodName),
          setMethodSignature_(setMethodSignature),
          depl_(_depl)    {
        assert(getMethodName);
    }

    void getValue(CommonAPI::CallStatus &_status, ValueType &_value, const CommonAPI::CallInfo *_info) const {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedValue(depl_);
        DBusProxyHelper<
            DBusSerializableArguments<
            >,
            DBusSerializableArguments<
                CommonAPI::Deployable<
                    ValueType,
                    AttributeDepl_
                >
            >
        >::callMethodWithReply(proxy_, getMethodName_, "", (_info ? _info : &defaultCallInfo), _status, deployedValue);
        _value = deployedValue.getValue();
    }

    std::future<CallStatus> getValueAsync(AttributeAsyncCallback _callback, const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedValue(depl_);
        return DBusProxyHelper<
                    DBusSerializableArguments<>,
                    DBusSerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>
               >::callMethodAsync(proxy_, getMethodName_, "", (_info ? _info : &defaultCallInfo),
                    [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, AttributeDepl_> _response) {
                        _callback(_status, _response.getValue());
                    },
                    std::make_tuple(deployedValue));
    }

 protected:
    DBusProxy &proxy_;
    const char *getMethodName_;
    const char *setMethodSignature_;
    AttributeDepl_ *depl_;
};

template <typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusAttribute: public DBusReadonlyAttribute<AttributeType_, AttributeDepl_> {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;

    DBusAttribute(DBusProxy &_proxy,
                  const char *_setMethodName, const char *_setMethodSignature, const char *_getMethodName,
                  AttributeDepl_ *_depl = nullptr)
        : DBusReadonlyAttribute<AttributeType_, AttributeDepl_>(_proxy, _setMethodSignature, _getMethodName, _depl),
            setMethodName_(_setMethodName),
            setMethodSignature_(_setMethodSignature) {
        assert(_setMethodName);
        assert(_setMethodSignature);
    }

    void setValue(const ValueType &_request, CommonAPI::CallStatus &_status, ValueType &_response, const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedRequest(_request, this->depl_);
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedResponse(this->depl_);
        DBusProxyHelper<DBusSerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>,
                        DBusSerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>> >::callMethodWithReply(
                                this->proxy_,
                                setMethodName_,
                                setMethodSignature_,
                                (_info ? _info : &defaultCallInfo),
                                deployedRequest,
                                _status,
                                deployedResponse);
        _response = deployedResponse.getValue();
    }


    std::future<CallStatus> setValueAsync(const ValueType &_request, AttributeAsyncCallback _callback, const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedRequest(_request, this->depl_);
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedResponse(this->depl_);
        return DBusProxyHelper<DBusSerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>,
                               DBusSerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>> >::callMethodAsync(
                                       this->proxy_,
                                       setMethodName_,
                                       setMethodSignature_,
                                       (_info ? _info : &defaultCallInfo),
                                       deployedRequest,
                                       [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, AttributeDepl_> _response) {
                                            _callback(_status, _response.getValue());
                                       },
                                       std::make_tuple(deployedResponse));
    }

 protected:
    const char* setMethodName_;
    const char* setMethodSignature_;
};

template <typename AttributeType_>
class DBusObservableAttribute: public AttributeType_ {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef typename AttributeType_::ValueTypeDepl ValueTypeDepl;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;
    typedef typename AttributeType_::ChangedEvent ChangedEvent;

    template <typename... AttributeType_Arguments>
    DBusObservableAttribute(DBusProxy &_proxy,
                            const char *_changedEventName,
                            AttributeType_Arguments... arguments)
         : AttributeType_(_proxy, arguments...),
           changedEvent_(_proxy, _changedEventName, this->setMethodSignature_, this->getMethodName_,
                               std::make_tuple(CommonAPI::Deployable<ValueType, ValueTypeDepl>(this->depl_))) {
    }

    ChangedEvent &getChangedEvent() {
        return changedEvent_;
    }

 protected:
    DBusEvent<ChangedEvent, CommonAPI::Deployable<ValueType, ValueTypeDepl> > changedEvent_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_ATTRIBUTE_HPP_
