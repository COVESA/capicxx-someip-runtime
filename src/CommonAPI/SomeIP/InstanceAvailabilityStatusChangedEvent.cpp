// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
                observedInterfaceServiceId_ (_serviceId),
                availabilityHandlerId_(0) {
    Address wildcardAddress(observedInterfaceServiceId_, vsomeip::ANY_INSTANCE, vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);
    proxy_.getConnection()->requestService(wildcardAddress);
}

InstanceAvailabilityStatusChangedEvent::~InstanceAvailabilityStatusChangedEvent() {
    Address wildcardAddress(observedInterfaceServiceId_, vsomeip::ANY_INSTANCE,  vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);

    proxy_.getConnection()->unregisterAvailabilityHandler(
            wildcardAddress, availabilityHandlerId_);

    proxy_.getConnection()->releaseService(wildcardAddress);
}

void
InstanceAvailabilityStatusChangedEvent::onEventMessage(const Message &) {
}

void
InstanceAvailabilityStatusChangedEvent::onServiceInstanceStatus(
        std::shared_ptr<Proxy> _proxy,
        service_id_t _serviceId,
        instance_id_t _instanceId,
        bool _isAvailable,
        void* _data) {
    (void)_proxy;
    InstanceAvailabilityStatusChangedEvent* itsSelf =
            static_cast<InstanceAvailabilityStatusChangedEvent*>(_data);
    if(_instanceId != vsomeip::ANY_INSTANCE) {
        Address service(_serviceId, _instanceId);
        CommonAPI::Address capiAddressNewService;
        AddressTranslator::get()->translate(service, capiAddressNewService);

        if(capiAddressNewService.getInterface() == itsSelf->observedInterfaceName_) {
            if(_isAvailable) {
                if (itsSelf->addInstance(capiAddressNewService, _instanceId)) {
                    itsSelf->notifyListeners(capiAddressNewService.getAddress(),
                            CommonAPI::AvailabilityStatus::AVAILABLE);
                }
            } else {
                if (itsSelf->removeInstance(capiAddressNewService, _instanceId)) {
                    itsSelf->notifyListeners(capiAddressNewService.getAddress(),
                            CommonAPI::AvailabilityStatus::NOT_AVAILABLE);
               }
            }
        } else {
            COMMONAPI_ERROR(
                    itsSelf->observedInterfaceName_ + " doesn't match "
                            + capiAddressNewService.getInterface());
        }
    }
}

void
InstanceAvailabilityStatusChangedEvent::getAvailableInstances(
        std::vector<std::string> *_instances) {
    proxy_.getConnection()->getAvailableInstances(observedInterfaceServiceId_, _instances);
}

void
InstanceAvailabilityStatusChangedEvent::getInstanceAvailabilityStatus(
        const std::string &_instanceAddress,
        CommonAPI::AvailabilityStatus *_availablityStatus) {
    CommonAPI::Address capiAddress(_instanceAddress);
    Address address;
    AddressTranslator::get()->translate(capiAddress, address);
    if(proxy_.getConnection()->isAvailable(address))
        *_availablityStatus = CommonAPI::AvailabilityStatus::AVAILABLE;
    else
        *_availablityStatus = CommonAPI::AvailabilityStatus::NOT_AVAILABLE;
}

void
InstanceAvailabilityStatusChangedEvent::onFirstListenerAdded(
        const Listener &_listener) {
    (void)_listener;
    Address wildcardAddress(observedInterfaceServiceId_, vsomeip::ANY_INSTANCE, vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);
    std::weak_ptr<Proxy> itsProxy = proxy_.shared_from_this();
    availabilityHandlerId_ = proxy_.getConnection()->registerAvailabilityHandler(
                                wildcardAddress, onServiceInstanceStatus, itsProxy, this);
}

void
InstanceAvailabilityStatusChangedEvent::onLastListenerRemoved(
        const Listener &) {
    // Destruktor of InstanceAvailabilityStatusChangedEvent will remove the availability handler
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
