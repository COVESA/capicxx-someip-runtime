// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
    Runtime::get()->registerFactory("someip", Factory::get());
}

std::shared_ptr<Factory>
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
    const std::string &_interface,
    ProxyCreateFunction _function) {
    COMMONAPI_DEBUG("Registering function for creating \"", _interface, "\" proxy.");
    proxyCreateFunctions_[_interface] = _function;
}

void
Factory::registerStubAdapterCreateMethod(
    const std::string &_interface,
    StubAdapterCreateFunction _function) {
    COMMONAPI_DEBUG("Registering function for creating \"", _interface, "\" stub adapter.");
    stubAdapterCreateFunctions_[_interface] = _function;
}

std::shared_ptr<CommonAPI::Proxy>
Factory::createProxy(
    const std::string &_domain, const std::string &_interface, const std::string &_instance,
    const ConnectionId_t &_connectionId) {

    auto proxyCreateFunctionsIterator = proxyCreateFunctions_.find(_interface);
    if (proxyCreateFunctionsIterator != proxyCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        Address someipAddress;
        if (AddressTranslator::get()->translate(address, someipAddress)) {
            std::shared_ptr<Connection> itsConnection = getConnection(_connectionId);
            if (itsConnection) {
                std::shared_ptr<Proxy> proxy
                    = proxyCreateFunctionsIterator->second(someipAddress, itsConnection);
                if (proxy)
                    proxy->init();
                return proxy;
            }
        }
    }

    return nullptr;
}

std::shared_ptr<CommonAPI::Proxy>
Factory::createProxy(
    const std::string &_domain, const std::string &_interface, const std::string &_instance,
    std::shared_ptr<MainLoopContext> _context) {

    auto proxyCreateFunctionsIterator = proxyCreateFunctions_.find(_interface);
    if (proxyCreateFunctionsIterator != proxyCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        Address someipAddress;
        if (AddressTranslator::get()->translate(address, someipAddress)) {
            std::shared_ptr<Connection> itsConnection = getConnection(_context);
            if (itsConnection) {
                std::shared_ptr<Proxy> proxy
                    = proxyCreateFunctionsIterator->second(someipAddress, itsConnection);
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
        std::shared_ptr<StubBase> _stub, const ConnectionId_t &_connection) {

    auto stubAdapterCreateFunctionsIterator = stubAdapterCreateFunctions_.find(_interface);
    if (stubAdapterCreateFunctionsIterator != stubAdapterCreateFunctions_.end()) {
        CommonAPI::Address address(_domain, _interface, _instance);
        Address someipAddress;
        AddressTranslator::get()->translate(address, someipAddress);

        std::shared_ptr<Connection> itsConnection = getConnection(_connection);
        if (itsConnection) {
            std::shared_ptr<StubAdapter> adapter
                = stubAdapterCreateFunctionsIterator->second(someipAddress, itsConnection, _stub);
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
        Address someipAddress;
        if (AddressTranslator::get()->translate(address, someipAddress)) {
            std::shared_ptr<StubAdapter> adapter
                = stubAdapterCreateFunctionsIterator->second(someipAddress, getConnection(_context), _stub);
            if (adapter) {
                adapter->init(adapter);
                return registerStubAdapter(adapter);
            }
        }
    }

    return false;
}

bool
Factory::registerStubAdapter(std::shared_ptr<StubAdapter> _adapter) {
    const std::shared_ptr<ProxyConnection> connection = _adapter->getConnection();
    CommonAPI::Address address;
    Address someipAddress = _adapter->getSomeIpAddress();
    if (AddressTranslator::get()->translate(someipAddress, address)) {
        std::lock_guard<std::mutex> itsLock(servicesMutex_);
        const auto &insertResult = services_.insert( { address.getAddress(), _adapter } );
        const auto &insertIter = insertResult.first;
        const bool &isInsertSuccessful = insertResult.second;

        if (isInsertSuccessful) {
            std::shared_ptr<StubManager> manager = connection->getStubManager();
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
    std::lock_guard<std::mutex> itsLock(servicesMutex_);
    CommonAPI::Address address(_domain, _interface, _instance);

    const auto &adapterResult = services_.find(address.getAddress());

    if (adapterResult == services_.end()) {
        return false;
    }

    const auto adapter = adapterResult->second;
    const auto &connection = adapter->getConnection();
    const auto stubManager = connection->getStubManager();
    stubManager->unregisterStubAdapter(adapter);

    if(!services_.erase(address.getAddress())) {
        return false;
    }

    return true;
}


std::shared_ptr<StubAdapter>
Factory::createStubAdapter(const std::shared_ptr<StubBase> &_stub,
                           const std::string &_interface,
                           const Address &_address,
                           const std::shared_ptr<ProxyConnection> &_connection) {
    std::shared_ptr<StubAdapter> stubAdapter;
    auto stubAdapterCreateFunctionsIterator = stubAdapterCreateFunctions_.find(_interface);
    if (stubAdapterCreateFunctionsIterator != stubAdapterCreateFunctions_.end()) {
        stubAdapter = stubAdapterCreateFunctionsIterator->second(
                        _address, _connection, _stub);
        if (stubAdapter)
            stubAdapter->init(stubAdapter);
        else
            COMMONAPI_ERROR("Couldn't create stubAdapter for " + _interface);
    } else {
        COMMONAPI_ERROR("Didn't find stubAdapter create function for: " + _interface);
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
    return true;
}

bool
Factory::unregisterManagedService(const std::string &_address) {
    CommonAPI::Address capiAddress(_address);
    if(!unregisterStub(capiAddress.getDomain(),
                       capiAddress.getInterface(),
                       capiAddress.getInstance())) {
        COMMONAPI_ERROR("Call to unregisterStub() failed with address: " + _address);
        return false;
    }
    return true;
}

std::shared_ptr<Connection>
Factory::getConnection(const ConnectionId_t &_connectionId) {
    std::unique_lock<std::mutex> itsLock(connectionMutex_);

    auto itsConnectionIterator = connections_.find(_connectionId);
    if (itsConnectionIterator != connections_.end()) {
        return itsConnectionIterator->second;
    }

    // No connection found, lets create and initialize one
    std::shared_ptr<Connection> itsConnection
            = std::make_shared<Connection>(_connectionId);
    if (itsConnection) {
        connections_.insert({ _connectionId, itsConnection } );

        (void)itsConnection->connect(true);
    }
    return itsConnection;
}

std::shared_ptr<Connection>
Factory::getConnection(std::shared_ptr<MainLoopContext> _context) {
    std::unique_lock<std::mutex> itsLock(connectionMutex_);

    if (!_context)
        return getConnection(DEFAULT_CONNECTION_ID);

    std::shared_ptr<Connection> itsConnection;

    auto itsConnectionIterator = connections_.find(_context->getName());
    if (itsConnectionIterator != connections_.end()) {
        itsConnection = itsConnectionIterator->second;
    } else {
        itsConnection = std::make_shared<Connection>(_context->getName());
        if (itsConnection) {
            connections_.insert({ _context->getName(), itsConnection } );
            (void)itsConnection->connect(false);
        } else {
            return nullptr;
        }
    }

    itsConnection->attachMainLoopContext(_context);

    return itsConnection;
}

} // namespace SomeIP
} // namespace CommonAPI
