// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <sstream>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Runtime.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusFactory.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusStubAdapter.hpp>

namespace CommonAPI {
namespace DBus {

INITIALIZER(FactoryInit) {
    Runtime::get()->registerFactory("dbus", Factory::get());
}

std::shared_ptr<CommonAPI::DBus::Factory>
Factory::get() {
    static std::shared_ptr<Factory> theFactory = std::make_shared<Factory>();
    return theFactory;
}

Factory::Factory() {
}

Factory::~Factory() {
}

void
Factory::registerProxyCreateMethod(
    const std::string &_interface, ProxyCreateFunction _function) {
    proxyCreateFunctions_[_interface] = _function;
}

void
Factory::registerStubAdapterCreateMethod(
    const std::string &_interface, StubAdapterCreateFunction _function) {
    stubAdapterCreateFunctions_[_interface] = _function;
}

std::shared_ptr<Proxy>
Factory::createProxy(
        const std::string &_domain, const std::string &_interface, const std::string &_instance,
        const ConnectionId_t &_connectionId) {
    auto proxyCreateFunctionsIterator = proxyCreateFunctions_.find(_interface);
    if (proxyCreateFunctionsIterator != proxyCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        DBusAddress dbusAddress;
        
        if (DBusAddressTranslator::get()->translate(address, dbusAddress)) {
            std::shared_ptr<DBusProxy> proxy
                = proxyCreateFunctionsIterator->second(dbusAddress, getConnection(_connectionId));
            if (proxy)
                proxy->init();
            return proxy;
        }
    }
    return nullptr;
}

std::shared_ptr<Proxy>
Factory::createProxy(
    const std::string &_domain, const std::string &_interface, const std::string &_instance,
    std::shared_ptr<MainLoopContext> _context) {

    auto proxyCreateFunctionsIterator = proxyCreateFunctions_.find(_interface);
    if (proxyCreateFunctionsIterator != proxyCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        DBusAddress dbusAddress;
        
        if (DBusAddressTranslator::get()->translate(address, dbusAddress)) {
            std::shared_ptr<DBusProxy> proxy
                = proxyCreateFunctionsIterator->second(dbusAddress, getConnection(_context));
            if (proxy)
                proxy->init();
            return proxy;
        }
    }

    return nullptr;
}

bool
Factory::registerStub(
        const std::string &_domain, const std::string &_interface, const std::string &_instance,
        std::shared_ptr<StubBase> _stub, const ConnectionId_t &_connectionId) {
    auto stubAdapterCreateFunctionsIterator = stubAdapterCreateFunctions_.find(_interface);
    if (stubAdapterCreateFunctionsIterator != stubAdapterCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        DBusAddress dbusAddress;
        if (DBusAddressTranslator::get()->translate(address, dbusAddress)) {
            std::shared_ptr<DBusStubAdapter> adapter
                = stubAdapterCreateFunctionsIterator->second(dbusAddress, getConnection(_connectionId), _stub);
            if (adapter) {
                adapter->init(adapter);
                return registerStubAdapter(adapter);
            }
        }
    }

    return false;
}

bool
Factory::registerStub(
        const std::string &_domain, const std::string &_interface, const std::string &_instance,
        std::shared_ptr<StubBase> _stub, std::shared_ptr<MainLoopContext> _context) {
    auto stubAdapterCreateFunctionsIterator = stubAdapterCreateFunctions_.find(_interface);
    if (stubAdapterCreateFunctionsIterator != stubAdapterCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        DBusAddress dbusAddress;
        if (DBusAddressTranslator::get()->translate(address, dbusAddress)) {
            std::shared_ptr<DBusStubAdapter> adapter
                = stubAdapterCreateFunctionsIterator->second(dbusAddress, getConnection(_context), _stub);
            if (adapter) {
                adapter->init(adapter);
                return registerStubAdapter(adapter);
            }
        }
    }
    return false;
}

bool
Factory::unregisterStub(const std::string &_domain, const std::string &_interface, const std::string &_instance) {
    CommonAPI::Address address(_domain, _interface, _instance);
    const auto &adapterResult = services_.find(address.getAddress());
    if (adapterResult != services_.end()) {
        const auto _adapter = adapterResult->second;
        const auto &connection = _adapter->getDBusConnection();
        const auto objectManager = connection->getDBusObjectManager();
        
        if (!objectManager->unregisterDBusStubAdapter(_adapter)) {
            return false;
        }

        if (!connection->releaseServiceName(_adapter->getDBusAddress().getService())) {
            return false;
        }

        if (!unregisterStubAdapter(_adapter)) {
            return false;
        }

        services_.erase(adapterResult->first);

        return true;
    } 

    return false;
}

bool
Factory::registerStubAdapter(std::shared_ptr<DBusStubAdapter> _adapter) {
    CommonAPI::Address address;
    DBusAddress dbusAddress = _adapter->getDBusAddress();
    if (DBusAddressTranslator::get()->translate(dbusAddress, address)) {
        const auto &insertResult = services_.insert( { address.getAddress(), _adapter } );

        const auto &connection = _adapter->getDBusConnection();

        std::shared_ptr<DBusObjectManagerStub> root
            = connection->getDBusObjectManager()->getRootDBusObjectManagerStub();
        if (!root->exportManagedDBusStubAdapter(_adapter)) {
            (void)unregisterManagedService(address.getAddress());
            return false;
        }

        const auto objectManager = connection->getDBusObjectManager();
        if (!objectManager->registerDBusStubAdapter(_adapter)) {
            (void)root->unexportManagedDBusStubAdapter(_adapter);
            (void)unregisterManagedService(address.getAddress());
            services_.erase(insertResult.first);
            return false;
        }

        const bool isServiceNameAcquired
            = connection->requestServiceNameAndBlock(dbusAddress.getService());
        if (!isServiceNameAcquired) {
            (void)root->unexportManagedDBusStubAdapter(_adapter);
            (void)objectManager->unregisterDBusStubAdapter(_adapter);
            (void)unregisterManagedService(address.getAddress());
            services_.erase(insertResult.first);
            return false;
        }

        return true;
    }

    return false;
}

bool
Factory::unregisterStubAdapter(std::shared_ptr<DBusStubAdapter> _adapter) {
    CommonAPI::Address address;
    DBusAddress dbusAddress = _adapter->getDBusAddress();
    if (DBusAddressTranslator::get()->translate(dbusAddress, address)) {
        const auto &connection = _adapter->getDBusConnection();

        std::shared_ptr<DBusObjectManagerStub> root
            = connection->getDBusObjectManager()->getRootDBusObjectManagerStub();
        if (!root->unexportManagedDBusStubAdapter(_adapter)) {
            //(void)unregisterManagedService(address.getAddress());
            return false;
        }

        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Connections
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<DBusConnection>
Factory::getConnection(const ConnectionId_t &_connectionId) {

    auto itsConnectionIterator = connections_.find(_connectionId);
    if (itsConnectionIterator != connections_.end()) {
        return itsConnectionIterator->second;
    }

    // No connection found, lets create and initialize one
    DBusType_t dbusType = DBusAddressTranslator::get()->getDBusBusType(_connectionId);
    std::shared_ptr<DBusConnection> itsConnection
        = std::make_shared<DBusConnection>(dbusType, _connectionId);
    connections_.insert({ _connectionId, itsConnection });

    itsConnection->connect(true);
    return itsConnection;
}

std::shared_ptr<DBusConnection>
Factory::getConnection(std::shared_ptr<MainLoopContext> _context) {
    if (!_context)
        return getConnection(DEFAULT_CONNECTION_ID);

    auto itsConnectionIterator = contextConnections_.find(_context.get());
    if (itsConnectionIterator != contextConnections_.end()) {
        return itsConnectionIterator->second;
    }

    // No connection found, lets create and initialize one
    std::shared_ptr<DBusConnection> itsConnection
        = std::make_shared<DBusConnection>(DBusType_t::SESSION, _context->getName());
    contextConnections_.insert({ _context.get(), itsConnection } );

    itsConnection->connect(false);
    if (_context)
        itsConnection->attachMainLoopContext(_context);

    return itsConnection;
}

///////////////////////////////////////////////////////////////////////////////
// Service registration
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<DBusStubAdapter>
Factory::getRegisteredService(const std::string &_address)  {
    auto serviceIterator = services_.find(_address);
    if (serviceIterator != services_.end()) {
        return serviceIterator->second;
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Managed Services
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<DBusStubAdapter>
Factory::createDBusStubAdapter(
        const std::shared_ptr<StubBase> &_stub,
        const std::string &_interface,
        const DBusAddress &_dbusAddress,
        const std::shared_ptr<DBusProxyConnection> &_connection) {

    std::shared_ptr<DBusStubAdapter> stubAdapter;
    auto stubAdapterCreateFunctionsIterator = stubAdapterCreateFunctions_.find(_interface);
    if (stubAdapterCreateFunctionsIterator != stubAdapterCreateFunctions_.end()) {
        stubAdapter = stubAdapterCreateFunctionsIterator->second(
                        _dbusAddress, _connection, _stub);
        if (stubAdapter)
            stubAdapter->init(stubAdapter);
    }
    return stubAdapter;
}

bool
Factory::registerManagedService(const std::shared_ptr<DBusStubAdapter> &_stubAdapter) {
    auto itsAddress = _stubAdapter->getAddress().getAddress();

    const auto &insertResult = services_.insert( { itsAddress, _stubAdapter} );
    if (insertResult.second) {
        const auto &connection = _stubAdapter->getDBusConnection();
        const auto objectManager = connection->getDBusObjectManager();
        const bool isRegistered = objectManager->registerDBusStubAdapter(_stubAdapter);
        if (!isRegistered) {
            services_.erase(insertResult.first);
            return false;
        }

        const auto &serviceName = _stubAdapter->getDBusAddress().getService();
        const bool isAcquired = connection->requestServiceNameAndBlock(serviceName);
        if (!isAcquired) {
            const bool isDeregistered = objectManager->unregisterDBusStubAdapter(_stubAdapter);
            assert(isDeregistered);
            (void)isDeregistered;

            services_.erase(insertResult.first);
        }

        return isAcquired;
    }

    return false;
}


bool
Factory::unregisterManagedService(const std::string &_address) {
    return unregisterManagedService(services_.find(_address));
}

bool
Factory::unregisterManagedService(const ServicesMap::iterator &iterator) {
    if (iterator == services_.end())
        return true;

    const auto &stubAdapter = iterator->second;
    const auto &connection = stubAdapter->getDBusConnection();
    const auto objectManager = connection->getDBusObjectManager();
    const auto &serviceName = stubAdapter->getDBusAddress().getService();

    const bool isUnregistered
        = objectManager->unregisterDBusStubAdapter(stubAdapter);
    if (isUnregistered) {
        connection->releaseServiceName(serviceName);
        services_.erase(iterator);
    }
    // TODO: log error
    return isUnregistered;
}

} // namespace DBus
} // namespace CommonAPI
