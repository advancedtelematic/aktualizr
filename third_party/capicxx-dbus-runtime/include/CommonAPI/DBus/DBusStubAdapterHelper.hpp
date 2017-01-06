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

template <typename StubClass_>
class StubDispatcher {
public:

    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    virtual ~StubDispatcher() {}
    virtual bool dispatchDBusMessage(const DBusMessage &_message,
                                     const std::shared_ptr<StubClass_> &_stub,
                                     RemoteEventHandlerType* _remoteEventHandler,
                                     std::weak_ptr<DBusProxyConnection> _connection_) = 0;
    virtual void appendGetAllReply(const DBusMessage &_message,
                                   const std::shared_ptr<StubClass_> &_stub,
                                   DBusOutputStream &_output) {
        (void)_message;
        (void)_stub;
        (void)_output;
    }
};

template <typename StubClass_>
struct DBusAttributeDispatcherStruct {
    StubDispatcher<StubClass_>* getter;
    StubDispatcher<StubClass_>* setter;

    DBusAttributeDispatcherStruct(StubDispatcher<StubClass_>* g, StubDispatcher<StubClass_>* s) {
        getter = g;
        setter = s;
    }
};

template <typename T>
struct identity { typedef T type; };

// interfaceMemberName, interfaceMemberSignature
typedef std::pair<const char*, const char*> DBusInterfaceMemberPath;

template <typename... Stubs_>
class DBusStubAdapterHelper {
public:
  DBusStubAdapterHelper(const DBusAddress &_address,
                        const std::shared_ptr<DBusProxyConnection> &_connection,
                        const bool _isManaging,
                        const std::shared_ptr<StubBase> &_stub) {
    (void)_address;
    (void)_connection;
    (void) _isManaging;
    (void) _stub;
  }
protected:
  bool findDispatcherAndHandle(const DBusMessage& dbusMessage, DBusInterfaceMemberPath& dbusInterfaceMemberPath) {
    (void) dbusMessage;
    (void) dbusInterfaceMemberPath;
    return false;
  }
  bool findAttributeGetDispatcherAndHandle(std::string interfaceName, std::string attributeName, const DBusMessage &_message) {
    (void) interfaceName;
    (void) attributeName;
    (void) _message;
    return false;
  }
  bool findAttributeSetDispatcherAndHandle(std::string interfaceName, std::string attributeName, const DBusMessage &_message) {
    (void) interfaceName;
    (void) attributeName;
    (void) _message;
    return false;
  }
  bool appendGetAllReply(const DBusMessage& dbusMessage, DBusOutputStream& dbusOutputStream) {
    (void) dbusMessage;
    (void) dbusOutputStream;
    return true;
  }
public:
  template <typename Stub_>
  void addStubDispatcher(DBusInterfaceMemberPath _dbusInterfaceMemberPath,
                         StubDispatcher<Stub_>* _stubDispatcher) {
    (void) _dbusInterfaceMemberPath;
    (void) _stubDispatcher;
  }
  template <typename RemoteEventHandlerType>
  void setRemoteEventHandler(RemoteEventHandlerType * _remoteEventHandler) {
    (void) _remoteEventHandler;
  }

};

template <typename StubClass_, typename... Stubs_>
class DBusStubAdapterHelper<StubClass_, Stubs_...>:
 public virtual DBusStubAdapter,
 public DBusStubAdapterHelper<Stubs_...> {
 public:
    typedef typename StubClass_::StubAdapterType StubAdapterType;
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    typedef std::unordered_map<DBusInterfaceMemberPath, StubDispatcher<StubClass_>*> StubDispatcherTable;
    typedef std::unordered_map<std::string, DBusAttributeDispatcherStruct<StubClass_>> StubAttributeTable;

    DBusStubAdapterHelper(const DBusAddress &_address,
                          const std::shared_ptr<DBusProxyConnection> &_connection,
                          const bool _isManaging,
                          const std::shared_ptr<StubBase> &_stub) :

                    DBusStubAdapter(_address, _connection, _isManaging),
                    DBusStubAdapterHelper<Stubs_...>(_address, _connection, _isManaging, _stub),
                    remoteEventHandler_(nullptr) {
                    stub_ = std::dynamic_pointer_cast<StubClass_>(_stub);
    }

    virtual ~DBusStubAdapterHelper() {
        DBusStubAdapter::deinit();
        stub_.reset();
    }

    virtual void init(std::shared_ptr<DBusStubAdapter> instance) {
        DBusStubAdapter::init(instance);
        std::shared_ptr<StubAdapterType> stubAdapter = std::dynamic_pointer_cast<StubAdapterType>(instance);
        remoteEventHandler_ = stub_->initStubAdapter(stubAdapter);
        DBusStubAdapterHelper<Stubs_...>::setRemoteEventHandler(remoteEventHandler_);
    }

    void setRemoteEventHandler(RemoteEventHandlerType* _remoteEventHandler) {
      remoteEventHandler_ = _remoteEventHandler;
      DBusStubAdapterHelper<Stubs_...>::setRemoteEventHandler(remoteEventHandler_);
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

        if (NULL == interfaceMemberName) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " member empty");
        }
        if (NULL == interfaceMemberSignature) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " signature empty");
        }

        DBusInterfaceMemberPath dbusInterfaceMemberPath = {interfaceMemberName, interfaceMemberSignature};
        return findDispatcherAndHandle(dbusMessage, dbusInterfaceMemberPath);
    }

    bool findDispatcherAndHandle(const DBusMessage& dbusMessage, DBusInterfaceMemberPath& dbusInterfaceMemberPath) {
        auto findIterator = stubDispatcherTable_.find(dbusInterfaceMemberPath);
        const bool foundInterfaceMemberHandler = (findIterator != stubDispatcherTable_.end());
        bool dbusMessageHandled = false;
        if (foundInterfaceMemberHandler) {
            StubDispatcher<StubClass_>* stubDispatcher = findIterator->second;
            dbusMessageHandled = stubDispatcher->dispatchDBusMessage(dbusMessage, stub_, getRemoteEventHandler(), getDBusConnection());
            return dbusMessageHandled;
        }

        return DBusStubAdapterHelper<Stubs_...>::findDispatcherAndHandle(dbusMessage, dbusInterfaceMemberPath);
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

    template <typename Stub_>
    void addStubDispatcher(DBusInterfaceMemberPath _dbusInterfaceMemberPath,
                           StubDispatcher<Stub_>* _stubDispatcher) {
        addStubDispatcher(_dbusInterfaceMemberPath, _stubDispatcher, identity<Stub_>());
    }

    template <typename Stub_>
    void addAttributeDispatcher(std::string _key,
                                StubDispatcher<Stub_>* _stubDispatcherGetter,
                                StubDispatcher<Stub_>* _stubDispatcherSetter) {
        addAttributeDispatcher(_key, _stubDispatcherGetter, _stubDispatcherSetter, identity<Stub_>());
    }

    std::shared_ptr<StubClass_> stub_;
    RemoteEventHandlerType* remoteEventHandler_;
    StubDispatcherTable stubDispatcherTable_;
    StubAttributeTable stubAttributeTable_;

protected:

    bool handleFreedesktopGet(const DBusMessage &_message, DBusInputStream &_input) {
        std::string interfaceName;
        std::string attributeName;
        _input >> interfaceName;
        _input >> attributeName;

        if (_input.hasError()) {
            return false;
        }
        return findAttributeGetDispatcherAndHandle(interfaceName, attributeName, _message);
    }

    bool findAttributeGetDispatcherAndHandle(std::string interfaceName, std::string attributeName, const DBusMessage &_message) {

        auto attributeDispatcherIterator = stubAttributeTable_.find(attributeName);
        if (attributeDispatcherIterator == stubAttributeTable_.end()) {
            // not found, try parent
            return DBusStubAdapterHelper<Stubs_...>::findAttributeGetDispatcherAndHandle(interfaceName, attributeName, _message);
        }

        StubDispatcher<StubClass_>* getterDispatcher = attributeDispatcherIterator->second.getter;
        if (NULL == getterDispatcher) { // all attributes have at least a getter
            COMMONAPI_ERROR(std::string(__FUNCTION__), "getterDispatcher == NULL");
            return false;
        } else {
            return getterDispatcher->dispatchDBusMessage(_message, stub_, getRemoteEventHandler(), getDBusConnection());
        }
    }

    bool handleFreedesktopSet(const DBusMessage& dbusMessage, DBusInputStream& dbusInputStream) {
        std::string interfaceName;
        std::string attributeName;
        dbusInputStream >> interfaceName;
        dbusInputStream >> attributeName;

        if(dbusInputStream.hasError()) {
            return false;
        }

        return findAttributeSetDispatcherAndHandle(interfaceName, attributeName, dbusMessage);
    }

    bool findAttributeSetDispatcherAndHandle(std::string interfaceName, std::string attributeName, const DBusMessage& dbusMessage) {

        auto attributeDispatcherIterator = stubAttributeTable_.find(attributeName);
        if(attributeDispatcherIterator == stubAttributeTable_.end()) {
          // not found, try parent
          return DBusStubAdapterHelper<Stubs_...>::findAttributeSetDispatcherAndHandle(interfaceName, attributeName, dbusMessage);

        }

        StubDispatcher<StubClass_> *setterDispatcher = attributeDispatcherIterator->second.setter;
        if (setterDispatcher == NULL) { // readonly attributes do not have a setter
            return false;
        }

        return setterDispatcher->dispatchDBusMessage(dbusMessage, stub_, getRemoteEventHandler(), getDBusConnection());
    }

    bool appendGetAllReply(const DBusMessage& dbusMessage, DBusOutputStream& dbusOutputStream)
    {
        for(auto attributeDispatcherIterator = stubAttributeTable_.begin(); attributeDispatcherIterator != stubAttributeTable_.end(); attributeDispatcherIterator++) {

            //To prevent the destruction of the stub whilst still handling a message
            if (stub_) {
                StubDispatcher<StubClass_>* getterDispatcher = attributeDispatcherIterator->second.getter;
                if (NULL == getterDispatcher) { // all attributes have at least a getter
                    COMMONAPI_ERROR(std::string(__FUNCTION__), "getterDispatcher == NULL");
                    return false;
                } else {
                    dbusOutputStream.align(8);
                    dbusOutputStream << attributeDispatcherIterator->first;
                    getterDispatcher->appendGetAllReply(dbusMessage, stub_, dbusOutputStream);
                }
            }
        }
        return DBusStubAdapterHelper<Stubs_...>::appendGetAllReply(dbusMessage, dbusOutputStream);
     }

 private:

   template <typename Stub_>
   void addStubDispatcher(DBusInterfaceMemberPath _dbusInterfaceMemberPath,
                          StubDispatcher<Stub_>* _stubDispatcher,
                          identity<Stub_>) {
       DBusStubAdapterHelper<Stubs_...>::addStubDispatcher(_dbusInterfaceMemberPath, _stubDispatcher);

   }

   void addStubDispatcher(DBusInterfaceMemberPath _dbusInterfaceMemberPath,
                          StubDispatcher<StubClass_>* _stubDispatcher,
                          identity<StubClass_>) {
       stubDispatcherTable_.insert({_dbusInterfaceMemberPath, _stubDispatcher});

   }

   template <typename Stub_>
   void addAttributeDispatcher(std::string _key,
                          StubDispatcher<Stub_>* _stubDispatcherGetter,
                          StubDispatcher<Stub_>* _stubDispatcherSetter,
                          identity<Stub_>) {
       DBusStubAdapterHelper<Stubs_...>::addAttributeDispatcher(_key, _stubDispatcherGetter, _stubDispatcherSetter);

   }

   void addAttributeDispatcher(std::string _key,
                          StubDispatcher<StubClass_>* _stubDispatcherGetter,
                          StubDispatcher<StubClass_>* _stubDispatcherSetter,
                          identity<StubClass_>) {
       stubAttributeTable_.insert({_key, {_stubDispatcherGetter, _stubDispatcherSetter}});
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
         appendGetAllReply(dbusMessage, dbusOutputStream);
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

class DBusMethodStubDispatcher<StubClass_, In_<InArgs_...>, DeplIn_<DeplIn_Args...> >: public StubDispatcher<StubClass_> {
 public:

    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_...);

    DBusMethodStubDispatcher(StubFunctor_ stubFunctor, std::tuple<DeplIn_Args*...> _in):
            stubFunctor_(stubFunctor) {
            initialize(typename make_sequence_range<sizeof...(DeplIn_Args), 0>::type(), _in);
    }

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub,
          RemoteEventHandlerType* _remoteEventHandler,
          std::weak_ptr<DBusProxyConnection> _connection) {
            (void) _remoteEventHandler;
            (void) _connection;
        return handleDBusMessage(dbusMessage, stub, typename make_sequence_range<sizeof...(InArgs_), 0>::type());
    }

 private:
    template <int... DeplIn_ArgIndices>
    inline void initialize(index_sequence<DeplIn_ArgIndices...>, std::tuple<DeplIn_Args*...> &_in) {
        in_ = std::make_tuple(std::get<DeplIn_ArgIndices>(_in)...);
    }

    template <int... InArgIndices_>
    inline bool handleDBusMessage(const DBusMessage& dbusMessage,
                                  const std::shared_ptr<StubClass_>& stub,
                                  index_sequence<InArgIndices_...>) {

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

template< class, class, class, class, class...>
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
       DeplOut_<DeplOutArgs_...>>:
            public StubDispatcher<StubClass_> {
 public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;

    typedef void (StubClass_::*StubFunctor_)(
                std::shared_ptr<CommonAPI::ClientId>, InArgs_..., ReplyType_t);

    DBusMethodWithReplyStubDispatcher(StubFunctor_ _stubFunctor,
        const char* _dbusReplySignature,
        const std::tuple<DeplIn_Args*...>& _inDepArgs,
        const std::tuple<DeplOutArgs_*...>& _outDepArgs):
            out_(_outDepArgs),
            currentCall_(0),
            stubFunctor_(_stubFunctor),
            dbusReplySignature_(_dbusReplySignature) {

        initialize(typename make_sequence_range<sizeof...(DeplIn_Args), 0>::type(), _inDepArgs);
    }

    bool dispatchDBusMessage(const DBusMessage& _dbusMessage,
                             const std::shared_ptr<StubClass_>& _stub,
                             RemoteEventHandlerType* _remoteEventHandler,
                             std::weak_ptr<DBusProxyConnection> _connection) {
        (void) _remoteEventHandler;
        connection_ = _connection;
        return handleDBusMessage(
                _dbusMessage,
                _stub,
                typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                typename make_sequence_range<sizeof...(OutArgs_), 0>::type());
    }

    bool sendReply(const CommonAPI::CallId_t _call,
                   const std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> args = std::make_tuple()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto dbusMessage = pending_.find(_call);
            if(dbusMessage != pending_.end()) {
                DBusMessage reply = dbusMessage->second.createMethodReturn(dbusReplySignature_);
                pending_[_call] = reply;
            } else {
                return false;
            }
        }
        return sendReplyInternal(_call, typename make_sequence_range<sizeof...(OutArgs_), 0>::type(), args);
    }

protected:

    std::tuple<CommonAPI::Deployable<InArgs_, DeplIn_Args>...> in_;
    std::tuple<DeplOutArgs_*...> out_;
    CommonAPI::CallId_t currentCall_;
    std::map<CommonAPI::CallId_t, DBusMessage> pending_;
    std::mutex mutex_; // protects pending_

    std::weak_ptr<DBusProxyConnection> connection_;

private:

    template <int... DeplIn_ArgIndices>
    inline void initialize(index_sequence<DeplIn_ArgIndices...>, const std::tuple<DeplIn_Args*...>& _in) {
        in_ = std::make_tuple(std::get<DeplIn_ArgIndices>(_in)...);
    }

    template <int... InArgIndices_, int... OutArgIndices_>
    inline bool handleDBusMessage(const DBusMessage& _dbusMessage,
                                  const std::shared_ptr<StubClass_>& _stub,
                                  index_sequence<InArgIndices_...>,
                                  index_sequence<OutArgIndices_...>) {
        if (sizeof...(DeplIn_Args) > 0) {
            DBusInputStream dbusInputStream(_dbusMessage);
            const bool success = DBusSerializableArguments<CommonAPI::Deployable<InArgs_, DeplIn_Args>...>::deserialize(dbusInputStream, std::get<InArgIndices_>(in_)...);
            if (!success)
                return false;
        }

        std::shared_ptr<DBusClientId> clientId
            = std::make_shared<DBusClientId>(std::string(_dbusMessage.getSender()));

        CommonAPI::CallId_t call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            call = currentCall_++;
            pending_[call] = _dbusMessage;
        }

        (_stub.get()->*stubFunctor_)(
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
    bool sendReplyInternal(const CommonAPI::CallId_t _call,
                           index_sequence<OutArgIndices_...>,
                           const std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>& _args) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto reply = pending_.find(_call);
        if (reply != pending_.end()) {
            if (sizeof...(DeplOutArgs_) > 0) {
                DBusOutputStream output(reply->second);
                if (!DBusSerializableArguments<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>::serialize(
                        output, std::get<OutArgIndices_>(_args)...)) {
                    (void)_args;
                    pending_.erase(_call);
                    return false;
                }
                output.flush();
            }
            if (std::shared_ptr<DBusProxyConnection> connection = connection_.lock()) {
                bool isSuccessful = connection->sendDBusMessage(reply->second);
                pending_.erase(_call);
                return isSuccessful;
            }
            else {
                return false;
            }
        }
        return false;
    }

    StubFunctor_ stubFunctor_;
    const char* dbusReplySignature_;
};

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_,
    template <class...> class DeplIn_, class... DeplIn_Args,
    template <class...> class DeplOut_, class... DeplOutArgs_,
    class... ErrorReplies_>

class DBusMethodWithReplyStubDispatcher<
       StubClass_,
       In_<InArgs_...>,
       Out_<OutArgs_...>,
       DeplIn_<DeplIn_Args...>,
       DeplOut_<DeplOutArgs_...>,
       ErrorReplies_...> :
            public DBusMethodWithReplyStubDispatcher<
                StubClass_,
                In_<InArgs_...>,
                Out_<OutArgs_...>,
                DeplIn_<DeplIn_Args...>,
                DeplOut_<DeplOutArgs_...>> {
 public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;

    typedef void (StubClass_::*StubFunctor_)(
                std::shared_ptr<CommonAPI::ClientId>, CommonAPI::CallId_t, InArgs_..., ReplyType_t, ErrorReplies_...);

    DBusMethodWithReplyStubDispatcher(StubFunctor_ _stubFunctor,
                                      const char* _dbusReplySignature,
                                      const std::tuple<DeplIn_Args*...>& _inDepArgs,
                                      const std::tuple<DeplOutArgs_*...>& _outDepArgs,
                                      const ErrorReplies_... _errorReplies) :
                                          DBusMethodWithReplyStubDispatcher<
                                              StubClass_,
                                              In_<InArgs_...>,
                                              Out_<OutArgs_...>,
                                              DeplIn_<DeplIn_Args...>,
                                              DeplOut_<DeplOutArgs_...>>(
                                                      NULL,
                                                      _dbusReplySignature,
                                                      _inDepArgs,
                                                      _outDepArgs),
                                          stubFunctor_(_stubFunctor),
                                          errorReplies_(std::make_tuple(_errorReplies...)) { }

    bool dispatchDBusMessage(const DBusMessage& _dbusMessage,
                             const std::shared_ptr<StubClass_>& _stub,
                             RemoteEventHandlerType* _remoteEventHandler,
                             std::weak_ptr<DBusProxyConnection> _connection) {
        (void) _remoteEventHandler;
        this->connection_ = _connection;
        return handleDBusMessage(
                _dbusMessage,
                _stub,
                typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                typename make_sequence_range<sizeof...(OutArgs_), 0>::type(),
                typename make_sequence_range<sizeof...(ErrorReplies_), 0>::type());
    }

    template <class... ErrorReplyOutArgs_, class... ErrorReplyDeplOutArgs_>
    bool sendErrorReply(const CommonAPI::CallId_t _call,
                        const std::string &_signature,
                        const std::string &_errorName,
                        const std::tuple<CommonAPI::Deployable<ErrorReplyOutArgs_, ErrorReplyDeplOutArgs_>...>& _args) {
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            auto dbusMessage = this->pending_.find(_call);
            if(dbusMessage != this->pending_.end()) {
                DBusMessage reply = dbusMessage->second.createMethodError(_errorName, _signature);
                this->pending_[_call] = reply;
            } else {
                return false;
            }
        }
        return sendErrorReplyInternal(_call, typename make_sequence_range<sizeof...(ErrorReplyOutArgs_), 0>::type(), _args);
    }

private:

    template <int... InArgIndices_, int... OutArgIndices_, int... ErrorRepliesIndices_>
    inline bool handleDBusMessage(const DBusMessage& _dbusMessage,
                                  const std::shared_ptr<StubClass_>& _stub,
                                  index_sequence<InArgIndices_...>,
                                  index_sequence<OutArgIndices_...>,
                                  index_sequence<ErrorRepliesIndices_...>) {
        if (sizeof...(DeplIn_Args) > 0) {
            DBusInputStream dbusInputStream(_dbusMessage);
            const bool success = DBusSerializableArguments<CommonAPI::Deployable<InArgs_, DeplIn_Args>...>::deserialize(dbusInputStream, std::get<InArgIndices_>(this->in_)...);
            if (!success)
                return false;
        }

        std::shared_ptr<DBusClientId> clientId
            = std::make_shared<DBusClientId>(std::string(_dbusMessage.getSender()));

        CommonAPI::CallId_t call;
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            call = this->currentCall_++;
            this->pending_[call] = _dbusMessage;
        }

        (_stub.get()->*stubFunctor_)(
            clientId,
            call,
            std::move(std::get<InArgIndices_>(this->in_).getValue())...,
            [call, this](OutArgs_... _args){
                this->sendReply(call, std::make_tuple(CommonAPI::Deployable<OutArgs_, DeplOutArgs_>(
                            _args, std::get<OutArgIndices_>(this->out_)
                        )...));
            },
            std::get<ErrorRepliesIndices_>(errorReplies_)...
        );

        return true;
    }

    template<int... ErrorReplyOutArgIndices_, class... ErrorReplyOutArgs_, class ...ErrorReplyDeplOutArgs_>
    bool sendErrorReplyInternal(CommonAPI::CallId_t _call,
                           index_sequence<ErrorReplyOutArgIndices_...>,
                           const std::tuple<CommonAPI::Deployable<ErrorReplyOutArgs_, ErrorReplyDeplOutArgs_>...>& _args) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        auto reply = this->pending_.find(_call);
        if (reply != this->pending_.end()) {
            if (sizeof...(ErrorReplyDeplOutArgs_) > 0) {
                DBusOutputStream output(reply->second);
                if (!DBusSerializableArguments<CommonAPI::Deployable<ErrorReplyOutArgs_, ErrorReplyDeplOutArgs_>...>::serialize(
                        output, std::get<ErrorReplyOutArgIndices_>(_args)...)) {
                    (void)_args;
                    this->pending_.erase(_call);
                    return false;
                }
                output.flush();
            }
            if (std::shared_ptr<DBusProxyConnection> connection = this->connection_.lock()) {
                bool isSuccessful = connection->sendDBusMessage(reply->second);
                this->pending_.erase(_call);
                return isSuccessful;
            }
            else {
                return false;
            }
        }
        return false;
    }

    StubFunctor_ stubFunctor_;
    std::tuple<ErrorReplies_...> errorReplies_;
};

template< class, class, class, class >
class DBusMethodWithReplyAdapterDispatcher;

template <
    typename StubClass_,
    typename StubAdapterClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_>
class DBusMethodWithReplyAdapterDispatcher<StubClass_, StubAdapterClass_, In_<InArgs_...>, Out_<OutArgs_...> >:
            public StubDispatcher<StubClass_> {
 public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef void (StubAdapterClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., OutArgs_&...);
    typedef typename CommonAPI::Stub<typename StubClass_::StubAdapterType, typename StubClass_::RemoteEventType> StubType;

    DBusMethodWithReplyAdapterDispatcher(StubFunctor_ stubFunctor, const char* dbusReplySignature):
            stubFunctor_(stubFunctor),
            dbusReplySignature_(dbusReplySignature) {
    }

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub,
        RemoteEventHandlerType* _remoteEventHandler,
        std::weak_ptr<DBusProxyConnection> _connection) {

        (void)_remoteEventHandler;

        std::tuple<InArgs_..., OutArgs_...> argTuple;
        return handleDBusMessage(
                        dbusMessage,
                        stub,
                        _connection,
                        typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                        typename make_sequence_range<sizeof...(OutArgs_), sizeof...(InArgs_)>::type(),argTuple);
    }

 private:
    template <int... InArgIndices_, int... OutArgIndices_>
    inline bool handleDBusMessage(const DBusMessage& dbusMessage,
                                  const std::shared_ptr<StubClass_>& stub,
                                  std::weak_ptr<DBusProxyConnection> _connection,
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
        if (std::shared_ptr<DBusProxyConnection> connection = _connection.lock()) {
            bool isSuccessful = connection->sendDBusMessage(dbusMessageReply);
            return isSuccessful;
        }
        else {
            return false;
        }
    }

    StubFunctor_ stubFunctor_;
    const char* dbusReplySignature_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusGetAttributeStubDispatcher: public virtual StubDispatcher<StubClass_> {
 public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef const AttributeType_& (StubClass_::*GetStubFunctor)(std::shared_ptr<CommonAPI::ClientId>);

    DBusGetAttributeStubDispatcher(GetStubFunctor _getStubFunctor, const char *_signature, AttributeDepl_ *_depl = nullptr):
        getStubFunctor_(_getStubFunctor),
        signature_(_signature),
        depl_(_depl) {
    }

    virtual ~DBusGetAttributeStubDispatcher() {};

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::weak_ptr<DBusProxyConnection> _connection) {
        (void) _remoteEventHandler;
        return sendAttributeValueReply(dbusMessage, stub, _connection);
    }

    void appendGetAllReply(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, DBusOutputStream &_output) {

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));
        auto varDepl = CommonAPI::DBus::VariantDeployment<AttributeDepl_>(true, depl_); // presuming FreeDesktop variant deployment, as support for "legacy" service only
        _output << CommonAPI::Deployable<CommonAPI::Variant<AttributeType_>, CommonAPI::DBus::VariantDeployment<AttributeDepl_>>((stub.get()->*getStubFunctor_)(clientId), &varDepl);
        _output.flush();
    }

 protected:
    virtual bool sendAttributeValueReply(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub, std::weak_ptr<DBusProxyConnection> connection_) {
        DBusMessage dbusMessageReply = dbusMessage.createMethodReturn(signature_);
        DBusOutputStream dbusOutputStream(dbusMessageReply);

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));

        dbusOutputStream << CommonAPI::Deployable<AttributeType_, AttributeDepl_>((stub.get()->*getStubFunctor_)(clientId), depl_);
        dbusOutputStream.flush();
        if (std::shared_ptr<DBusProxyConnection> connection = connection_.lock()) {
            bool isSuccessful = connection->sendDBusMessage(dbusMessageReply);
            return isSuccessful;
        }
        else {
            return false;
        }
    }


    GetStubFunctor getStubFunctor_;
    const char* signature_;
    AttributeDepl_ *depl_;
};

template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class DBusSetAttributeStubDispatcher: public virtual DBusGetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
 public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

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

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::weak_ptr<DBusProxyConnection> _connection) {
        bool attributeValueChanged;

        if (!setAttributeValue(dbusMessage, stub, _remoteEventHandler, _connection, attributeValueChanged))
            return false;

        if (attributeValueChanged)
            notifyOnRemoteChanged(_remoteEventHandler);

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
                                  RemoteEventHandlerType* _remoteEventHandler,
                                  std::weak_ptr<DBusProxyConnection> _connection,
                                  bool& attributeValueChanged) {
        bool errorOccured;
        CommonAPI::Deployable<AttributeType_, AttributeDepl_> attributeValue(
             retrieveAttributeValue(dbusMessage, errorOccured), this->depl_);

        if(errorOccured) {
            return false;
        }

        std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));

        attributeValueChanged = (_remoteEventHandler->*onRemoteSetFunctor_)(clientId, std::move(attributeValue.getValue()));

        return this->sendAttributeValueReply(dbusMessage, stub, _connection);
    }

    inline void notifyOnRemoteChanged(RemoteEventHandlerType* _remoteEventHandler) {
        (_remoteEventHandler->*onRemoteChangedFunctor_)();
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
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef typename StubClass_::StubAdapterType StubAdapterType;
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

    bool dispatchDBusMessage(const DBusMessage& dbusMessage, const std::shared_ptr<StubClass_>& stub,
        RemoteEventHandlerType* _remoteEventHandler,
        std::weak_ptr<DBusProxyConnection> _connection) {
        bool attributeValueChanged;
        if (!this->setAttributeValue(dbusMessage, stub, _remoteEventHandler, _connection, attributeValueChanged))
            return false;

        if (attributeValueChanged) {
            std::shared_ptr<DBusClientId> clientId = std::make_shared<DBusClientId>(std::string(dbusMessage.getSender()));
            fireAttributeValueChanged(clientId, _remoteEventHandler, stub);
            this->notifyOnRemoteChanged(_remoteEventHandler);
        }
        return true;
    }
protected:
    virtual void fireAttributeValueChanged(std::shared_ptr<CommonAPI::ClientId> _client,
                                           RemoteEventHandlerType* _remoteEventHandler,
                                           const std::shared_ptr<StubClass_> _stub) {
        (void)_remoteEventHandler;
        (_stub->StubType::getStubAdapter().get()->*fireChangedFunctor_)(this->getAttributeValue(_client, _stub));
    }

    const FireChangedFunctor fireChangedFunctor_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSSTUBADAPTERHELPER_HPP_
