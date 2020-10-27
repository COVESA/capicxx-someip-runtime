// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXYMANAGER_HPP_
#define COMMONAPI_SOMEIP_PROXYMANAGER_HPP_

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Event.hpp>
#include <CommonAPI/ProxyManager.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>
#include <CommonAPI/SomeIP/Factory.hpp>
#include <CommonAPI/SomeIP/InstanceAvailabilityStatusChangedEvent.hpp>

namespace CommonAPI {
namespace SomeIP {

class COMMONAPI_EXPORT_CLASS_EXPLICIT ProxyManager: public CommonAPI::ProxyManager {
public:
    COMMONAPI_EXPORT ProxyManager(Proxy &_proxy,
                     const std::string &_interfaceName,
                     const service_id_t &_serviceId);

    COMMONAPI_EXPORT ~ProxyManager();

    COMMONAPI_EXPORT const std::string &getDomain() const;
    COMMONAPI_EXPORT const std::string &getInterface() const;
    COMMONAPI_EXPORT const ConnectionId_t &getConnectionId() const;

    COMMONAPI_EXPORT virtual void getAvailableInstances( CommonAPI::CallStatus &_callStatus, std::vector<std::string> &_instances);
    COMMONAPI_EXPORT virtual std::future<CallStatus> getAvailableInstancesAsync(GetAvailableInstancesCallback _callback);

    COMMONAPI_EXPORT virtual void getInstanceAvailabilityStatus(const std::string &_instance,
                                               CallStatus &_callStatus,
                                               AvailabilityStatus &_availabilityStatus);

    COMMONAPI_EXPORT  virtual std::future<CallStatus> getInstanceAvailabilityStatusAsync(
                                        const std::string &_instance,
                                        GetInstanceAvailabilityStatusCallback _callback);

    COMMONAPI_EXPORT virtual InstanceAvailabilityStatusChangedEvent& getInstanceAvailabilityStatusChangedEvent();

private:
    Proxy &proxy_;
    const std::string interfaceId_;
    std::shared_ptr<CommonAPI::SomeIP::InstanceAvailabilityStatusChangedEvent> instanceAvailabilityStatusEvent_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXYMANAGER_HPP_
