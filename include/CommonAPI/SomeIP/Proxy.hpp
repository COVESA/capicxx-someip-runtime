// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_HPP_
#define COMMONAPI_SOMEIP_PROXY_HPP_

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
    virtual void onListenerAdded(const Listener &_listener);

    Proxy *proxy_;
};

class Factory;
class ProxyConnection;

class Proxy
		: public ProxyBase,
		  public std::enable_shared_from_this<Proxy> {
public:
	COMMONAPI_EXPORT Proxy(const Address &_address,
          const std::shared_ptr<ProxyConnection> &_connection, bool hasSelective = false);
	COMMONAPI_EXPORT virtual ~Proxy();

	COMMONAPI_EXPORT void init();

	COMMONAPI_EXPORT virtual const Address &getSomeIpAddress() const;

	COMMONAPI_EXPORT virtual bool isAvailable() const;
	COMMONAPI_EXPORT virtual bool isAvailableBlocking() const;

	COMMONAPI_EXPORT virtual ProxyStatusEvent& getProxyStatusEvent();
	COMMONAPI_EXPORT virtual InterfaceVersionAttribute& getInterfaceVersionAttribute();

private:
	COMMONAPI_EXPORT Proxy(const Proxy&) = delete;

	COMMONAPI_EXPORT void onServiceInstanceStatus(uint16_t serviceId, uint16_t instanceId, bool isAvailbale);

private:
    Address address_;

    ProxyStatusEventHelper proxyStatusEvent_;
    ReadonlyAttribute<InterfaceVersionAttribute> interfaceVersionAttribute_;//TODO to be removed!!!

    AvailabilityStatus availabilityStatus_;
    AvailabilityHandlerId_t availabilityHandlerId_;

    mutable std::mutex availabilityMutex_;
    mutable std::condition_variable availabilityCondition_;

    bool hasSelectiveEvents_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXY_HPP_
