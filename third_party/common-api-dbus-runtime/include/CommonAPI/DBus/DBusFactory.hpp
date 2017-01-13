// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_FACTORY_HPP_
#define COMMONAPI_DBUS_FACTORY_HPP_

#include <map>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Factory.hpp>
#include <CommonAPI/DBus/DBusTypes.hpp>

namespace CommonAPI {
namespace DBus {

class DBusAddress;
class DBusProxy;
class DBusProxyConnection;
class DBusStubAdapter;

typedef std::shared_ptr<DBusProxy>
(*ProxyCreateFunction)(const DBusAddress &_address,
                       const std::shared_ptr<DBusProxyConnection> &_connection);

typedef std::shared_ptr<DBusStubAdapter>
(*StubAdapterCreateFunction) (const DBusAddress &_address,
                              const std::shared_ptr<DBusProxyConnection> &_connection,
                              const std::shared_ptr<StubBase> &_stub);

class Factory : public CommonAPI::Factory {
public:
    COMMONAPI_EXPORT static std::shared_ptr<Factory> get();

    COMMONAPI_EXPORT Factory();
    COMMONAPI_EXPORT virtual ~Factory();

    COMMONAPI_EXPORT void registerProxyCreateMethod(const std::string &_address,
                                    ProxyCreateFunction _function);

    COMMONAPI_EXPORT void registerStubAdapterCreateMethod(const std::string &_address,
                                         StubAdapterCreateFunction _function);


    COMMONAPI_EXPORT std::shared_ptr<Proxy> createProxy(const std::string &_domain,
                                       const std::string &_interface,
                                       const std::string &_instance,
                                       const ConnectionId_t &_connectionId);

    COMMONAPI_EXPORT std::shared_ptr<Proxy> createProxy(const std::string &_domain,
                                       const std::string &_interface,
                                       const std::string &_instance,
                                       std::shared_ptr<MainLoopContext> _context);

    COMMONAPI_EXPORT bool registerStub(const std::string &_domain,
                                const std::string &_interface,
                                const std::string &_instance,
                          std::shared_ptr<StubBase> _stub,
                          const ConnectionId_t &_connectionId);

    COMMONAPI_EXPORT bool registerStub(const std::string &_domain,
                            const std::string &_interface,
                            const std::string &_instance,
                      std::shared_ptr<StubBase> _stub,
                      std::shared_ptr<MainLoopContext> _context);

    COMMONAPI_EXPORT bool unregisterStub(const std::string &_domain,
                        const std::string &_interface, 
                        const std::string &_instance);

    // Services
    COMMONAPI_EXPORT std::shared_ptr<DBusStubAdapter> getRegisteredService(const std::string &_address);

    // Managed services
    COMMONAPI_EXPORT std::shared_ptr<DBusStubAdapter> createDBusStubAdapter(const std::shared_ptr<StubBase> &_stub,
                                                           const std::string &_interface,
                                                           const DBusAddress &_address,
                                                           const std::shared_ptr<DBusProxyConnection> &_connection);
    COMMONAPI_EXPORT bool registerManagedService(const std::shared_ptr<DBusStubAdapter> &_adapter);
    COMMONAPI_EXPORT bool unregisterManagedService(const std::string &_address);

private:
    COMMONAPI_EXPORT std::shared_ptr<DBusConnection> getConnection(const ConnectionId_t &);
    COMMONAPI_EXPORT std::shared_ptr<DBusConnection> getConnection(std::shared_ptr<MainLoopContext>);
    COMMONAPI_EXPORT bool registerStubAdapter(std::shared_ptr<DBusStubAdapter>);
    COMMONAPI_EXPORT bool unregisterStubAdapter(std::shared_ptr<DBusStubAdapter>);

    // Managed services
    typedef std::unordered_map<std::string, std::shared_ptr<DBusStubAdapter>> ServicesMap;
    COMMONAPI_EXPORT bool unregisterManagedService(const ServicesMap::iterator &);

private:
    static std::shared_ptr<Factory> theFactory;

    std::map<ConnectionId_t, std::shared_ptr<DBusConnection>> connections_;
    std::map<MainLoopContext *, std::shared_ptr<DBusConnection>> contextConnections_;

    std::map<std::string, ProxyCreateFunction> proxyCreateFunctions_;
    std::map<std::string, StubAdapterCreateFunction> stubAdapterCreateFunctions_;

    ServicesMap services_;

    DBusType_t dBusBusType_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_FACTORY_HPP_
