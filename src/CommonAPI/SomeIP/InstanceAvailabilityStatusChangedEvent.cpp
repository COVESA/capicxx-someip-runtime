// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/InstanceAvailabilityStatusChangedEvent.hpp>

#include <CommonAPI/SomeIP/AddressTranslator.hpp>
#include <CommonAPI/Logger.hpp>

namespace CommonAPI {
namespace SomeIP {

InstanceAvailabilityStatusChangedEvent::InstanceAvailabilityStatusChangedEvent(
        Proxy &_proxy,
        const std::string &_interfaceName,
        const service_id_t &_serviceId) :
                proxy_(_proxy),
                observedInterfaceName_(_interfaceName),
                observedInterfaceServiceId_(_serviceId),
                proxyConnection_(proxy_.getConnection()),
                availabilityHandlerId_(0) {
}

InstanceAvailabilityStatusChangedEvent::~InstanceAvailabilityStatusChangedEvent() {}

void
InstanceAvailabilityStatusChangedEvent::onEventMessage(const Message& _message) {
    //TODO find out if this is necessary for SomeIP
}

void
InstanceAvailabilityStatusChangedEvent::onServiceInstanceStatus(
        service_id_t _serviceId,
        instance_id_t _instanceId,
        bool _isAvailable) {
    Address service(_serviceId, _instanceId);
    CommonAPI::Address capiAddressNewService;
    AddressTranslator::get()->translate(service, capiAddressNewService);
    if(capiAddressNewService.getInterface() == observedInterfaceName_) {
        if(_isAvailable) {
            addInstance(capiAddressNewService, _instanceId);
            notifyListeners(capiAddressNewService.getAddress(),
                    CommonAPI::AvailabilityStatus::AVAILABLE);
        } else {
            removeInstance(capiAddressNewService, _instanceId);
            notifyListeners(capiAddressNewService.getAddress(),
                    CommonAPI::AvailabilityStatus::NOT_AVAILABLE);
        }
    } else {
        COMMONAPI_ERROR(
                observedInterfaceName_ + " doesn't match "
                        + capiAddressNewService.getInterface())
    }
}

void
InstanceAvailabilityStatusChangedEvent::getAvailableInstances(
        std::vector<std::string> *_instances) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    for (const auto &instance : instancesForward_) {
        _instances->push_back(instance.second);
    }
}

void
InstanceAvailabilityStatusChangedEvent::getInstanceAvailabilityStatus(
        const std::string &_instanceAddress,
        CommonAPI::AvailabilityStatus *_availablityStatus) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    CommonAPI::Address capiAddress(_instanceAddress);
    auto instance = instancesBackward_.find(capiAddress.getAddress());
    if(instance != instancesBackward_.end())
        *_availablityStatus = CommonAPI::AvailabilityStatus::AVAILABLE;
    else
        *_availablityStatus = CommonAPI::AvailabilityStatus::NOT_AVAILABLE;
}

void
InstanceAvailabilityStatusChangedEvent::onFirstListenerAdded(
        const Listener& listener) {
    std::function<void(service_id_t, instance_id_t, bool)> availabilityHandler =
            std::bind(
                    &InstanceAvailabilityStatusChangedEvent::onServiceInstanceStatus,
                    shared_from_this(), std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3);
    Address wildcardAddress(observedInterfaceServiceId_, vsomeip::ANY_INSTANCE);
    availabilityHandlerId_ = proxyConnection_->registerAvailabilityHandler(
                                wildcardAddress, availabilityHandler);
}

void
InstanceAvailabilityStatusChangedEvent::onLastListenerRemoved(
        const Listener& listener) {
    Address wildcardAddress(observedInterfaceServiceId_, vsomeip::ANY_INSTANCE);
    proxyConnection_->unregisterAvailabilityHandler(
            wildcardAddress, availabilityHandlerId_);
}

void
InstanceAvailabilityStatusChangedEvent::addInstance(
        const CommonAPI::Address &_address,
        const instance_id_t &_instanceId) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    instancesForward_[_instanceId] = _address.getAddress();
    instancesBackward_[_address.getAddress()] = _instanceId;
}

void
InstanceAvailabilityStatusChangedEvent::removeInstance(
        const CommonAPI::Address &_address,
        const instance_id_t &_instanceId) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    instancesForward_.erase(_instanceId);
    instancesBackward_.erase(_address.getAddress());
}


} // namespace SomeIP
} // namespace CommonAPI
