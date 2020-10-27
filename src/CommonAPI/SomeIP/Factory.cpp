// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Runtime.hpp>
#include <CommonAPI/SomeIP/Address.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>
#include <CommonAPI/SomeIP/Factory.hpp>
#include <CommonAPI/SomeIP/StubAdapter.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>

namespace CommonAPI {
namespace SomeIP {

INITIALIZER(FactoryInit) {
    Factory::runtime_ = Runtime::get();
    Factory::runtime_.lock()->registerFactory("someip", Factory::get());
}

DEINITIALIZER(FactoryDeinit) {
    if (auto rt = Factory::runtime_.lock()) {
        rt->unregisterFactory("someip");
    }
}

std::weak_ptr<CommonAPI::Runtime> Factory::runtime_;

std::shared_ptr<Factory>
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
#ifndef _WIN32
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
#ifndef _WIN32
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
    const std::string &_interface,
    ProxyCreateFunction _function) {
    COMMONAPI_VERBOSE("Registering function for creating \"", _interface,
            "\" proxy.");
    proxyCreateFunctions_[_interface] = _function;
}

void
Factory::registerStubAdapterCreateMethod(
    const std::string &_interface,
    StubAdapterCreateFunction _function) {
    COMMONAPI_INFO("Registering function for creating \"", _interface,
            "\" stub adapter.");
    stubAdapterCreateFunctions_[_interface] = _function;
}

std::shared_ptr<CommonAPI::Proxy>
Factory::createProxy(
    const std::string &_domain,
    const std::string &_interface, const std::string &_instance,
    const ConnectionId_t &_connectionId) {

    COMMONAPI_VERBOSE("Creating proxy for \"", _domain, ":", _interface, ":",
            _instance, "\"");

    auto proxyCreateFunctionsIterator
        = proxyCreateFunctions_.lower_bound(_interface);
    if (proxyCreateFunctionsIterator
            != proxyCreateFunctions_.end()) {
        std::string itsInterface(_interface);
        if (proxyCreateFunctionsIterator->first != _interface) {
            std::string itsInterfaceMajor(_interface.substr(0, _interface.find('_')));
            if (proxyCreateFunctionsIterator->first.find(itsInterfaceMajor) != 0)
                return nullptr;

            itsInterface = proxyCreateFunctionsIterator->first;
        }

        CommonAPI::Address address(_domain, itsInterface, _instance);
        Address someipAddress;
        if (AddressTranslator::get()->translate(address, someipAddress)) {
            std::shared_ptr<Connection> itsConnection
                = getConnection(_connectionId);
            if (itsConnection) {
                std::shared_ptr<Proxy> proxy
                    = proxyCreateFunctionsIterator->second(
                            someipAddress, itsConnection);
                if (proxy && proxy->init())
                    return proxy;
            }
        }
    }

    COMMONAPI_ERROR("Creating proxy for \"", _domain, ":", _interface, ":",
            _instance, "\" failed!");
    return nullptr;
}

std::shared_ptr<CommonAPI::Proxy>
Factory::createProxy(
    const std::string &_domain,
    const std::string &_interface, const std::string &_instance,
    std::shared_ptr<MainLoopContext> _context) {

    COMMONAPI_VERBOSE("Creating proxy for \"", _domain, ":", _interface, ":",
            _instance, "\"");

    auto proxyCreateFunctionsIterator
        = proxyCreateFunctions_.lower_bound(_interface);
    if (proxyCreateFunctionsIterator
            != proxyCreateFunctions_.end()) {
        std::string itsInterface(_interface);
        if (proxyCreateFunctionsIterator->first != _interface) {
            std::string itsInterfaceMajor(_interface.substr(0, _interface.find('_')));
            if (proxyCreateFunctionsIterator->first.find(itsInterfaceMajor) != 0)
                return nullptr;

            itsInterface = proxyCreateFunctionsIterator->first;
        }

        CommonAPI::Address address(_domain, itsInterface, _instance);
        Address someipAddress;
        if (AddressTranslator::get()->translate(address, someipAddress)) {
            std::shared_ptr<Connection> itsConnection
                = getConnection(_context);
            if (itsConnection) {
                std::shared_ptr<Proxy> proxy
                    = proxyCreateFunctionsIterator->second(
                            someipAddress, itsConnection);
                if (proxy && proxy->init())
                    return proxy;
            }
        }
    }

    COMMONAPI_ERROR("Creating proxy for \"", _domain, ":", _interface, ":",
            _instance, "\" failed!");
    return nullptr;
}

bool
Factory::registerStub(
        const std::string &_domain,
        const std::string &_interface, const std::string &_instance,
        std::shared_ptr<StubBase> _stub, const ConnectionId_t &_connection) {

    COMMONAPI_INFO("Registering stub for \"", _domain, ":", _interface, ":",
            _instance, "\"");

    auto stubAdapterCreateFunctionsIterator
        = stubAdapterCreateFunctions_.lower_bound(_interface);
    if (stubAdapterCreateFunctionsIterator
            != stubAdapterCreateFunctions_.end()) {
        std::string itsInterface(_interface);
        if (stubAdapterCreateFunctionsIterator->first != _interface) {
            std::string itsInterfaceMajor(_interface.substr(0, _interface.find('_')));
            if (stubAdapterCreateFunctionsIterator->first.find(itsInterfaceMajor) != 0)
                return false;

            itsInterface = stubAdapterCreateFunctionsIterator->first;
        }

        CommonAPI::Address address(_domain, itsInterface, _instance);
        Address someipAddress;
        AddressTranslator::get()->translate(address, someipAddress);

        std::shared_ptr<Connection> itsConnection = getConnection(_connection);
        if (itsConnection) {
            std::shared_ptr<StubAdapter> adapter
                = stubAdapterCreateFunctionsIterator->second(
                        someipAddress, itsConnection, _stub);
            if (adapter) {
                adapter->registerSelectiveEventHandlers();
                adapter->init(adapter);
                if (registerStubAdapter(adapter))
                    return true;
            }
        }
    }

    COMMONAPI_ERROR("Registering stub for \"", _domain, ":", _interface, ":",
            _instance, "\" failed!");
    return false;
}

bool
Factory::registerStub(
        const std::string &_domain,
        const std::string &_interface, const std::string &_instance,
        std::shared_ptr<StubBase> _stub,
        std::shared_ptr<MainLoopContext> _context) {

    COMMONAPI_INFO("Registering stub for \"", _domain, ":", _interface, ":",
            _instance, "\"");

    auto stubAdapterCreateFunctionsIterator
        = stubAdapterCreateFunctions_.lower_bound(_interface);
    if (stubAdapterCreateFunctionsIterator
            != stubAdapterCreateFunctions_.end()) {
        std::string itsInterface(_interface);
        if (stubAdapterCreateFunctionsIterator->first != _interface) {
            std::string itsInterfaceMajor(_interface.substr(0, _interface.find('_')));
            if (stubAdapterCreateFunctionsIterator->first.find(itsInterfaceMajor) != 0)
                return false;

            itsInterface = stubAdapterCreateFunctionsIterator->first;
        }

        CommonAPI::Address address(_domain, itsInterface, _instance);
        Address someipAddress;
        if (AddressTranslator::get()->translate(address, someipAddress)) {
            std::shared_ptr<Connection> itsConnection = getConnection(_context);
            if (itsConnection) {
                std::shared_ptr<StubAdapter> adapter
                    = stubAdapterCreateFunctionsIterator->second(
                            someipAddress, itsConnection, _stub);
                if (adapter) {
                    adapter->registerSelectiveEventHandlers();
                    adapter->init(adapter);
                    if (registerStubAdapter(adapter))
                        return true;
                }
            }
        }
    }

    COMMONAPI_ERROR("Registering stub for \"", _domain, ":", _interface, ":",
            _instance, "\" failed!");
    return false;
}

bool
Factory::registerStubAdapter(std::shared_ptr<StubAdapter> _adapter) {
    const std::shared_ptr<ProxyConnection> connection
        = _adapter->getConnection();
    CommonAPI::Address address;
    Address someipAddress = _adapter->getSomeIpAddress();
    if (AddressTranslator::get()->translate(someipAddress, address)) {
        std::lock_guard<std::mutex> itsLock(servicesMutex_);
        const auto &insertResult
            = services_.insert( { address.getAddress(), _adapter } );
        const bool &isInsertSuccessful = insertResult.second;

        if (isInsertSuccessful) {
            std::shared_ptr<StubManager> manager
                = connection->getStubManager();
            manager->registerStubAdapter(_adapter);
            return true;
        }
    }

    return false;
}

bool
Factory::unregisterStub(const std::string &_domain,
                        const std::string &_interface,
                        const std::string &_instance) {

    COMMONAPI_INFO("Deregistering stub for \"", _domain, ":", _interface, ":",
            _instance, "\"");

    servicesMutex_.lock();
    CommonAPI::Address address(_domain, _interface, _instance);

    const auto &adapterResult = services_.find(address.getAddress());

    if (adapterResult == services_.end()) {
        servicesMutex_.unlock();
        COMMONAPI_INFO("Deregistering stub for \"", _domain, ":", _interface,
                ":", _instance, "\" failed (Not registered).");
        return false;
    }

    const auto adapter = adapterResult->second;
    const auto &connection = adapter->getConnection();
    const auto stubManager = connection->getStubManager();

    adapter->unregisterSelectiveEventHandlers();
    stubManager->unregisterStubAdapter(adapter);

    if(!services_.erase(address.getAddress())) {
        servicesMutex_.unlock();
        COMMONAPI_INFO("Deregistering stub for \"", _domain, ":", _interface,
                ":", _instance, "\" failed (Removal failed).");
        return false;
    }

    servicesMutex_.unlock();

    decrementConnection(connection);

    return true;
}


std::shared_ptr<StubAdapter>
Factory::createStubAdapter(
        const std::shared_ptr<StubBase> &_stub,
        const std::string &_interface,
        const Address &_address,
        const std::shared_ptr<ProxyConnection> &_connection) {
    std::shared_ptr<StubAdapter> stubAdapter;
    auto stubAdapterCreateFunctionsIterator
        = stubAdapterCreateFunctions_.find(_interface);
    if (stubAdapterCreateFunctionsIterator
            != stubAdapterCreateFunctions_.end()) {
        stubAdapter = stubAdapterCreateFunctionsIterator->second(
                        _address, _connection, _stub);
        if (stubAdapter) {
            stubAdapter->registerSelectiveEventHandlers();
            stubAdapter->init(stubAdapter);
        } else
            COMMONAPI_ERROR("Couldn't create stubAdapter for " + _interface);
    } else {
        COMMONAPI_ERROR("Didn't find stubAdapter create function for: "
                + _interface);
    }
    return stubAdapter;
}

bool
Factory::isRegisteredService(const std::string &_address) {
    auto serviceIterator = services_.find(_address);
    if (serviceIterator != services_.end()) {
        return true;
    }
    return false;
}

bool
Factory::registerManagedService(const std::shared_ptr<StubAdapter> &_adapter) {
    if(!registerStubAdapter(_adapter)) {
        COMMONAPI_ERROR("Call to registerStubAdapter(_adapter) failed");
        return false;
    }
    incrementConnection(_adapter->getConnection());
    return true;
}

bool
Factory::unregisterManagedService(const std::string &_address) {
    CommonAPI::Address capiAddress(_address);
    if(!unregisterStub(capiAddress.getDomain(),
                       capiAddress.getInterface(),
                       capiAddress.getInstance())) {
        COMMONAPI_ERROR("Call to unregisterStub() failed with address: "
                + _address);
        return false;
    }
    return true;
}

std::shared_ptr<Connection>
Factory::getConnection(const ConnectionId_t &_connectionId) {
    std::unique_lock<std::recursive_mutex> itsLock(connectionMutex_);

    auto itsConnectionIterator = connections_.find(_connectionId);
    if (itsConnectionIterator != connections_.end()) {
        incrementConnection(itsConnectionIterator->second);
        return itsConnectionIterator->second;
    }

    // No connection found, lets create and initialize one
    std::shared_ptr<Connection> itsConnection
            = std::make_shared<Connection>(_connectionId);
    if (itsConnection) {
        if (!itsConnection->connect(true)) {
            COMMONAPI_ERROR("Failed to create connection ", _connectionId);
            itsConnection.reset();
        } else {
            connections_.insert({ _connectionId, itsConnection } );
        }
    }

    if(itsConnection)
        incrementConnection(itsConnection);

    return itsConnection;
}

std::shared_ptr<Connection>
Factory::getConnection(std::shared_ptr<MainLoopContext> _context) {
    std::unique_lock<std::recursive_mutex> itsLock(connectionMutex_);

    if (!_context)
        return getConnection(DEFAULT_CONNECTION_ID);

    std::shared_ptr<Connection> itsConnection;

    auto itsConnectionIterator = connections_.find(_context->getName());
    if (itsConnectionIterator != connections_.end()) {
        itsConnection = itsConnectionIterator->second;
    } else {
        itsConnection = std::make_shared<Connection>(_context->getName());
        if (itsConnection) {
            if (!itsConnection->connect(false)) {
                COMMONAPI_ERROR("Failed to create connection ",
                        _context->getName());
                itsConnection.reset();
            } else {
                connections_.insert({ _context->getName(), itsConnection } );
            }
        }
    }

    if (itsConnection) {
        incrementConnection(itsConnection);
        itsConnection->attachMainLoopContext(_context);
    }

    return itsConnection;
}

void Factory::incrementConnection(
        std::shared_ptr<ProxyConnection> _connection) {
    std::shared_ptr<Connection> connection;
    std::unique_lock<std::recursive_mutex> itsLock(connectionMutex_);
    for (auto itsConnectionIterator = connections_.begin();
            itsConnectionIterator != connections_.end();
            itsConnectionIterator++) {
        if (itsConnectionIterator->first
                == _connection->getConnectionId()) {
            connection = itsConnectionIterator->second;
            break;
        }
    }
    if (connection)
        connection->incrementConnection();
}

void Factory::decrementConnection(
        std::shared_ptr<ProxyConnection> _connection) {
    std::shared_ptr<Connection> connection;
    std::unique_lock<std::recursive_mutex> itsLock(connectionMutex_);
    for (auto itsConnectionIterator = connections_.begin();
            itsConnectionIterator != connections_.end();
            itsConnectionIterator++) {
        if (itsConnectionIterator->first
                == _connection->getConnectionId()) {
            connection = itsConnectionIterator->second;
            break;
        }
    }
    if (connection)
        connection->decrementConnection();
}

void Factory::releaseConnection(const ConnectionId_t& _connectionId) {
    std::unique_lock<std::recursive_mutex> itsLock(connectionMutex_);
    connections_.erase(_connectionId);
}

} // namespace SomeIP
} // namespace CommonAPI
