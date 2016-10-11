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
        const std::string &_interfaceName) :
                observedInterfaceName_(_interfaceName) {
}

InstanceAvailabilityStatusChangedEvent::~InstanceAvailabilityStatusChangedEvent() {}

void
InstanceAvailabilityStatusChangedEvent::onEventMessage(const Message &) {
}

void
InstanceAvailabilityStatusChangedEvent::onServiceInstanceStatus(
        service_id_t _serviceId,
        instance_id_t _instanceId,
        bool _isAvailable) {
    if(_instanceId != vsomeip::ANY_INSTANCE) {
        Address service(_serviceId, _instanceId);
        CommonAPI::Address capiAddressNewService;
        AddressTranslator::get()->translate(service, capiAddressNewService);
        if(capiAddressNewService.getInterface() == observedInterfaceName_) {
            if(_isAvailable) {
                if (addInstance(capiAddressNewService, _instanceId)) {
                    notifyListeners(capiAddressNewService.getAddress(),
                            CommonAPI::AvailabilityStatus::AVAILABLE);
                }
            } else {
                if (removeInstance(capiAddressNewService, _instanceId)) {
                    notifyListeners(capiAddressNewService.getAddress(),
                            CommonAPI::AvailabilityStatus::NOT_AVAILABLE);
                }
            }
        } else {
            COMMONAPI_ERROR(
                    observedInterfaceName_ + " doesn't match "
                            + capiAddressNewService.getInterface())
        }
    }
}

void
InstanceAvailabilityStatusChangedEvent::getAvailableInstances(
        std::vector<std::string> *_instances) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    _instances->clear();
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
    auto instance = instancesBackward_.find(capiAddress.getInstance());
    if(instance != instancesBackward_.end())
        *_availablityStatus = CommonAPI::AvailabilityStatus::AVAILABLE;
    else
        *_availablityStatus = CommonAPI::AvailabilityStatus::NOT_AVAILABLE;
}

void
InstanceAvailabilityStatusChangedEvent::onFirstListenerAdded(
        const Listener &) {
    // Proxy Manager will add the availability handler
    // As its needed anyways even if a user never subscribes on this event
}

void
InstanceAvailabilityStatusChangedEvent::onLastListenerRemoved(
        const Listener &) {
    // Proxy Manager will remove the availability handler
    // As its needed anyways even if a user never subscribes on this event
}

bool
InstanceAvailabilityStatusChangedEvent::addInstance(
        const CommonAPI::Address &_address,
        const instance_id_t &_instanceId) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    if (instancesForward_.find(_instanceId) == instancesForward_.end()) {
        instancesForward_[_instanceId] = _address.getInstance();
        instancesBackward_[_address.getInstance()] = _instanceId;
        return true;
    }
    return false;
}

bool
InstanceAvailabilityStatusChangedEvent::removeInstance(
        const CommonAPI::Address &_address,
        const instance_id_t &_instanceId) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    if(instancesForward_.find(_instanceId) != instancesForward_.end()) {
        instancesForward_.erase(_instanceId);
        instancesBackward_.erase(_address.getInstance());
        return true;
    }
    return false;
}


} // namespace SomeIP
} // namespace CommonAPI
