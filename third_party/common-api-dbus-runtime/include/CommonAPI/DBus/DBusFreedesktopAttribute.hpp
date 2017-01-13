// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_FREEDESKTOPATTRIBUTE_HPP_
#define COMMONAPI_DBUS_DBUS_FREEDESKTOPATTRIBUTE_HPP_

#include <CommonAPI/DBus/DBusDeployment.hpp>

namespace CommonAPI {
namespace DBus {

template <typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusFreedesktopReadonlyAttribute: public AttributeType_ {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef AttributeDepl_ ValueTypeDepl;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;

    DBusFreedesktopReadonlyAttribute(DBusProxy &_proxy, const std::string &_interfaceName, const std::string &_propertyName,
        AttributeDepl_ *_depl = nullptr)
        : proxy_(_proxy),
          interfaceName_(_interfaceName),
          propertyName_(_propertyName),
          depl_(_depl) {
    }

    void getValue(CommonAPI::CallStatus &_status, ValueType &_value, const CommonAPI::CallInfo *_info) const {
        VariantDeployment<AttributeDepl_> actualDepl(true, depl_);
        CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>> deployedValue(&actualDepl);
        DBusProxyHelper<
            DBusSerializableArguments<
                std::string, std::string
            >,
            DBusSerializableArguments<
                CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>>
            >
        >::callMethodWithReply(
                proxy_,
                "org.freedesktop.DBus.Properties",
                "Get",
                "ss",
                (_info ? _info : &defaultCallInfo),
                interfaceName_,
                propertyName_,
                _status,
                deployedValue);

        _value = deployedValue.getValue().template get<ValueType>();
    }

    std::future<CallStatus> getValueAsync(AttributeAsyncCallback _callback, const CommonAPI::CallInfo *_info) {
        VariantDeployment<AttributeDepl_> actualDepl(true, depl_);
        CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>> deployedValue(&actualDepl);
        return DBusProxyHelper<
                    DBusSerializableArguments<
                        std::string, std::string
                    >,
                    DBusSerializableArguments<
                        CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>>
                    >
               >::callMethodAsync(
                        proxy_,
                        "org.freedesktop.DBus.Properties",
                        "Get",
                        "ss",
                        (_info ? _info : &defaultCallInfo),
                        interfaceName_,
                        propertyName_,
                        [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>> _value) {
                            _callback(_status, _value.getValue().template get<ValueType>());
                        },
                        std::make_tuple(deployedValue)
                );
    }
    AttributeDepl_ *getDepl(void) {
        return depl_;
    }
protected:
    DBusProxy &proxy_;
    std::string interfaceName_;
    std::string propertyName_;
    AttributeDepl_ *depl_;
};

template <typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusFreedesktopAttribute
        : public DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_> {
 public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;

    DBusFreedesktopAttribute(DBusProxy &_proxy, const std::string &_interfaceName, const std::string &_propertyName, AttributeDepl_ *_depl = nullptr)
        : DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>(_proxy, _interfaceName, _propertyName, _depl) {
    }

    void setValue(const ValueType &_request, CommonAPI::CallStatus &_status, ValueType &_response, const CommonAPI::CallInfo *_info) {
        VariantDeployment<AttributeDepl_> actualDepl(true, DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::depl_);
        Variant<ValueType> reqVariant(_request);
        CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>> deployedVariant(reqVariant, &actualDepl);
        DBusProxyHelper<
            DBusSerializableArguments<
                std::string, std::string, CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>>
            >,
            DBusSerializableArguments<
            >
        >::callMethodWithReply(
                DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::proxy_,
                "org.freedesktop.DBus.Properties",
                "Set",
                "ssv",
                (_info ? _info : &defaultCallInfo),
                DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::interfaceName_,
                DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::propertyName_,
                deployedVariant,
                _status);
        _response = _request;
    }

    std::future<CommonAPI::CallStatus> setValueAsync(const ValueType &_request, AttributeAsyncCallback _callback, const CommonAPI::CallInfo *_info) {
        VariantDeployment<AttributeDepl_> actualDepl(true, DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::depl_);
        Variant<ValueType> reqVariant(_request);
        CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>> deployedVariant(reqVariant, &actualDepl);
        return DBusProxyHelper<
                    DBusSerializableArguments<
                        std::string, std::string, CommonAPI::Deployable<Variant<ValueType>, VariantDeployment<AttributeDepl_>>
                    >,
                    DBusSerializableArguments<
                    >
               >::callMethodAsync(
                    DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::proxy_,
                    "org.freedesktop.DBus.Properties",
                    "Set",
                    "ssv",
                    (_info ? _info : &defaultCallInfo),
                    DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::interfaceName_,
                    DBusFreedesktopReadonlyAttribute<AttributeType_, AttributeDepl_>::propertyName_,
                    deployedVariant,
                    [_callback, deployedVariant](CommonAPI::CallStatus _status) {
                        _callback(_status, deployedVariant.getValue().template get<ValueType>());
                    },
                    std::tuple<>());
    }
};

template<class, class, class>
class LegacyEvent;
template <template <class...> class Type_, class Types_, class DataType_, class DataDeplType_>
class LegacyEvent<Type_<Types_>, DataType_, DataDeplType_>: public Type_<Types_>,
    public DBusProxyConnection::DBusSignalHandler {
public:
    typedef Types_ ValueType;
    typedef typename Type_<ValueType>::Listener Listener;
    typedef std::unordered_map<std::string, Variant<DataType_>> PropertyMap;
    typedef MapDeployment<EmptyDeployment, VariantDeployment<DataDeplType_>> PropertyMapDeployment;
    typedef Deployable<PropertyMap, PropertyMapDeployment> DeployedPropertyMap;
    typedef std::vector<std::string> InvalidArray;
    typedef Event<std::string, DeployedPropertyMap, InvalidArray> SignalEvent;
    typedef typename Type_<ValueType>::Subscription Subscription;

    LegacyEvent(DBusProxy &_proxy, const std::string &_interfaceName, const std::string &_propertyName, DataDeplType_ *_depl)
        : interfaceName_(_interfaceName),
          propertyName_(_propertyName),
          variantDepl_(true, _depl),
          mapDepl_(nullptr, &variantDepl_),
          deployedMap_(&mapDepl_),
          proxy_(_proxy),
          isSubcriptionSet_(false),
          internalEvent_(_proxy,
                         "PropertiesChanged",
                         "sa{sv}as",
                         _proxy.getDBusAddress().getObjectPath(),
                         "org.freedesktop.DBus.Properties",
                         std::make_tuple("", deployedMap_, InvalidArray())) {
    }

protected:
    void onFirstListenerAdded(const Listener &) {
        if (!isSubcriptionSet_) {
            subscription_ = internalEvent_.subscribe(
                                [this](const std::string &_interfaceName,
                                       const PropertyMap &_properties,
                                       const InvalidArray &_invalid) {
                                    (void)_invalid;
                                    if (interfaceName_ == _interfaceName) {
                                        auto iter = _properties.find(propertyName_);
                                        if (iter != _properties.end()) {
                                            const ValueType &value = iter->second.template get<ValueType>();
                                            this->notifyListeners(value);
                                        }
                                    }
                                });

            isSubcriptionSet_ = true;
        }
    }

    virtual void onListenerAdded(const Listener& listener, const Subscription subscription) {
        (void)listener;
        proxy_.freeDesktopGetCurrentValueForSignalListener(this, subscription, interfaceName_, propertyName_);
    }

    virtual void onInitialValueSignalDBusMessage(const DBusMessage&_message, const uint32_t tag) {
        CommonAPI::Deployable<Variant<DataType_>, VariantDeployment<DataDeplType_>> deployedValue(&variantDepl_);
        DBusInputStream input(_message);
        if (DBusSerializableArguments<
             CommonAPI::Deployable<
              Variant<DataType_>,
              VariantDeployment<DataDeplType_>
             >
            >::deserialize(input, deployedValue)) {
                Variant<DataType_> v = deployedValue.getValue();
            const DataType_ &value = v.template get<DataType_>();
            this->notifySpecificListener(tag, value);
        }
    }
    virtual void onSignalDBusMessage(const DBusMessage&) {
        // ignore
    }
    void onLastListenerRemoved(const Listener &) {
        if (isSubcriptionSet_) {
            internalEvent_.unsubscribe(subscription_);
            isSubcriptionSet_ = false;
        }
    }

    std::string interfaceName_;
    std::string propertyName_;
    VariantDeployment<DataDeplType_> variantDepl_;
    PropertyMapDeployment mapDepl_;
    DeployedPropertyMap deployedMap_;
    DBusProxy &proxy_;

    typename DBusEvent<SignalEvent, std::string, DeployedPropertyMap, InvalidArray>::Subscription subscription_;
    bool isSubcriptionSet_;

    DBusEvent<SignalEvent, std::string, DeployedPropertyMap, InvalidArray> internalEvent_;
};

template <typename AttributeType_>
class DBusFreedesktopObservableAttribute: public AttributeType_ {
 public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;
    typedef typename AttributeType_::ChangedEvent ChangedEvent;
    typedef typename AttributeType_::ValueTypeDepl ValueTypeDepl;
    template <typename... AttributeType_Arguments>
    DBusFreedesktopObservableAttribute(DBusProxy &_proxy,
                                       const std::string &_interfaceName,
                                       const std::string &_propertyName,
                                       AttributeType_Arguments... _arguments)
        : AttributeType_(_proxy, _interfaceName, _propertyName, _arguments...),
          interfaceName_(_interfaceName),
          propertyName_(_propertyName),
          externalChangedEvent_(_proxy, _interfaceName, _propertyName, AttributeType_::getDepl()) {
    }

    ChangedEvent &getChangedEvent() {
        return externalChangedEvent_;
    }

 protected:
    std::string interfaceName_;
    std::string propertyName_;
    LegacyEvent<ChangedEvent, ValueType, ValueTypeDepl> externalChangedEvent_;};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_FREEDESKTOPATTRIBUTE_HPP_
