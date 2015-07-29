// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_SOMEIP_INSTANCEAVAILABILITYSTATUSCHANGEDEVENT_HPP_
#define COMMONAPI_SOMEIP_INSTANCEAVAILABILITYSTATUSCHANGEDEVENT_HPP_

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#include <CommonAPI/ProxyManager.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>
#include <CommonAPI/Types.hpp>

#include <mutex>
#include <map>

namespace CommonAPI {
namespace SomeIP {

class InstanceAvailabilityStatusChangedEvent :
        public CommonAPI::ProxyManager::InstanceAvailabilityStatusChangedEvent,
        public ProxyConnection::EventHandler,
        public std::enable_shared_from_this<InstanceAvailabilityStatusChangedEvent> {
public:
    COMMONAPI_EXPORT InstanceAvailabilityStatusChangedEvent(Proxy &_proxy,
                                           const std::string &_interfaceName,
                                           const service_id_t &_serviceId);
	COMMONAPI_EXPORT virtual ~InstanceAvailabilityStatusChangedEvent();

	COMMONAPI_EXPORT virtual void onEventMessage(const Message& _message);
	COMMONAPI_EXPORT void onServiceInstanceStatus(service_id_t _serviceId,
                                 instance_id_t _instanceId,
                                 bool _isAvailable);
	COMMONAPI_EXPORT void getAvailableInstances(std::vector<std::string> *_instances);
	COMMONAPI_EXPORT void getInstanceAvailabilityStatus(const std::string &_instanceAddress,
                                       CommonAPI::AvailabilityStatus *_availablityStatus);

protected:
	COMMONAPI_EXPORT virtual void onFirstListenerAdded(const Listener& listener);
	COMMONAPI_EXPORT virtual void onLastListenerRemoved(const Listener& listener);

private:
	COMMONAPI_EXPORT void addInstance(const CommonAPI::Address &_address,
                     const instance_id_t &_instanceId);
	COMMONAPI_EXPORT void removeInstance(const CommonAPI::Address &_address,
                        const instance_id_t &_instanceId);

private:
    Proxy& proxy_;
    std::string observedInterfaceName_;
    service_id_t observedInterfaceServiceId_;
    std::shared_ptr<ProxyConnection> proxyConnection_;
    std::mutex instancesMutex_;
    std::map<instance_id_t, std::string> instancesForward_;
    std::map<std::string, instance_id_t> instancesBackward_;
    AvailabilityHandlerId_t availabilityHandlerId_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif /* COMMONAPI_SOMEIP_INSTANCEAVAILABILITYSTATUSCHANGEDEVENT_HPP_ */
