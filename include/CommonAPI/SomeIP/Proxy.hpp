// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_HPP_
#define COMMONAPI_SOMEIP_PROXY_HPP_

#include <list>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/SomeIP/Address.hpp>
#include <CommonAPI/SomeIP/ProxyBase.hpp>
#include <CommonAPI/SomeIP/ProxyHelper.hpp>
#include <CommonAPI/SomeIP/Attribute.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

class ProxyStatusEventHelper: public ProxyStatusEvent {
    friend class Proxy;

 public:
    ProxyStatusEventHelper(Proxy *_proxy);

 protected:
    virtual void onListenerAdded(const Listener &_listener, const Subscription _subscription);
    virtual void onListenerRemoved(const Listener &_listener, const Subscription _subscription);

    Proxy *proxy_;

    std::recursive_mutex listenersMutex_;
    std::vector<std::pair<ProxyStatusEvent::Subscription, ProxyStatusEvent::Listener>> listeners_;
};

class Factory;
class ProxyConnection;

class COMMONAPI_EXPORT_CLASS_EXPLICIT Proxy
        : public ProxyBase,
          public std::enable_shared_from_this<Proxy> {
public:
    COMMONAPI_EXPORT Proxy(const Address &_address,
          const std::shared_ptr<ProxyConnection> &_connection);
    COMMONAPI_EXPORT virtual ~Proxy();

    COMMONAPI_EXPORT bool init();

    COMMONAPI_EXPORT virtual const Address &getSomeIpAddress() const;

    COMMONAPI_EXPORT virtual bool isAvailable() const;
    COMMONAPI_EXPORT virtual bool isAvailableBlocking() const;
    COMMONAPI_EXPORT virtual std::future<AvailabilityStatus> isAvailableAsync(
                isAvailableAsyncCallback _callback,
                const CallInfo *_info) const;

    COMMONAPI_EXPORT AvailabilityStatus getAvailabilityStatus() const;

    COMMONAPI_EXPORT virtual ProxyStatusEvent& getProxyStatusEvent();
    COMMONAPI_EXPORT virtual InterfaceVersionAttribute& getInterfaceVersionAttribute();

    COMMONAPI_EXPORT static void notifySpecificListener(std::weak_ptr<Proxy> _proxy,
                                                         const ProxyStatusEvent::Listener &_listener,
                                                         const ProxyStatusEvent::Subscription _subscription);

    COMMONAPI_EXPORT virtual const Address &getSomeIpAlias() const;

private:
    COMMONAPI_EXPORT Proxy(const Proxy&) = delete;

    COMMONAPI_EXPORT void onServiceInstanceStatus(std::shared_ptr<Proxy> _proxy,
                                                         uint16_t serviceId,
                                                         uint16_t instanceId,
                                                         bool isAvailbale,
                                                         void* _data);

    COMMONAPI_EXPORT void availabilityTimeoutThreadHandler() const;

private:
    Address address_;
    Address alias_;

    ProxyStatusEventHelper proxyStatusEvent_;

    AvailabilityStatus availabilityStatus_;
    AvailabilityHandlerId_t availabilityHandlerId_;
    ReadonlyAttribute<InterfaceVersionAttribute> interfaceVersionAttribute_;

    mutable std::mutex availabilityMutex_;
    mutable std::condition_variable availabilityCondition_;

    mutable std::shared_ptr<std::thread> availabilityTimeoutThread_;
    mutable std::mutex availabilityTimeoutThreadMutex_;
    mutable std::mutex timeoutsMutex_;
    mutable std::condition_variable availabilityTimeoutCondition_;

    typedef std::tuple<
                std::chrono::steady_clock::time_point,
                isAvailableAsyncCallback,
                std::promise<AvailabilityStatus>
                > AvailabilityTimeout_t;
    mutable std::list<AvailabilityTimeout_t> timeouts_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXY_HPP_
