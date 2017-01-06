// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include <sys/stat.h>

#include <algorithm>

#include <CommonAPI/Factory.hpp>
#include <CommonAPI/IniFileReader.hpp>
#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Runtime.hpp>

namespace CommonAPI {

const char *COMMONAPI_DEFAULT_BINDING = "dbus";
const char *COMMONAPI_DEFAULT_FOLDER = "/usr/local/lib/commonapi";
const char *COMMONAPI_DEFAULT_CONFIG_FILE = "commonapi.ini";
const char *COMMONAPI_DEFAULT_CONFIG_FOLDER = "/etc";

std::map<std::string, std::string> properties__;
std::shared_ptr<Runtime> Runtime::theRuntime__ = std::make_shared<Runtime>();

std::string
Runtime::getProperty(const std::string &_name) {
    auto foundProperty = properties__.find(_name);
    if (foundProperty != properties__.end())
        return foundProperty->second;
    return "";
}

void
Runtime::setProperty(const std::string &_name, const std::string &_value) {
    properties__[_name] = _value;
}

std::shared_ptr<Runtime> Runtime::get() {
    theRuntime__->init();
    return theRuntime__;
}

Runtime::Runtime()
    : defaultBinding_(COMMONAPI_DEFAULT_BINDING),
      defaultFolder_(COMMONAPI_DEFAULT_FOLDER) {
}

Runtime::~Runtime() {
    // intentionally left empty
}

bool
Runtime::registerFactory(const std::string &_binding, std::shared_ptr<Factory> _factory) {
    COMMONAPI_DEBUG("Registering factory for binding=", _binding);
    bool isRegistered(false);
#ifndef WIN32
    std::lock_guard<std::mutex> itsLock(factoriesMutex_);
#endif
    if (_binding == defaultBinding_) {
        defaultFactory_ = _factory;
    } else {
        auto foundFactory = factories_.find(_binding);
        if (foundFactory == factories_.end()) {
            factories_[_binding] = _factory;
            isRegistered = true;
        }
    }
    return isRegistered;
}

bool
Runtime::unregisterFactory(const std::string &_binding) {
    COMMONAPI_DEBUG("Unregistering factory for binding=", _binding);
#ifndef WIN32
    std::lock_guard<std::mutex> itsLock(factoriesMutex_);
#endif
    if (_binding == defaultBinding_) {
        defaultFactory_.reset();
    } else {
        factories_.erase(_binding);
    }
    return true;
}

/*
 * Private
 */
void Runtime::init() {
    static bool isInitialized(false);
#ifndef WIN32
    std::lock_guard<std::mutex> itsLock(mutex_);
#endif
    if (!isInitialized) {
        // Determine default configuration file
        const char *config = getenv("COMMONAPI_CONFIG");
        if (config) {
            defaultConfig_ = config;
        } else {
            defaultConfig_ = COMMONAPI_DEFAULT_CONFIG_FOLDER;
            defaultConfig_ += "/";
            defaultConfig_ += COMMONAPI_DEFAULT_CONFIG_FILE;
        }

        // TODO: evaluate return parameter and decide what to do
        (void)readConfiguration();

        // Determine default ipc & shared library folder
        const char *binding = getenv("COMMONAPI_DEFAULT_BINDING");
        if (binding)
            defaultBinding_ = binding;

        const char *folder = getenv("COMMONAPI_DEFAULT_FOLDER");
        if (folder)
            defaultFolder_ = folder;

        // Log settings
        COMMONAPI_INFO("Using default binding \'", defaultBinding_, "\'");
        COMMONAPI_INFO("Using default shared library folder \'", defaultFolder_, "\'");
        COMMONAPI_INFO("Using default configuration file \'", defaultConfig_, "\'");

        isInitialized = true;
    }
}

bool
Runtime::readConfiguration() {
#define MAX_PATH_LEN 255
    std::string config;
    char currentDirectory[MAX_PATH_LEN];
#ifdef WIN32
    if (GetCurrentDirectory(MAX_PATH_LEN, currentDirectory)) {
#else
    if (getcwd(currentDirectory, MAX_PATH_LEN)) {
#endif
        config = currentDirectory;
        config += "/";
        config += COMMONAPI_DEFAULT_CONFIG_FILE;

        struct stat s;
        if (stat(config.c_str(), &s) != 0) {
            config = defaultConfig_;
        }
    }

    IniFileReader reader;
    if (!reader.load(config))
        return false;

    std::string itsConsole("true");
    std::string itsFile;
    std::string itsDlt("false");
    std::string itsLevel("info");

    std::shared_ptr<IniFileReader::Section> section
        = reader.getSection("logging");
    if (section) {
        itsConsole = section->getValue("console");
        itsFile = section->getValue("file");
        itsDlt = section->getValue("dlt");
        itsLevel = section->getValue("level");
    }

    Logger::init((itsConsole == "true"),
                 itsFile,
                 (itsDlt == "true"),
                 itsLevel);

    section    = reader.getSection("default");
    if (section) {
        std::string binding = section->getValue("binding");
        if ("" != binding)
            defaultBinding_ = binding;

        std::string folder = section->getValue("folder");
        if ("" != folder)
            defaultFolder_ = folder;
    }

    section = reader.getSection("proxy");
    if (section) {
        for (auto m : section->getMappings()) {
            COMMONAPI_DEBUG("Adding proxy mapping: ", m.first, " --> ", m.second);
            libraries_[m.first][true] = m.second;
        }
    }

    section = reader.getSection("stub");
    if (section) {
        for (auto m : section->getMappings()) {
            COMMONAPI_DEBUG("Adding stub mapping: ", m.first, " --> ", m.second);
            libraries_[m.first][false] = m.second;
        }
    }

    return true;
}

std::shared_ptr<Proxy>
Runtime::createProxy(
        const std::string &_domain, const std::string &_interface, const std::string &_instance,
        const ConnectionId_t &_connectionId) {

    // Check whether we already know how to create such proxies...
    std::shared_ptr<Proxy> proxy = createProxyHelper(_domain, _interface, _instance, _connectionId, false);
    if (!proxy) {
        // ...it seems do not, lets try to load a library that does...
        std::lock_guard<std::mutex> itsGuard(loadMutex_);
        std::string library = getLibrary(_domain, _interface, _instance, true);
        if (loadLibrary(library) || defaultFactory_) {
            proxy = createProxyHelper(_domain, _interface, _instance, _connectionId, true);
        }
    }
    return proxy;
}

std::shared_ptr<Proxy>
Runtime::createProxy(
        const std::string &_domain, const std::string &_interface, const std::string &_instance,
        std::shared_ptr<MainLoopContext> _context) {

    // Check whether we already know how to create such proxies...
    std::shared_ptr<Proxy> proxy = createProxyHelper(_domain, _interface, _instance, _context, false);
    if (!proxy) {
        // ...it seems do not, lets try to load a library that does...
        std::lock_guard<std::mutex> itsGuard(loadMutex_);
        std::string library = getLibrary(_domain, _interface, _instance, true);
        if (loadLibrary(library) || defaultFactory_) {
            proxy = createProxyHelper(_domain, _interface, _instance, _context, true);
        }
    }
    return proxy;
}


bool
Runtime::registerStub(const std::string &_domain, const std::string &_interface, const std::string &_instance,
                        std::shared_ptr<StubBase> _stub, const ConnectionId_t &_connectionId) {

    bool isRegistered = registerStubHelper(_domain, _interface, _instance, _stub, _connectionId, false);
    if (!isRegistered) {
        std::string library = getLibrary(_domain, _interface, _instance, false);
        std::lock_guard<std::mutex> itsGuard(loadMutex_);
        if (loadLibrary(library) || defaultFactory_) {
            isRegistered = registerStubHelper(_domain, _interface, _instance, _stub, _connectionId, true);
        }
    }
    return isRegistered;
}

bool
Runtime::registerStub(const std::string &_domain, const std::string &_interface, const std::string &_instance,
                        std::shared_ptr<StubBase> _stub, std::shared_ptr<MainLoopContext> _context) {

    bool isRegistered = registerStubHelper(_domain, _interface, _instance, _stub, _context, false);
    if (!isRegistered) {
        std::string library = getLibrary(_domain, _interface, _instance, false);
        std::lock_guard<std::mutex> itsGuard(loadMutex_);
        if (loadLibrary(library) || defaultFactory_) {
            isRegistered = registerStubHelper(_domain, _interface, _instance, _stub, _context, true);
        }
    }
    return isRegistered;
}

bool
Runtime::unregisterStub(const std::string &_domain, const std::string &_interface, const std::string &_instance) {
    for (auto factory : factories_) {
        if (factory.second->unregisterStub(_domain, _interface, _instance))
            return true;
    }

    return (defaultFactory_ ? defaultFactory_->unregisterStub(_domain, _interface, _instance) : false);
}

std::string
Runtime::getLibrary(
    const std::string &_domain, const std::string &_interface, const std::string &_instance,
    bool _isProxy) {

    std::string library;
    std::string address = _domain + ":" + _interface + ":" + _instance;

    COMMONAPI_DEBUG("Loading library for ", address, (_isProxy ? " proxy." : " stub."));

    auto libraryIterator = libraries_.find(address);
    if (libraryIterator != libraries_.end()) {
        auto addressIterator = libraryIterator->second.find(_isProxy);
        if (addressIterator != libraryIterator->second.end()) {
            library = addressIterator->second;
            return library;
        }
    }

    // If no library was explicitely configured, check whether property
    // "LibraryBase" is set. If yes, use it, if not build default library
    // name.
    library = getProperty("LibraryBase");
    if (library != "") {
#ifdef WIN32
        library = library + "-" + defaultBinding_;
#else
        library = "lib" + library + "-" + defaultBinding_;
#endif
    } else {
        library = "lib" + _domain + "__" + _interface + "__" + _instance;
        std::replace(library.begin(), library.end(), '.', '_');
    }

    return library;
}

bool
Runtime::loadLibrary(const std::string &_library) {
    std::string itsLibrary(_library);

    // TODO: decide whether this really is a good idea...
    #ifdef WIN32
    if (itsLibrary.rfind(".dll") != itsLibrary.length() - 4) {
        itsLibrary += ".dll";
    }
    #else
    if (itsLibrary.rfind(".so") != itsLibrary.length() - 3) {
        itsLibrary += ".so";
    }
    #endif

    bool isLoaded(true);
    if (loadedLibraries_.end() == loadedLibraries_.find(itsLibrary)) {
        #ifdef WIN32
        if (LoadLibrary(itsLibrary.c_str()) != 0) {
            loadedLibraries_.insert(itsLibrary);
            COMMONAPI_DEBUG("Loading interface library \"", itsLibrary, "\" succeeded.");
        } else {
            COMMONAPI_DEBUG("Loading interface library \"", itsLibrary, "\" failed (", GetLastError(), ")");
            isLoaded = false;
        }
        #else
        if (dlopen(itsLibrary.c_str(), RTLD_LAZY | RTLD_GLOBAL) != 0) {
            loadedLibraries_.insert(itsLibrary);
            COMMONAPI_DEBUG("Loading interface library \"", itsLibrary, "\" succeeded.");
        }
        else {
            COMMONAPI_DEBUG("Loading interface library \"", itsLibrary, "\" failed (", dlerror(), ")");
            isLoaded = false;
        }
        #endif
    }
    return isLoaded;
}

std::shared_ptr<Proxy>
Runtime::createProxyHelper(const std::string &_domain, const std::string &_interface, const std::string &_instance,
                           const std::string &_connectionId, bool _useDefault) {
    std::lock_guard<std::mutex> itsLock(factoriesMutex_);
    for (auto factory : factories_) {
        std::shared_ptr<Proxy> proxy
            = factory.second->createProxy(_domain, _interface, _instance, _connectionId);
        if (proxy)
            return proxy;
    }
    return (_useDefault && defaultFactory_ ?
                defaultFactory_->createProxy(_domain, _interface, _instance, _connectionId)
                : nullptr);
}

std::shared_ptr<Proxy>
Runtime::createProxyHelper(const std::string &_domain, const std::string &_interface, const std::string &_instance,
                           std::shared_ptr<MainLoopContext> _context, bool _useDefault) {
    std::lock_guard<std::mutex> itsLock(factoriesMutex_);
    for (auto factory : factories_) {
        std::shared_ptr<Proxy> proxy
            = factory.second->createProxy(_domain, _interface, _instance, _context);
        if (proxy)
            return proxy;
    }
    return (_useDefault && defaultFactory_ ?
                defaultFactory_->createProxy(_domain, _interface, _instance, _context) :
                nullptr);
}

bool
Runtime::registerStubHelper(const std::string &_domain, const std::string &_interface, const std::string &_instance,
                            std::shared_ptr<StubBase> _stub, const std::string &_connectionId, bool _useDefault) {
    bool isRegistered(false);
    std::lock_guard<std::mutex> itsLock(factoriesMutex_);
    for (auto factory : factories_) {
        isRegistered = factory.second->registerStub(_domain, _interface, _instance, _stub, _connectionId);
        if (isRegistered)
            return isRegistered;
    }
    return (_useDefault && defaultFactory_ ?
                defaultFactory_->registerStub(_domain, _interface, _instance, _stub, _connectionId) :
                false);
}

bool
Runtime::registerStubHelper(const std::string &_domain, const std::string &_interface, const std::string &_instance,
                            std::shared_ptr<StubBase> _stub, std::shared_ptr<MainLoopContext> _context, bool _useDefault) {
    bool isRegistered(false);
    std::lock_guard<std::mutex> itsLock(factoriesMutex_);
    for (auto factory : factories_) {
        isRegistered = factory.second->registerStub(_domain, _interface, _instance, _stub, _context);
        if (isRegistered)
            return isRegistered;
    }
    return (_useDefault && defaultFactory_ ?
                defaultFactory_->registerStub(_domain, _interface, _instance, _stub, _context) :
                false);
}

} //Namespace CommonAPI
