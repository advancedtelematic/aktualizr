// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <sstream>
#include <unordered_set>

#include <dbus/dbus-protocol.h>

#include <CommonAPI/Utils.hpp>
#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusDaemonProxy.hpp>
#include <CommonAPI/DBus/DBusFreedesktopPropertiesStub.hpp>
#include <CommonAPI/DBus/DBusObjectManager.hpp>
#include <CommonAPI/DBus/DBusOutputStream.hpp>
#include <CommonAPI/DBus/DBusStubAdapter.hpp>
#include <CommonAPI/DBus/DBusUtils.hpp>

namespace CommonAPI {
namespace DBus {

DBusObjectManager::DBusObjectManager(const std::shared_ptr<DBusProxyConnection>& dbusConnection):
                rootDBusObjectManagerStub_(new DBusObjectManagerStub("/", dbusConnection)),
                dbusConnection_(dbusConnection){

    if (!dbusConnection->isObjectPathMessageHandlerSet()) {
        dbusConnection->setObjectPathMessageHandler(
                        std::bind(&DBusObjectManager::handleMessage, this, std::placeholders::_1));
    }
    dbusConnection->registerObjectPath("/");

    dbusRegisteredObjectsTable_.insert({
                    DBusInterfaceHandlerPath("/", DBusObjectManagerStub::getInterfaceName()),
                    rootDBusObjectManagerStub_ });
}

DBusObjectManager::~DBusObjectManager() {
    std::shared_ptr<DBusProxyConnection> dbusConnection = dbusConnection_.lock();
    if (dbusConnection) {
        dbusConnection->unregisterObjectPath("/");
        dbusConnection->setObjectPathMessageHandler(DBusProxyConnection::DBusObjectPathMessageHandler());
    }
}

bool DBusObjectManager::registerDBusStubAdapter(std::shared_ptr<DBusStubAdapter> dbusStubAdapter) {
    const auto& dbusStubAdapterObjectPath = dbusStubAdapter->getDBusAddress().getObjectPath();
    const auto& dbusStubAdapterInterfaceName = dbusStubAdapter->getDBusAddress().getInterface();
    DBusInterfaceHandlerPath dbusStubAdapterHandlerPath(dbusStubAdapterObjectPath, dbusStubAdapterInterfaceName);
    bool isRegistrationSuccessful = false;

    objectPathLock_.lock();
    isRegistrationSuccessful = addDBusInterfaceHandler(dbusStubAdapterHandlerPath, dbusStubAdapter);

    if (isRegistrationSuccessful && dbusStubAdapter->hasFreedesktopProperties()) {
        const std::shared_ptr<DBusFreedesktopPropertiesStub> dbusFreedesktopPropertiesStub =
                std::make_shared<DBusFreedesktopPropertiesStub>(dbusStubAdapterObjectPath,
                                                                dbusStubAdapterInterfaceName,
                                                                dbusStubAdapter->getDBusConnection(),
                                                                dbusStubAdapter);
        isRegistrationSuccessful = isRegistrationSuccessful
            && addDBusInterfaceHandler({ dbusFreedesktopPropertiesStub->getObjectPath(),
                                         dbusFreedesktopPropertiesStub->getInterface() },
                                       dbusFreedesktopPropertiesStub);
    }

    if (isRegistrationSuccessful && dbusStubAdapter->isManaging()) {
        auto managerStubIterator = managerStubs_.find(dbusStubAdapterObjectPath);
        const bool managerStubExists = managerStubIterator != managerStubs_.end();

        if (!managerStubExists) {
            const std::shared_ptr<DBusObjectManagerStub> newManagerStub
                = std::make_shared<DBusObjectManagerStub>(
                        dbusStubAdapterObjectPath,
                        dbusStubAdapter->getDBusConnection()
                  );
            auto insertResult = managerStubs_.insert( {dbusStubAdapterObjectPath, {newManagerStub, 1} });
            assert(insertResult.second);
            managerStubIterator = insertResult.first;
        } else {
            uint32_t& countReferencesToManagerStub = std::get<1>(managerStubIterator->second);
            ++countReferencesToManagerStub;
        }

        std::shared_ptr<DBusObjectManagerStub> dbusObjectManagerStub = std::get<0>(managerStubIterator->second);
        assert(dbusObjectManagerStub);

        isRegistrationSuccessful = addDBusInterfaceHandler(
                        { dbusStubAdapterObjectPath, dbusObjectManagerStub->getInterfaceName() }, dbusObjectManagerStub);

        if (!isRegistrationSuccessful) {
            const bool isDBusStubAdapterRemoved = removeDBusInterfaceHandler(dbusStubAdapterHandlerPath, dbusStubAdapter);
            assert(isDBusStubAdapterRemoved);
            (void)isDBusStubAdapterRemoved;
        }
    }

    if (isRegistrationSuccessful) {
        std::shared_ptr<DBusProxyConnection> dbusConnection = dbusConnection_.lock();
        if (dbusConnection) {
            dbusConnection->registerObjectPath(dbusStubAdapterObjectPath);
        }
    }
    objectPathLock_.unlock();

    return isRegistrationSuccessful;
}


bool DBusObjectManager::unregisterDBusStubAdapter(std::shared_ptr<DBusStubAdapter> dbusStubAdapter) {
    const auto& dbusStubAdapterObjectPath = dbusStubAdapter->getDBusAddress().getObjectPath();
    const auto& dbusStubAdapterInterfaceName = dbusStubAdapter->getDBusAddress().getInterface();
    DBusInterfaceHandlerPath dbusStubAdapterHandlerPath(dbusStubAdapterObjectPath, dbusStubAdapterInterfaceName);
    bool isDeregistrationSuccessful = false;

    objectPathLock_.lock();
    isDeregistrationSuccessful = removeDBusInterfaceHandler(dbusStubAdapterHandlerPath, dbusStubAdapter);

    if (isDeregistrationSuccessful && dbusStubAdapter->isManaging()) {
        auto managerStubIterator = managerStubs_.find(dbusStubAdapterObjectPath);
        assert(managerStubIterator != managerStubs_.end());

        std::shared_ptr<DBusObjectManagerStub> dbusObjectManagerStub = std::get<0>(managerStubIterator->second);
        assert(dbusObjectManagerStub);

        uint32_t& countReferencesToManagerStub = std::get<1>(managerStubIterator->second);
        assert(countReferencesToManagerStub > 0);
        --countReferencesToManagerStub;

        if (countReferencesToManagerStub == 0) {
            isDeregistrationSuccessful = removeDBusInterfaceHandler(
                            { dbusStubAdapterObjectPath, dbusObjectManagerStub->getInterfaceName() }, dbusObjectManagerStub);
            managerStubs_.erase(managerStubIterator);
            assert(isDeregistrationSuccessful);
        }
    }

    if (isDeregistrationSuccessful) {
        std::shared_ptr<DBusProxyConnection> lockedConnection = dbusConnection_.lock();
        if (lockedConnection) {
            lockedConnection->unregisterObjectPath(dbusStubAdapterObjectPath);
        }
    }

    objectPathLock_.unlock();

    return isDeregistrationSuccessful;
}


bool DBusObjectManager::exportManagedDBusStubAdapter(const std::string& parentObjectPath, std::shared_ptr<DBusStubAdapter> dbusStubAdapter) {
    auto foundManagerStubIterator = managerStubs_.find(parentObjectPath);

    assert(foundManagerStubIterator != managerStubs_.end());

    if (std::get<0>(foundManagerStubIterator->second)->exportManagedDBusStubAdapter(dbusStubAdapter)) {
        // TODO Check if other handling is necessary?
        return true;
    }
    return false;
}


bool DBusObjectManager::unexportManagedDBusStubAdapter(const std::string& parentObjectPath, std::shared_ptr<DBusStubAdapter> dbusStubAdapter) {
    auto foundManagerStubIterator = managerStubs_.find(parentObjectPath);

    if (foundManagerStubIterator != managerStubs_.end()) {
        if (std::get<0>(foundManagerStubIterator->second)->unexportManagedDBusStubAdapter(
                dbusStubAdapter)) {
            // Check if other handling is necessary?
            return true;
        }
    }
    return false;
}


bool DBusObjectManager::handleMessage(const DBusMessage& dbusMessage) {
    const char* objectPath = dbusMessage.getObjectPath();
    const char* interfaceName = dbusMessage.getInterface();

    assert(objectPath);
    assert(interfaceName);

    DBusInterfaceHandlerPath handlerPath(objectPath, interfaceName);

    objectPathLock_.lock();
    auto handlerIterator = dbusRegisteredObjectsTable_.find(handlerPath);
    const bool foundDBusInterfaceHandler = handlerIterator != dbusRegisteredObjectsTable_.end();
    bool dbusMessageHandled = false;

    if (foundDBusInterfaceHandler) {
        std::shared_ptr<DBusInterfaceHandler> dbusStubAdapterBase = handlerIterator->second;
        dbusMessageHandled = dbusStubAdapterBase->onInterfaceDBusMessage(dbusMessage);
    } else if (dbusMessage.hasInterfaceName("org.freedesktop.DBus.Introspectable")) {
        dbusMessageHandled = onIntrospectableInterfaceDBusMessage(dbusMessage);
    }
    objectPathLock_.unlock();

    return dbusMessageHandled;
}

bool DBusObjectManager::addDBusInterfaceHandler(const DBusInterfaceHandlerPath& dbusInterfaceHandlerPath,
                                                std::shared_ptr<DBusInterfaceHandler> dbusInterfaceHandler) {
    const auto& dbusRegisteredObjectsTableIter = dbusRegisteredObjectsTable_.find(dbusInterfaceHandlerPath);
    const bool isDBusInterfaceHandlerAlreadyAdded = (dbusRegisteredObjectsTableIter != dbusRegisteredObjectsTable_.end());

    if (isDBusInterfaceHandlerAlreadyAdded) {
        //If another ObjectManager or a freedesktop properties stub is to be registered,
        //you can go on and just use the first one.
        if (dbusInterfaceHandlerPath.second == "org.freedesktop.DBus.ObjectManager" ||
                dbusInterfaceHandlerPath.second == "org.freedesktop.DBus.Properties") {
            return true;
        }
        return false;
    }

    auto insertResult = dbusRegisteredObjectsTable_.insert({ dbusInterfaceHandlerPath, dbusInterfaceHandler });
    const bool insertSuccess = insertResult.second;

    return insertSuccess;
}

bool DBusObjectManager::removeDBusInterfaceHandler(const DBusInterfaceHandlerPath& dbusInterfaceHandlerPath,
                                                   std::shared_ptr<DBusInterfaceHandler> dbusInterfaceHandler) {
    const auto& dbusRegisteredObjectsTableIter = dbusRegisteredObjectsTable_.find(dbusInterfaceHandlerPath);
    const bool isDBusInterfaceHandlerAdded = (dbusRegisteredObjectsTableIter != dbusRegisteredObjectsTable_.end());

    if (isDBusInterfaceHandlerAdded) {
        auto registeredDBusStubAdapter = dbusRegisteredObjectsTableIter->second;
        assert(registeredDBusStubAdapter == dbusInterfaceHandler);
        (void)dbusInterfaceHandler;
        dbusRegisteredObjectsTable_.erase(dbusRegisteredObjectsTableIter);
    }

    return isDBusInterfaceHandlerAdded;
}

bool DBusObjectManager::onIntrospectableInterfaceDBusMessage(const DBusMessage& dbusMessage) {
    std::shared_ptr<DBusProxyConnection> dbusConnection = dbusConnection_.lock();

    if (!dbusConnection || !dbusMessage.isMethodCallType() || !dbusMessage.hasMemberName("Introspect")) {
        return false;
    }

    bool foundRegisteredObjects = false;
    std::stringstream xmlData(std::ios_base::out);

    xmlData << "<!DOCTYPE node PUBLIC \"" DBUS_INTROSPECT_1_0_XML_PUBLIC_IDENTIFIER "\"\n\""
                    DBUS_INTROSPECT_1_0_XML_SYSTEM_IDENTIFIER"\">\n"
                    "<node name=\"" << dbusMessage.getObjectPath() << "\">\n"
                        "<interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                            "<method name=\"Introspect\">\n"
                                "<arg type=\"s\" name=\"xml_data\" direction=\"out\"/>\n"
                            "</method>\n"
                        "</interface>\n";

    std::unordered_set<std::string> nodeSet;
    for (auto& registeredObjectsIterator : dbusRegisteredObjectsTable_) {
        const DBusInterfaceHandlerPath& handlerPath = registeredObjectsIterator.first;
        const std::string& dbusObjectPath = handlerPath.first;
        const std::string& dbusInterfaceName = handlerPath.second;
        std::shared_ptr<DBusInterfaceHandler> dbusStubAdapterBase = registeredObjectsIterator.second;
        std::vector<std::string> elems = CommonAPI::split(dbusObjectPath, '/');

        if (dbusMessage.hasObjectPath(dbusObjectPath)) {
            foundRegisteredObjects = true;

            xmlData << "<interface name=\"" << dbusInterfaceName << "\">\n"
                            << dbusStubAdapterBase->getMethodsDBusIntrospectionXmlData() << "\n"
                            "</interface>\n";
            nodeSet.insert(elems.back());
        } else {
            if (dbusMessage.hasObjectPath("/") && elems.size() > 1) {
                if (nodeSet.find(elems[1]) == nodeSet.end()) {
                    if (nodeSet.size() == 0) {
                        xmlData.str("");
                        xmlData << "<!DOCTYPE node PUBLIC \"" DBUS_INTROSPECT_1_0_XML_PUBLIC_IDENTIFIER "\"\n\""
                        DBUS_INTROSPECT_1_0_XML_SYSTEM_IDENTIFIER"\">\n"
                        "<node>\n";
                    }
                    xmlData << "    <node name=\"" << elems[1] << "\"/>\n";
                    nodeSet.insert(elems[1]);
                    foundRegisteredObjects = true;
                }
            } else {
                for (unsigned int i = 1; i < elems.size() - 1; i++) {
                    std::string build;
                    for (unsigned int j = 1; j <= i; j++) {
                        build = build + "/" + elems[j];
                        if (dbusMessage.hasObjectPath(build)) {
                            if (nodeSet.find(elems[j + 1]) == nodeSet.end()) {
                                if (nodeSet.size() == 0) {
                                    xmlData.str("");
                                    xmlData << "<!DOCTYPE node PUBLIC \"" DBUS_INTROSPECT_1_0_XML_PUBLIC_IDENTIFIER "\"\n\""
                                            DBUS_INTROSPECT_1_0_XML_SYSTEM_IDENTIFIER"\">\n"
                                            "<node>\n";
                                }
                                xmlData << "    <node name=\"" << elems[j + 1] << "\"/>\n";
                                nodeSet.insert(elems[j + 1]);
                                foundRegisteredObjects = true;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    if (foundRegisteredObjects) {
        DBusMessage dbusMessageReply = dbusMessage.createMethodReturn("s");
        DBusOutputStream dbusOutputStream(dbusMessageReply);

        xmlData << "</node>"
                        "";

        dbusOutputStream << xmlData.str();
        dbusOutputStream.flush();

        return dbusConnection->sendDBusMessage(dbusMessageReply);
    }

    return false;
}

std::shared_ptr<DBusObjectManagerStub> DBusObjectManager::getRootDBusObjectManagerStub() {
    return rootDBusObjectManagerStub_;
}

} // namespace DBus
} // namespace CommonAPI
