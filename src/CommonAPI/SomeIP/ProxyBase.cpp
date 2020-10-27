// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/ProxyBase.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>

namespace CommonAPI {
namespace SomeIP {

ProxyBase::ProxyBase(const std::shared_ptr< ProxyConnection > &connection)
    : commonApiDomain_("local"),
      connection_(connection),
      addressTranslator_(AddressTranslator::get()) {
}

Message ProxyBase::createMethodCall(const method_id_t _method, bool _reliable) const {
    return Message::createMethodCall(getSomeIpAlias(),
            AddressTranslator::get()->getMethodAlias(getSomeIpAddress(), _method), _reliable);
}

void ProxyBase::addEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        event_type_e eventType,
        reliability_type_e reliabilityType,
        std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
        major_version_t major) {
    event_id_t itsEventId = AddressTranslator::get()->getMethodAlias(getSomeIpAddress(), eventId);
    {
        std::lock_guard<std::mutex> itsLock(eventHandlerAddedMutex_);
        eventHandlerAdded_.insert(itsEventId);
    }
    eventgroup_id_t itsEventGroupId = AddressTranslator::get()->getEventgroupAlias(getSomeIpAddress(), eventGroupId);
    connection_->addEventHandler(serviceId, instanceId, itsEventGroupId, itsEventId,
            eventHandler, major, eventType);
    connection_->requestEvent(serviceId, instanceId, itsEventId, itsEventGroupId,
            eventType, reliabilityType);
}

void ProxyBase::removeEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler,
        major_version_t major,
        minor_version_t minor) {
    bool found(false);
    event_id_t itsEventId = AddressTranslator::get()->getMethodAlias(getSomeIpAddress(), eventId);
    {
        std::lock_guard<std::mutex> itsLock(eventHandlerAddedMutex_);
        if (eventHandlerAdded_.find(itsEventId) != eventHandlerAdded_.end()) {
            found = true;
            eventHandlerAdded_.erase(itsEventId);
        }
    }
    if (found) {
        eventgroup_id_t itsEventGroupId = AddressTranslator::get()->getEventgroupAlias(getSomeIpAddress(), eventGroupId);
        connection_->releaseEvent(serviceId, instanceId, itsEventId);
        connection_->removeEventHandler(serviceId, instanceId, itsEventGroupId, itsEventId, eventHandler, major, minor);
    }
}

void ProxyBase::registerEvent(
            service_id_t serviceId,
            instance_id_t instanceId,
            event_id_t eventId,
            eventgroup_id_t eventGroupId,
            event_type_e eventType,
            reliability_type_e reliabilityType) {
    event_id_t itsEventId = AddressTranslator::get()->getMethodAlias(getSomeIpAddress(), eventId);
    eventgroup_id_t itsEventGroupId = AddressTranslator::get()->getEventgroupAlias(getSomeIpAddress(), eventGroupId);
    connection_->requestEvent(serviceId, instanceId, itsEventId,
            itsEventGroupId, eventType, reliabilityType);
}

void ProxyBase::unregisterEvent(
            service_id_t serviceId,
            instance_id_t instanceId,
            event_id_t eventId) {
    event_id_t itsEventId = AddressTranslator::get()->getMethodAlias(getSomeIpAddress(), eventId);
    connection_->releaseEvent(serviceId, instanceId, itsEventId);
}

void ProxyBase::subscribe(
             service_id_t serviceId,
             instance_id_t instanceId,
             eventgroup_id_t eventGroupId,
             event_id_t eventId,
             std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
             uint32_t tag,
             major_version_t major) {
    event_id_t itsEventId = AddressTranslator::get()->getMethodAlias(getSomeIpAddress(), eventId);
    eventgroup_id_t itsEventGroupId = AddressTranslator::get()->getEventgroupAlias(getSomeIpAddress(), eventGroupId);
    connection_->subscribe(serviceId, instanceId, itsEventGroupId, itsEventId,
            eventHandler, tag, major);
}

std::weak_ptr<ProxyBase> ProxyBase::getWeakPtr() {
    if(auto p = dynamic_cast<CommonAPI::SomeIP::Proxy*>(this)) {
        return p->shared_from_this();
    }
    return std::weak_ptr<ProxyBase>();
}

} // namespace SomeIP
} // namespace CommonAPI
