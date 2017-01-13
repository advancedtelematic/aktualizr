// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSSTUBADAPTERHELPER_HPP_
#define COMMONAPI_DBUS_DBUSSTUBADAPTERHELPER_HPP_

#include <initializer_list>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <map>

#include <CommonAPI/Variant.hpp>
#include <CommonAPI/DBus/DBusStubAdapter.hpp>
#include <CommonAPI/DBus/DBusInputStream.hpp>
#include <CommonAPI/DBus/DBusOutputStream.hpp>
#include <CommonAPI/DBus/DBusHelper.hpp>
#include <CommonAPI/DBus/DBusSerializableArguments.hpp>
#include <CommonAPI/DBus/DBusClientId.hpp>

namespace CommonAPI {
namespace DBus {

class StubDispatcherBase {
public:
   virtual ~StubDispatcherBase() { }
};




struct DBusAttributeDispatcherStruct {
    StubDispatcherBase* getter;
    StubDispatcherBase* setter;

    DBusAttributeDispatcherStruct(StubDispatcherBase* g, StubDispatcherBase* s) {
        getter = g;
        setter = s;
    }
};

typedef std::unordered_map<std::string, DBusAttributeDispatcherStruct> StubAttributeTable;

template <typename StubClass_>
class DBusStubAdapterHelper: public virtual DBusStubAdapter {
 public:
    typedef typename StubClass_::StubAdapterType StubAdapterType;
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    class StubDispatcher: public StubDispatcherBase {
    public:
        virtual ~StubDispatcher() {}
        virtual bool dispatchDBusMessage(const DBusMessage &_message,
                                         const std::shared_ptr<StubClass_> &_stub,
                                         DBusStubAdapterHelper<StubClass_> &_helper) = 0;
        virtual void appendGetAllReply(const DBusMessage &_message,
                                       const std::shared_ptr<StubClass_> &_stub,
                                       DBusStubAdapterHelper<StubClass_> &_helper,
                                       DBusOutputStream &_output) {
            (void)_message;
            (void)_stub;
            (void)_helper;
            (void)_output;
        }
    };
    // interfaceMemberName, interfaceMemberSignature
    typedef std::pair<const char*, const char*> DBusInterfaceMemberPath;
    typedef std::unordered_map<DBusInterfaceMemberPath, StubDispatcherBase*> StubDispatcherTable;

    DBusStubAdapterHelper(const DBusAddress &_address,
                          const std::shared_ptr<DBusProxyConnection> &_connection,
                          const std::shared_ptr<StubClass_> &_stub,
                          const bool _isManaging):
                    DBusStubAdapter(_address, _connection, _isManaging),
                    stub_(_stub),
                    remoteEventHandler_(nullptr) {
    }

    virtual ~DBusStubAdapterHelper() {
        DBusStubAdapter::deinit();
        stub_.reset();
    }

    virtual void init(std::shared_ptr<DBusStubAdapter> instance) {
        DBusStubAdapter::init(instance);
        std::shared_ptr<StubAdapterType> stubAdapter = std::dynamic_pointer_cast<StubAdapterType>(instance);
        remoteEventHandler_ = stub_->initStubAdapter(stubAdapter);
    }

    virtual void deinit() {
        DBusStubAdapter::deinit();
        stub_.reset();
    }

    inline RemoteEventHandlerType* getRemoteEventHandler() {
        return remoteEventHandler_;
    }

 protected:

    virtual bool onInterfaceDBusMessage(const DBusMessage& dbusMessage) {
        const char* interfaceMemberName = dbusMessage.getMember();
        const char* interfaceMemberSignature = dbusMessage.getSignature();

        assert(interfaceMemberName);
        assert(interfaceMemberSignature);

        DBusInterfaceMemberPath dbusInterfaceMemberPath(interfaceMemberName, interfaceMemberSignature);
        auto findIterator = getStubDispatcherTable().find(dbusInterfaceMemberPath);
        const bool foundInterfaceMemberHandler = (findIterator != getStubDispatcherTable().end());
        bool dbusMessageHandled = false;
        if (foundInterfaceMemberHandler) {
            StubDispatcher* stubDispatcher = static_cast<StubDispatcher*>(findIterator->second);
            dbusMessageHandled = stubDispatcher->dispatchDBusMessage(dbusMessage, stub_, *this);
        }

        return dbusMessageHandled;
    }

    virtual bool onInterfaceDBusFreedesktopPropertiesMessage(const DBusMessage &_message) {
        DBusInputStream input(_message);

        if (_message.hasMemberName("Get")) {
            return handleFreedesktopGet(_message, input);
        } else if (_message.hasMemberName("Set")) {
            return handleFreedesktopSet(_message, input);
        } else if (_message.hasMemberName("GetAll")) {
            return handleFreedesktopGetAll(_message, input);
        }

        return false;
    }

    virtual const StubDispatcherTable& getStubDispatcherTable() = 0;
    virtual const StubAttributeTable& getStubAttributeTable() = 0;

    std::shared_ptr<StubClass_> stub_;
    RemoteEventHandlerType* remoteEventHandler_;

 private:
    bool handleFreedesktopGet(const DBusMessage &_message, DBusInputStream &_input) {
        std::string interfaceName;
        std::string attributeName;
        _input >> interfaceName;
        _input >> attributeName;

        if (_input.hasError()) {
            return false;
        }

        auto attributeDispatcherIterator = getStubAttributeTable().find(attributeName);
        if (attributeDispatcherIterator == getStubAttributeTable().end()) {
            return false;
        }

        StubDispatcher* getterDispatcher = static_cast<StubDispatcher*>(attributeDispatcherIterator->second.getter);
        assert(getterDispatcher != NULL); // all attributes have at least a getter
        return (getterDispatcher->dispatchDBusMessage(_message, stub_, *this));
    }

    bool handleFreedesktopSet(const DBusMessage& dbusMessage, DBusInputStream& dbusInputStream) {
        std::string interfaceName;
        std::string attributeName;
        dbusInputStream >> interfaceName;
        dbusInputStream >> attributeName;

        if(dbusInputStream.hasError()) {
            return false;
        }

        auto attributeDispatcherIterator = getStubAttributeTable().find(attributeName);
        if(attributeDispatcherIterator == getStubAttributeTable().end()) {
            return false;
        }

        StubDispatcher *setterDispatcher = static_cast<StubDispatcher*>(attributeDispatcherIterator->second.setter);
        if (setterDispatcher == NULL) { // readonly attributes do not have a setter
            return false;
        }

        return setterDispatcher->dispatchDBusMessage(dbusMessage, stub_, *this);
    }

    bool handleFreedesktopGetAll(const DBusMessage& dbusMessage, DBusInputStream& dbusInputStream) {
        std::string interfaceName;
        dbusInputStream >> interfaceName;

        if(dbusInputStream.hasError()) {
            return false;
        }

        DBusMessage dbusMessageReply = dbusMessage.createMethodReturn("a{sv}");
        DBusOutputStream dbusOutputStream(dbusMessageReply);

        dbusOutputStream.beginWriteMap();

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));
        for(auto attributeDispatcherIterator = getStubAttributeTable().begin(); attributeDispatcherIterator != getStubAttributeTable().end(); attributeDispatcherIterator++) {

            //To prevent the destruction of the stub whilst still handling a message
            if (stub_) {
                StubDispatcher* getterDispatcher = static_cast<StubDispatcher*>(attributeDispatcherIterator->second.getter);
                assert(getterDispatcher != NULL); // all attributes have at least a getter
                dbusOutputStream.align(8);
                dbusOutputStream << attributeDispatcherIterator->first;
                getterDispatcher->appendGetAllReply(dbusMessage, stub_, *this, dbusOutputStream);
            }
        }

        dbusOutputStream.endWriteMap();
        dbusOutputStream.flush();

        return getDBusConnection()->sendDBusMessage(dbusMessageReply);
    }
};

template< class >
struct DBusStubSignalHelper;

template<template<class ...> class In_, class... InArgs_>
struct DBusStubSignalHelper<In_<DBusInputStream, DBusOutputStream, InArgs_...>> {

    static inline bool sendSignal(const char* objectPath,
                           const char* interfaceName,
                    const char* signalName,
                    const char* signalSignature,
                    const std::shared_ptr<DBusProxyConnection>& dbusConnection,
                    const InArgs_&... inArgs) {
        DBusMessage dbusMessage = DBusMessage::createSignal(
                        objectPath,
                        interfaceName,
                        signalName,
                        signalSignature);

        if (sizeof...(InArgs_) > 0) {
            DBusOutputStream outputStream(dbusMessage);
            const bool success = DBusSerializableArguments<InArgs_...>::serialize(outputStream, inArgs...);
            if (!success) {
                return false;
            }
            outputStream.flush();
        }

        const bool dbusMessageSent = dbusConnection->sendDBusMessage(dbusMessage);
        return dbusMessageSent;
    }

    template <typename DBusStub_ = DBusStubAdapter>
    static bool sendSignal(const DBusStub_ &_stub,
                    const char *_name,
                    const char *_signature,
                    const InArgs_&... inArgs) {
        return(sendSignal(_stub.getDBusAddress().getObjectPath().c_str(),
                          _stub.getDBusAddress().getInterface().c_str(),
                          _name,
                          _signature,
                          _stub.getDBusConnection(),
                          inArgs...));
    }


    template <typename DBusStub_ = DBusStubAdapter>
       static bool sendSignal(const char *_target,
                                    const DBusStub_ &_stub,
                                    const char *_name,
                                    const char *_signature,
                                    const InArgs_&... inArgs) {
           DBusMessage dbusMessage
                  = DBusMessage::createSignal(
                   _stub.getDBusAddress().getObjectPath().c_str(),
                   _stub.getDBusAddress().getInterface().c_str(),
                   _name,
                   _signature);

           dbusMessage.setDestination(_target);

           if (sizeof...(InArgs_) > 0) {
               DBusOutputStream outputStream(dbusMessage);
               const bool success = DBusSerializableArguments<InArgs_...>::serialize(outputStream, inArgs...);
               if (!success) {
                   return false;
               }
               outputStream.flush();
           }

           return _stub.getDBusConnection()->sendDBusMessage(dbusMessage);
       }
};

template< class, class, class >
class DBusMethodStubDispatcher;

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class DeplIn_, class... DeplIn_Args>

class DBusMethodStubDispatcher<StubClass_, In_<InArgs_...>, DeplIn_<DeplIn_Args...> >: public DBusStubAdapterHelper<StubClass_>::StubDispatcher {
 public:
    typedef DBusStubAdapterHelper<StubClass_> DBusStubAdapterHelperType;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_...);

    DBusMethodStubDispatcher(StubFunctor_ stubFunctor, std::tuple<DeplIn_Args*...> _in):
            stubFunctor_(stubFunctor) {
            initialize(typename make_sequence_range<sizeof...(DeplIn_Args), 0>::type(), _in);
    }

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        return handleDBusMessage(dbusMessage, stub, dbusStubAdapterHelper, typename make_sequence_range<sizeof...(InArgs_), 0>::type());
    }

 private:
    template <int... DeplIn_ArgIndices>
    inline void initialize(index_sequence<DeplIn_ArgIndices...>, std::tuple<DeplIn_Args*...> &_in) {
        in_ = std::make_tuple(std::get<DeplIn_ArgIndices>(_in)...);
    }

    template <int... InArgIndices_>
    inline bool handleDBusMessage(const DBusMessage& dbusMessage,
                                  const std::shared_ptr<StubClass_>& stub,
                                  DBusStubAdapterHelperType& dbusStubAdapterHelper,
                                  index_sequence<InArgIndices_...>) {
        (void)dbusStubAdapterHelper;

        if (sizeof...(InArgs_) > 0) {
            DBusInputStream dbusInputStream(dbusMessage);
            const bool success = DBusSerializableArguments<CommonAPI::Deployable<InArgs_, DeplIn_Args>...>::deserialize(dbusInputStream, std::get<InArgIndices_>(in_)...);
            if (!success)
                return false;
        }

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));

        (stub.get()->*stubFunctor_)(clientId, std::move(std::get<InArgIndices_>(in_).getValue())...);

        return true;
    }

    StubFunctor_ stubFunctor_;
    std::tuple<CommonAPI::Deployable<InArgs_, DeplIn_Args>...> in_;
};


template< class, class, class, class, class>
class DBusMethodWithReplyStubDispatcher;

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_,
    template <class...> class DeplIn_, class... DeplIn_Args,
    template <class...> class DeplOut_, class... DeplOutArgs_>

class DBusMethodWithReplyStubDispatcher<
       StubClass_,
       In_<InArgs_...>,
       Out_<OutArgs_...>,
       DeplIn_<DeplIn_Args...>,
       DeplOut_<DeplOutArgs_...> >:
            public DBusStubAdapterHelper<StubClass_>::StubDispatcher {
 public:
    typedef DBusStubAdapterHelper<StubClass_> DBusStubAdapterHelperType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;
    typedef void (StubClass_::*StubFunctor_)(
                std::shared_ptr<CommonAPI::ClientId>, InArgs_..., ReplyType_t);

    DBusMethodWithReplyStubDispatcher(StubFunctor_ stubFunctor,
        const char* dbusReplySignature, 
        std::tuple<DeplIn_Args*...> _inDepArgs,
        std::tuple<DeplOutArgs_*...> _outDepArgs):
            stubFunctor_(stubFunctor),
            dbusReplySignature_(dbusReplySignature),
            out_(_outDepArgs),
            currentCall_(0) {

        initialize(typename make_sequence_range<sizeof...(DeplIn_Args), 0>::type(), _inDepArgs);

    }

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, 
                             const std::shared_ptr<StubClass_>& stub,
                             DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        connection_ = dbusStubAdapterHelper.getDBusConnection();
        return handleDBusMessage(
                        dbusMessage,
                        stub,
                        dbusStubAdapterHelper,
                        typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                        typename make_sequence_range<sizeof...(OutArgs_), 0>::type());
    }

    bool sendReply(CommonAPI::CallId_t _call, 
                       std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> args = std::make_tuple()) {
        return sendReplyInternal(_call, typename make_sequence_range<sizeof...(OutArgs_), 0>::type(), args);
    }

private:

    template <int... DeplIn_ArgIndices>
    inline void initialize(index_sequence<DeplIn_ArgIndices...>, std::tuple<DeplIn_Args*...> &_in) {
        in_ = std::make_tuple(std::get<DeplIn_ArgIndices>(_in)...);
    }


    template <int... InArgIndices_, int... OutArgIndices_>
    inline bool handleDBusMessage(const DBusMessage& dbusMessage,
                                  const std::shared_ptr<StubClass_>& stub,
                                  DBusStubAdapterHelperType& dbusStubAdapterHelper,
                                  index_sequence<InArgIndices_...>,
                                  index_sequence<OutArgIndices_...>) {
        (void)dbusStubAdapterHelper;
        if (sizeof...(DeplIn_Args) > 0) {
            DBusInputStream dbusInputStream(dbusMessage);
            const bool success = DBusSerializableArguments<CommonAPI::Deployable<InArgs_, DeplIn_Args>...>::deserialize(dbusInputStream, std::get<InArgIndices_>(in_)...);
            if (!success)
                return false;
        }

        std::shared_ptr<DBusClientId> clientId
            = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));
        DBusMessage reply = dbusMessage.createMethodReturn(dbusReplySignature_);

        CommonAPI::CallId_t call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            call = currentCall_++;
            pending_[call] = reply;
        }

        (stub.get()->*stubFunctor_)(
            clientId,
            std::move(std::get<InArgIndices_>(in_).getValue())...,
            [call, this](OutArgs_... _args){
                this->sendReply(call, std::make_tuple(CommonAPI::Deployable<OutArgs_, DeplOutArgs_>(
                            _args, std::get<OutArgIndices_>(out_)
                        )...));
            }
        );

           return true;
    }

    template<int... OutArgIndices_>
    bool sendReplyInternal(CommonAPI::CallId_t _call,
                           index_sequence<OutArgIndices_...>,
                           std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> args) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto reply = pending_.find(_call);
        if (reply != pending_.end()) {
            if (sizeof...(DeplOutArgs_) > 0) {
                DBusOutputStream output(reply->second);
                if (!DBusSerializableArguments<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>::serialize(
                        output, std::get<OutArgIndices_>(args)...)) {
                    (void)args;
                    pending_.erase(_call);
                    return false;
                }
                output.flush();
            }
            bool isSuccessful = connection_->sendDBusMessage(reply->second);
            pending_.erase(_call);
            return isSuccessful;
        }
        return false;
    }

    StubFunctor_ stubFunctor_;
    const char* dbusReplySignature_;

    std::tuple<CommonAPI::Deployable<InArgs_, DeplIn_Args>...> in_;
    std::tuple<DeplOutArgs_*...> out_;
    CommonAPI::CallId_t currentCall_;
    std::map<CommonAPI::CallId_t, DBusMessage> pending_;
    std::mutex mutex_; // protects pending_

    std::shared_ptr<DBusProxyConnection> connection_;
};

template< class, class, class, class >
class DBusMethodWithReplyAdapterDispatcher;

template <
    typename StubClass_,
    typename StubAdapterClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_>
class DBusMethodWithReplyAdapterDispatcher<StubClass_, StubAdapterClass_, In_<InArgs_...>, Out_<OutArgs_...> >:
            public DBusStubAdapterHelper<StubClass_>::StubDispatcher {
 public:
    typedef DBusStubAdapterHelper<StubClass_> DBusStubAdapterHelperType;
    typedef void (StubAdapterClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., OutArgs_&...);
    typedef typename CommonAPI::Stub<typename DBusStubAdapterHelperType::StubAdapterType, typename StubClass_::RemoteEventType> StubType;

    DBusMethodWithReplyAdapterDispatcher(StubFunctor_ stubFunctor, const char* dbusReplySignature):
            stubFunctor_(stubFunctor),
            dbusReplySignature_(dbusReplySignature) {
    }

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        std::tuple<InArgs_..., OutArgs_...> argTuple;
        return handleDBusMessage(
                        dbusMessage,
                        stub,
                        dbusStubAdapterHelper,
                        typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                        typename make_sequence_range<sizeof...(OutArgs_), sizeof...(InArgs_)>::type(),argTuple);
    }

 private:
    template <int... InArgIndices_, int... OutArgIndices_>
    inline bool handleDBusMessage(const DBusMessage& dbusMessage,
                                  const std::shared_ptr<StubClass_>& stub,
                                  DBusStubAdapterHelperType& dbusStubAdapterHelper,
                                  index_sequence<InArgIndices_...>,
                                  index_sequence<OutArgIndices_...>,
                                  std::tuple<InArgs_..., OutArgs_...> argTuple) const {
        (void)argTuple;

        if (sizeof...(InArgs_) > 0) {
            DBusInputStream dbusInputStream(dbusMessage);
            const bool success = DBusSerializableArguments<InArgs_...>::deserialize(dbusInputStream, std::get<InArgIndices_>(argTuple)...);
            if (!success)
                return false;
        }

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));

        (stub->StubType::getStubAdapter().get()->*stubFunctor_)(clientId, std::move(std::get<InArgIndices_>(argTuple))..., std::get<OutArgIndices_>(argTuple)...);
        DBusMessage dbusMessageReply = dbusMessage.createMethodReturn(dbusReplySignature_);

        if (sizeof...(OutArgs_) > 0) {
            DBusOutputStream dbusOutputStream(dbusMessageReply);
            const bool success = DBusSerializableArguments<OutArgs_...>::serialize(dbusOutputStream, std::get<OutArgIndices_>(argTuple)...);
            if (!success)
                return false;

            dbusOutputStream.flush();
        }

        return dbusStubAdapterHelper.getDBusConnection()->sendDBusMessage(dbusMessageReply);
    }

    StubFunctor_ stubFunctor_;
    const char* dbusReplySignature_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusGetAttributeStubDispatcher: public virtual DBusStubAdapterHelper<StubClass_>::StubDispatcher {
 public:
    typedef DBusStubAdapterHelper<StubClass_> DBusStubAdapterHelperType;
    typedef const AttributeType_& (StubClass_::*GetStubFunctor)(std::shared_ptr<CommonAPI::ClientId>);

    DBusGetAttributeStubDispatcher(GetStubFunctor _getStubFunctor, const char *_signature, AttributeDepl_ *_depl = nullptr):
        getStubFunctor_(_getStubFunctor),
        signature_(_signature),
        depl_(_depl) {
    }

    virtual ~DBusGetAttributeStubDispatcher() {};

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        return sendAttributeValueReply(dbusMessage, stub, dbusStubAdapterHelper);
    }

    void appendGetAllReply(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper, DBusOutputStream &_output) {
        (void)dbusStubAdapterHelper;

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));
        auto varDepl = CommonAPI::DBus::VariantDeployment<AttributeDepl_>(true, depl_); // presuming FreeDesktop variant deployment, as support for "legacy" service only
        _output << CommonAPI::Deployable<CommonAPI::Variant<AttributeType_>, CommonAPI::DBus::VariantDeployment<AttributeDepl_>>((stub.get()->*getStubFunctor_)(clientId), &varDepl);
        _output.flush();
    }

 protected:
    virtual bool sendAttributeValueReply(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        DBusMessage dbusMessageReply = dbusMessage.createMethodReturn(signature_);
        DBusOutputStream dbusOutputStream(dbusMessageReply);

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));

        dbusOutputStream << CommonAPI::Deployable<AttributeType_, AttributeDepl_>((stub.get()->*getStubFunctor_)(clientId), depl_);
        dbusOutputStream.flush();

        return dbusStubAdapterHelper.getDBusConnection()->sendDBusMessage(dbusMessageReply);
    }


    GetStubFunctor getStubFunctor_;
    const char* signature_;
    AttributeDepl_ *depl_;
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusSetAttributeStubDispatcher: public virtual DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
 public:
    typedef typename DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::DBusStubAdapterHelperType DBusStubAdapterHelperType;
    typedef typename DBusStubAdapterHelperType::RemoteEventHandlerType RemoteEventHandlerType;

    typedef typename DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef bool (RemoteEventHandlerType::*OnRemoteSetFunctor)(std::shared_ptr<CommonAPI::ClientId>, AttributeType_);
    typedef void (RemoteEventHandlerType::*OnRemoteChangedFunctor)();

    DBusSetAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                                   OnRemoteSetFunctor onRemoteSetFunctor,
                                   OnRemoteChangedFunctor onRemoteChangedFunctor,
                                   const char* dbusSignature,
                                   AttributeDepl_ *_depl = nullptr) :
                    DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(getStubFunctor, dbusSignature, _depl),
                    onRemoteSetFunctor_(onRemoteSetFunctor),
                    onRemoteChangedFunctor_(onRemoteChangedFunctor) {
    }

    virtual ~DBusSetAttributeStubDispatcher() {};

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        bool attributeValueChanged;

        if (!setAttributeValue(dbusMessage, stub, dbusStubAdapterHelper, attributeValueChanged))
            return false;

        if (attributeValueChanged)
            notifyOnRemoteChanged(dbusStubAdapterHelper);

        return true;
    }

 protected:
    virtual AttributeType_ retrieveAttributeValue(const DBusMessage& dbusMessage, bool& errorOccured) {
        errorOccured = false;

        DBusInputStream dbusInputStream(dbusMessage);
        CommonAPI::Deployable<AttributeType_, AttributeDepl_> attributeValue(this->depl_);
        dbusInputStream >> attributeValue;

        if (dbusInputStream.hasError()) {
            errorOccured = true;
        }

        return attributeValue.getValue();
    }

    inline bool setAttributeValue(const DBusMessage& dbusMessage,
                                  const std::shared_ptr<StubClass_>& stub,
                                  DBusStubAdapterHelperType& dbusStubAdapterHelper,
                                  bool& attributeValueChanged) {
        bool errorOccured;
        CommonAPI::Deployable<AttributeType_, AttributeDepl_> attributeValue(
             retrieveAttributeValue(dbusMessage, errorOccured), this->depl_);

        if(errorOccured) {
            return false;
        }

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));

        attributeValueChanged = (dbusStubAdapterHelper.getRemoteEventHandler()->*onRemoteSetFunctor_)(clientId, std::move(attributeValue.getValue()));

        return this->sendAttributeValueReply(dbusMessage, stub, dbusStubAdapterHelper);
    }

    inline void notifyOnRemoteChanged(DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        (dbusStubAdapterHelper.getRemoteEventHandler()->*onRemoteChangedFunctor_)();
    }

    inline const AttributeType_& getAttributeValue(std::shared_ptr<CommonAPI::ClientId> clientId, const std::shared_ptr<StubClass_>& stub) {
        return (stub.get()->*(this->getStubFunctor_))(clientId);
    }

    const OnRemoteSetFunctor onRemoteSetFunctor_;
    const OnRemoteChangedFunctor onRemoteChangedFunctor_;
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusSetObservableAttributeStubDispatcher: public virtual DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
 public:
    typedef typename DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::DBusStubAdapterHelperType DBusStubAdapterHelperType;
    typedef typename DBusStubAdapterHelperType::StubAdapterType StubAdapterType;
    typedef typename DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef typename CommonAPI::Stub<StubAdapterType, typename StubClass_::RemoteEventType> StubType;
    typedef void (StubAdapterType::*FireChangedFunctor)(const AttributeType_&);

    DBusSetObservableAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                                             OnRemoteSetFunctor onRemoteSetFunctor,
                                             OnRemoteChangedFunctor onRemoteChangedFunctor,
                                             FireChangedFunctor fireChangedFunctor,
                                             const char* dbusSignature,
                                             AttributeDepl_ *_depl = nullptr)
        : DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(
                getStubFunctor, dbusSignature, _depl),
          DBusSetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(
                getStubFunctor, onRemoteSetFunctor, onRemoteChangedFunctor, dbusSignature, _depl),
          fireChangedFunctor_(fireChangedFunctor) {
    }

    virtual ~DBusSetObservableAttributeStubDispatcher() {};

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusStubAdapterHelperType& dbusStubAdapterHelper) {
        bool attributeValueChanged;
        if (!this->setAttributeValue(dbusMessage, stub, dbusStubAdapterHelper, attributeValueChanged))
            return false;

        if (attributeValueChanged) {
            std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));
            fireAttributeValueChanged(clientId, dbusStubAdapterHelper, stub);
            this->notifyOnRemoteChanged(dbusStubAdapterHelper);
        }
        return true;
    }
protected:
    virtual void fireAttributeValueChanged(std::shared_ptr<CommonAPI::ClientId> _client,
                                           DBusStubAdapterHelperType &_helper,
                                           const std::shared_ptr<StubClass_> _stub) {
        (void)_helper;
        (_stub->StubType::getStubAdapter().get()->*fireChangedFunctor_)(this->getAttributeValue(_client, _stub));
    }

    const FireChangedFunctor fireChangedFunctor_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSSTUBADAPTERHELPER_HPP_

