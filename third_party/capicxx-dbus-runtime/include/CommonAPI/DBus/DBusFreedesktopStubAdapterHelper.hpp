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

#  if _MSC_VER >= 1300
/*
* Diamond inheritance is used for the DBusSetAttributeStubDispatcher base class.
* The Microsoft compiler put warning (C4250) using a desired c++ feature: "Delegating to a sister class"
* A powerful technique that arises from using virtual inheritance is to delegate a method from a class in another class
* by using a common abstract base class. This is also called cross delegation.
*/
#    pragma warning( disable : 4250 )
#  endif

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
   virtual bool sendAttributeValueReply(const DBusMessage &_message, const std::shared_ptr<StubClass_> &_stub, std::weak_ptr<DBusProxyConnection> connection_) {
       DBusMessage reply = _message.createMethodReturn(DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::signature_);

       VariantDeployment<AttributeDepl_> actualDepl(true, DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::depl_);
       std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(_message.getSender()));
       CommonAPI::Deployable<CommonAPI::Variant<AttributeType_>, VariantDeployment<AttributeDepl_>> deployedVariant(
                (_stub.get()->*(DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::getStubFunctor_))(clientId), &actualDepl);

       DBusOutputStream output(reply);
       output << deployedVariant;
       output.flush();
       if (std::shared_ptr<DBusProxyConnection> connection = connection_.lock()) {
           return connection->sendDBusMessage(reply);
       } else {
           return false;
       }
   }
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusSetFreedesktopAttributeStubDispatcher
        : public virtual DBusGetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>,
          public virtual DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
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
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename DBusSetFreedesktopAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef typename StubClass_::StubAdapterType StubAdapterType;
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

    typedef std::unordered_map<std::string, Variant<DataType_>> PropertyMap;
    typedef MapDeployment<EmptyDeployment, VariantDeployment<DeplType_>> PropertyMapDeployment;
    typedef Deployable<PropertyMap, PropertyMapDeployment> DeployedPropertyMap;

    template <typename StubClass_>
    static bool sendPropertiesChangedSignal(const StubClass_ &_stub, const std::string &_propertyName, const DataType_ &_inArg, DeplType_ *_depl) {
        const std::vector<std::string> invalidatedProperties;

        // fill out property map
        PropertyMap changedProperties;
        changedProperties[_propertyName] = _inArg;

        VariantDeployment<DeplType_> actualDepl(true, _depl);
        PropertyMapDeployment changedPropertiesDeployment(nullptr, &actualDepl);
        DeployedPropertyMap deployedChangedProperties(changedProperties, &changedPropertiesDeployment);

        return DBusStubSignalHelper<
                    DBusSerializableArguments<
                        const std::string,
                        DeployedPropertyMap,
                        std::vector<std::string>
                    >
               >::sendSignal(
                    _stub.getDBusAddress().getObjectPath().c_str(),
                    "org.freedesktop.DBus.Properties",
                    "PropertiesChanged",
                    "sa{sv}as",
                    _stub.getDBusConnection(),
                    _stub.getDBusAddress().getInterface(),
                    deployedChangedProperties,
                    invalidatedProperties);
    }
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSFREEDESKTOPSTUBADAPTERHELPER_HPP_
