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

static std::weak_ptr<CommonAPI::Runtime> runtime__;

INITIALIZER(FactoryInit) {
    runtime__ = Runtime::get();
    Runtime::get()->registerFactory("dbus", Factory::get());
}

DEINITIALIZER(FactoryDeinit) {
    if (auto rt = runtime__.lock()) {
        rt->unregisterFactory("dbus");
    }
}

std::shared_ptr<CommonAPI::DBus::Factory>
Factory::get() {
    static std::shared_ptr<Factory> theFactory = std::make_shared<Factory>();
    return theFactory;
}

Factory::Factory() : isInitialized_(false) {
}

Factory::~Factory() {
}

void
Factory::init() {
#ifndef WIN32
	std::lock_guard<std::mutex> itsLock(initializerMutex_);
#endif
	if (!isInitialized_) {
		for (auto i : initializers_) i();
		initializers_.clear(); // Not needed anymore
		isInitialized_ = true;
	}
}

void
Factory::registerInterface(InterfaceInitFunction _function) {
#ifndef WIN32
	std::lock_guard<std::mutex> itsLock(initializerMutex_);
#endif
	if (isInitialized_) {
		// We are already running --> initialize the interface library!
		_function();
	} else {
		// We are not initialized --> save the initializer
		initializers_.push_back(_function);
	}
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
            std::shared_ptr<DBusConnection> connection
                = getConnection(_connectionId);
            if (connection) {
                std::shared_ptr<DBusProxy> proxy
                    = proxyCreateFunctionsIterator->second(dbusAddress, connection);
                if (proxy)
                    proxy->init();
                return proxy;
            }
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
            std::shared_ptr<DBusConnection> connection
                = getConnection(_context);
            if (connection) {
                std::shared_ptr<DBusProxy> proxy
                    = proxyCreateFunctionsIterator->second(dbusAddress, connection);
                if (proxy)
                    proxy->init();
                return proxy;
            }
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
            std::shared_ptr<DBusConnection> connection = getConnection(_connectionId);
            if (connection) {
                std::shared_ptr<DBusStubAdapter> adapter
                    = stubAdapterCreateFunctionsIterator->second(dbusAddress, connection, _stub);
                if (adapter) {
                    adapter->init(adapter);
                    if (registerStubAdapter(adapter))
                        return true;
                }
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
            std::shared_ptr<DBusConnection> connection = getConnection(_context);
            if (connection) {
                std::shared_ptr<DBusStubAdapter> adapter
                    = stubAdapterCreateFunctionsIterator->second(dbusAddress, connection, _stub);
                if (adapter) {
                    adapter->init(adapter);
                    if (registerStubAdapter(adapter))
                        return true;
                }
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

        decrementConnection(connection);

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
    std::lock_guard<std::recursive_mutex> itsConnectionGuard(connectionsMutex_);
    std::shared_ptr<DBusConnection> itsConnection;
    auto itsConnectionIterator = connections_.find(_connectionId);
    if (itsConnectionIterator != connections_.end())
        itsConnection = itsConnectionIterator->second;

    if(!itsConnection) {
        // No connection found, lets create and initialize one
        const DBusType_t dbusType = DBusAddressTranslator::get()->getDBusBusType(_connectionId);
        itsConnection = std::make_shared<DBusConnection>(dbusType, _connectionId);

        if (itsConnection) {
            if (!itsConnection->connect(true)) {
                COMMONAPI_ERROR("Failed to create connection ", _connectionId);
                itsConnection.reset();
            } else {
                connections_.insert({ _connectionId, itsConnection } );
            }
        }
    }

    if(itsConnection)
        incrementConnection(itsConnection);

    return itsConnection;
}

std::shared_ptr<DBusConnection>
Factory::getConnection(std::shared_ptr<MainLoopContext> _context) {
    if (!_context)
        return getConnection(DEFAULT_CONNECTION_ID);

    std::lock_guard<std::recursive_mutex> itsConnectionGuard(connectionsMutex_);
    std::shared_ptr<DBusConnection> itsConnection;
    auto itsConnectionIterator = connections_.find(_context->getName());
    if (itsConnectionIterator != connections_.end())
        itsConnection = itsConnectionIterator->second;

    if(!itsConnection) {
        // No connection found, lets create and initialize one
        const DBusType_t dbusType = DBusAddressTranslator::get()->getDBusBusType(_context->getName());
        itsConnection = std::make_shared<DBusConnection>(dbusType, _context->getName());

        if (itsConnection) {
            if (!itsConnection->connect(false)) {
                itsConnection.reset();
            } else {
                connections_.insert({ _context->getName(), itsConnection } );
                itsConnection->attachMainLoopContext(_context);
            }
        }
    }

    if (itsConnection)
        incrementConnection(itsConnection);

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
            if (!isDeregistered) {
                COMMONAPI_ERROR(std::string(__FUNCTION__), " couldn't deregister ", serviceName);
            }
            services_.erase(insertResult.first);
        }

        if(isAcquired)
            incrementConnection(_stubAdapter->getDBusConnection());

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
        decrementConnection(connection);
    }
    // TODO: log error
    return isUnregistered;
}

void Factory::incrementConnection(std::shared_ptr<DBusProxyConnection> _connection) {
    std::lock_guard<std::recursive_mutex> itsConnectionGuard(connectionsMutex_);
    std::shared_ptr<DBusConnection> connection;
    for (auto itsConnectionIterator = connections_.begin(); itsConnectionIterator != connections_.end(); itsConnectionIterator++) {
        if (itsConnectionIterator->second == _connection) {
            connection = itsConnectionIterator->second;
            break;
        }
    }

    if(connection)
        connection->incrementConnection();
}

void Factory::decrementConnection(std::shared_ptr<DBusProxyConnection> _connection) {
    std::lock_guard<std::recursive_mutex> itsConnectionGuard(connectionsMutex_);
    std::shared_ptr<DBusConnection> connection;
    for (auto itsConnectionIterator = connections_.begin(); itsConnectionIterator != connections_.end(); itsConnectionIterator++) {
        if (itsConnectionIterator->second == _connection) {
            connection = itsConnectionIterator->second;
            break;
        }
    }

    if(connection)
        connection->decrementConnection();
}

void Factory::releaseConnection(const ConnectionId_t& _connectionId) {
    std::lock_guard<std::recursive_mutex> itsConnectionGuard(connectionsMutex_);
    auto itsConnection = connections_.find(_connectionId);

    if (itsConnection != connections_.end()) {
        DBusServiceRegistry::remove(itsConnection->second);
        connections_.erase(_connectionId);
    }
}

} // namespace DBus
} // namespace CommonAPI
