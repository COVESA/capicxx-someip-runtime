// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_FACTORY_HPP_
#define COMMONAPI_SOMEIP_FACTORY_HPP_

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <CommonAPI/Export.hpp>

#include <CommonAPI/Factory.hpp>
#include <CommonAPI/SomeIP/Configuration.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {

class Runtime;

namespace SomeIP {

class Address;
class AddressTranslator;
class Proxy;
class ProxyConnection;
class StubAdapter;

typedef void (*InterfaceInitFunction)(void);

typedef std::shared_ptr<Proxy>
(*ProxyCreateFunction)(
    const Address &_address,
    const std::shared_ptr< ProxyConnection > &_connection
);

typedef std::shared_ptr<StubAdapter>
(*StubAdapterCreateFunction) (
    const Address &_address,
    const std::shared_ptr< ProxyConnection > &connection,
    const std::shared_ptr< StubBase > &stub
);

class Factory : public CommonAPI::Factory {
public:
    COMMONAPI_EXPORT static std::shared_ptr<Factory> get();

    COMMONAPI_EXPORT Factory();
    COMMONAPI_EXPORT virtual ~Factory();

    COMMONAPI_EXPORT void init();

    COMMONAPI_EXPORT void registerProxyCreateMethod(
            const std::string &_interface, ProxyCreateFunction _function);

    COMMONAPI_EXPORT void registerStubAdapterCreateMethod(
            const std::string &_interface,
            StubAdapterCreateFunction _function);

    COMMONAPI_EXPORT std::shared_ptr<CommonAPI::Proxy> createProxy(
            const std::string &_domain,
            const std::string &_interface, const std::string &_instance,
            const ConnectionId_t &_connectionId);
    COMMONAPI_EXPORT std::shared_ptr<CommonAPI::Proxy> createProxy(
            const std::string &_domain,
            const std::string &_interface, const std::string &_instance,
            std::shared_ptr<MainLoopContext> _context);

    COMMONAPI_EXPORT bool registerStub(const std::string &_domain,
                      const std::string &_interface,
                      const std::string &_instance,
                      std::shared_ptr<CommonAPI::StubBase> _stub,
                      const ConnectionId_t &_connectionId);
    COMMONAPI_EXPORT bool registerStub(const std::string &_domain,
                      const std::string &_interface,
                      const std::string &_instance,
                      std::shared_ptr<CommonAPI::StubBase> _stub,
                      std::shared_ptr<CommonAPI::MainLoopContext> _context);

    COMMONAPI_EXPORT bool unregisterStub(const std::string &_domain,
                        const std::string &_interface,
                        const std::string &_instance);

    // Services
    COMMONAPI_EXPORT bool isRegisteredService(const std::string &_address);

    // Managed services
    COMMONAPI_EXPORT std::shared_ptr<StubAdapter> createStubAdapter(
            const std::shared_ptr<StubBase> &_stub,
            const std::string &_interface, const Address &_address,
            const std::shared_ptr<ProxyConnection> &_connection);
    COMMONAPI_EXPORT bool registerManagedService(
            const std::shared_ptr<StubAdapter> &_adapter);
    COMMONAPI_EXPORT bool unregisterManagedService(
            const std::string &_address);

    COMMONAPI_EXPORT void decrementConnection(
            std::shared_ptr<ProxyConnection> _connection);
    COMMONAPI_EXPORT void releaseConnection(
            const ConnectionId_t& _connectionId);

    // Initialization
    COMMONAPI_EXPORT void registerInterface(InterfaceInitFunction _function);

    static std::weak_ptr<CommonAPI::Runtime> runtime_;

private:
    COMMONAPI_EXPORT void incrementConnection(
            std::shared_ptr<ProxyConnection> _connection);
    COMMONAPI_EXPORT std::shared_ptr<Connection> getConnection(
            const ConnectionId_t &);
    COMMONAPI_EXPORT std::shared_ptr<Connection> getConnection(
            std::shared_ptr<MainLoopContext>);
    COMMONAPI_EXPORT bool registerStubAdapter(std::shared_ptr<StubAdapter>);

private:
    std::map<ConnectionId_t, std::shared_ptr<Connection>> connections_;
    std::recursive_mutex connectionMutex_;

    std::map<std::string, ProxyCreateFunction> proxyCreateFunctions_;
    std::map<std::string,
        StubAdapterCreateFunction> stubAdapterCreateFunctions_;

    std::map<std::string, std::shared_ptr<StubAdapter>> services_;
    std::mutex servicesMutex_;

    std::list<InterfaceInitFunction> initializers_;
    std::mutex initializerMutex_;
    bool isInitialized_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_FACTORY_HPP_
