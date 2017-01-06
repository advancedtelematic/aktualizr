// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSFREEDESKTOPSTUBADAPTERHELPER_HPP_
#define COMMONAPI_DBUS_DBUSFREEDESKTOPSTUBADAPTERHELPER_HPP_

#include <CommonAPI/Struct.hpp>
#include <CommonAPI/DBus/DBusStubAdapterHelper.hpp>

namespace CommonAPI {
namespace DBus {

template <typename StubClass_>
class DBusGetFreedesktopAttributeStubDispatcherBase {
public:
    virtual ~DBusGetFreedesktopAttributeStubDispatcherBase() {}
    virtual void dispatchDBusMessageAndAppendReply(const DBusMessage &_message,
                                                   const std::shared_ptr<StubClass_> &_stub,
                                                   DBusOutputStream &_output,
                                                   const std::shared_ptr<DBusClientId> &_clientId) = 0;
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusGetFreedesktopAttributeStubDispatcher
        : public virtual DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>,
          public virtual DBusGetFreedesktopAttributeStubDispatcherBase<StubClass_> {
public:
    typedef DBusStubAdapterHelper<StubClass_> DBusStubAdapterHelperType;
    typedef typename DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;

    DBusGetFreedesktopAttributeStubDispatcher(GetStubFunctor _getStubFunctor, AttributeDepl_ *_depl = nullptr)
        : DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, "v", _depl) {
    }

    virtual ~DBusGetFreedesktopAttributeStubDispatcher() {};

    void dispatchDBusMessageAndAppendReply(const DBusMessage &_message,
                                           const std::shared_ptr<StubClass_> &_stub,
                                           DBusOutputStream &_output,
                                           const std::shared_ptr<DBusClientId> &_clientId) {
        (void)_message;
        VariantDeployment<AttributeDepl_> actualDepl(true, DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::depl_);
        CommonAPI::Deployable<CommonAPI::Variant<AttributeType_>, VariantDeployment<AttributeDepl_>> deployedVariant(
                (_stub.get()->*(DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::getStubFunctor_))(_clientId), &actualDepl);

        _output << deployedVariant;
    }

protected:
   virtual bool sendAttributeValueReply(const DBusMessage &_message, const std::shared_ptr<StubClass_> &_stub, DBusStubAdapterHelperType &_helper) {
       DBusMessage reply = _message.createMethodReturn(DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::signature_);

       VariantDeployment<AttributeDepl_> actualDepl(true, DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::depl_);
       std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(_message.getSender()));
       CommonAPI::Deployable<CommonAPI::Variant<AttributeType_>, VariantDeployment<AttributeDepl_>> deployedVariant(
                (_stub.get()->*(DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::getStubFunctor_))(clientId), &actualDepl);

       DBusOutputStream output(reply);
       output << deployedVariant;
       output.flush();

       return _helper.getDBusConnection()->sendDBusMessage(reply);
   }
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusSetFreedesktopAttributeStubDispatcher
        : public virtual DBusGetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>,
          public virtual DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::DBusStubAdapterHelperType DBusStubAdapterHelperType;
    typedef typename DBusStubAdapterHelperType::RemoteEventHandlerType RemoteEventHandlerType;
    typedef bool (RemoteEventHandlerType::*OnRemoteSetFunctor)(std::shared_ptr<CommonAPI::ClientId>, AttributeType_);
    typedef void (RemoteEventHandlerType::*OnRemoteChangedFunctor)();

    DBusSetFreedesktopAttributeStubDispatcher(
            GetStubFunctor _getStubFunctor,
            OnRemoteSetFunctor _onRemoteSetFunctor,
            OnRemoteChangedFunctor _onRemoteChangedFunctor,
            AttributeDepl_ * _depl = nullptr)
        : DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, "v", _depl),
          DBusGetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, _depl),
          DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, _onRemoteSetFunctor, _onRemoteChangedFunctor, "v", _depl) {
    }

    virtual ~DBusSetFreedesktopAttributeStubDispatcher() {};

protected:
    virtual AttributeType_ retrieveAttributeValue(const DBusMessage &_message, bool &_error) {
        std::string interfaceName, attributeName;
        DBusInputStream input(_message);
        VariantDeployment<AttributeDepl_> actualDepl(true, DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::depl_);
        CommonAPI::Deployable<CommonAPI::Variant<AttributeType_>, VariantDeployment<AttributeDepl_>> deployedVariant(&actualDepl);
        input >> interfaceName; // skip over interface and attribute name
        input >> attributeName;
        input >> deployedVariant;
        _error = input.hasError();
        AttributeType_ attributeValue = deployedVariant.getValue().template get<AttributeType_>() ;
        return attributeValue;
    }
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusSetFreedesktopObservableAttributeStubDispatcher
        : public virtual DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>,
          public virtual DBusSetObservableAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::DBusStubAdapterHelperType DBusStubAdapterHelperType;
    typedef typename DBusStubAdapterHelperType::StubAdapterType StubAdapterType;
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef void (StubAdapterType::*FireChangedFunctor)(const AttributeType_&);

    DBusSetFreedesktopObservableAttributeStubDispatcher(
            GetStubFunctor _getStubFunctor,
            OnRemoteSetFunctor _onRemoteSetFunctor,
            OnRemoteChangedFunctor _onRemoteChangedFunctor,
            FireChangedFunctor _fireChangedFunctor,
            AttributeDepl_ *_depl = nullptr)
        : DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, "v", _depl),
          DBusGetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, _depl),
          DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, _onRemoteSetFunctor, _onRemoteChangedFunctor, "v", _depl),
          DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, _onRemoteSetFunctor, _onRemoteChangedFunctor, _depl),
          DBusSetObservableAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(_getStubFunctor, _onRemoteSetFunctor, _onRemoteChangedFunctor, _fireChangedFunctor, "v", _depl) {
    }
};

template<typename, typename>
struct DBusStubFreedesktopPropertiesSignalHelper;

template<typename DataType_, typename DeplType_>
struct DBusStubFreedesktopPropertiesSignalHelper {

    template <typename ValueType_>
    struct DBusPropertiesEntry
            : public CommonAPI::Struct<std::string, CommonAPI::Variant<ValueType_>> {

        DBusPropertiesEntry() = default;
        DBusPropertiesEntry(const std::string &_propertyName,
                            const ValueType_ &_propertyValue) {
            std::get<0>(this->values_) = _propertyName;
            std::get<1>(this->values_) = _propertyValue;
        };

        const std::string &getPropertyName() const { return std::get<0>(this->values_); }
        void setPropertyName(const std::string &_value) { std::get<0>(this->values_) = _value; }

        const ValueType_ getPropertyValue() const { return std::get<1>(this->values_); }
        void setPropertyValue(const ValueType_ &_value) { std::get<1>(this->values_) = _value; }
    };

    typedef std::vector<DBusPropertiesEntry<DataType_>> PropertiesArray;
    typedef CommonAPI::Deployment<CommonAPI::EmptyDeployment, VariantDeployment<DeplType_>> PropertyDeployment;
    typedef CommonAPI::ArrayDeployment<PropertyDeployment> PropertiesDeployment;
    typedef CommonAPI::Deployable<PropertiesArray, PropertiesDeployment> DeployedPropertiesArray;

    template <typename StubClass_>
    static bool sendPropertiesChangedSignal(const StubClass_ &_stub, const std::string &_propertyName, const DataType_ &_inArg, DeplType_ *_depl) {
        const std::vector<std::string> invalidatedProperties;
        PropertiesArray changedProperties;
        DBusPropertiesEntry<DataType_> entry(_propertyName, _inArg);
        changedProperties.push_back(entry);

        VariantDeployment<DeplType_> actualDepl(true, _depl);
        PropertyDeployment propertyDeployment(nullptr, &actualDepl);
        PropertiesDeployment changedPropertiesDeployment(&propertyDeployment);

        DeployedPropertiesArray deployedChangedProperties(changedProperties, &changedPropertiesDeployment);

        return DBusStubSignalHelper<
                    DBusSerializableArguments<
                        const std::string,
                        DeployedPropertiesArray,
                        std::vector<std::string>
                    >
               >::sendSignal(
                    _stub.getDBusAddress().getObjectPath().c_str(),
                    "org.freedesktop.DBus.Properties",
                    "PropertiesChanged",
                    "sa{sv}as",
                    _stub.getDBusConnection(),
                    _stub.getInterface(),
                    deployedChangedProperties,
                    invalidatedProperties);
    }
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSFREEDESKTOPSTUBADAPTERHELPER_HPP_
