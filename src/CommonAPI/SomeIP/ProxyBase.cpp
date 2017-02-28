// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/ProxyBase.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>

namespace CommonAPI {
namespace SomeIP {

ProxyBase::ProxyBase(const std::shared_ptr< ProxyConnection > &connection)
    : commonApiDomain_("local"),
      connection_(connection),
      addressTranslator_(AddressTranslator::get()) {
}

Message ProxyBase::createMethodCall(const method_id_t _method, bool _reliable) const {
    return Message::createMethodCall(getSomeIpAddress(), _method, _reliable);
}

void ProxyBase::addEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        bool isField,
        std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
        major_version_t major,
        bool _isSelective) {

    {
        std::lock_guard<std::mutex> itsLock(eventHandlerAddedMutex_);
        eventHandlerAdded_.insert(eventId);
    }
    connection_->addEventHandler(serviceId, instanceId, eventGroupId, eventId,
            eventHandler, major, isField, _isSelective);
    connection_->requestEvent(serviceId, instanceId, eventId, eventGroupId, isField);
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
    {
        std::lock_guard<std::mutex> itsLock(eventHandlerAddedMutex_);
        if (eventHandlerAdded_.find(eventId) != eventHandlerAdded_.end()) {
            found = true;
            eventHandlerAdded_.erase(eventId);
        }
    }
    if (found) {
        connection_->releaseEvent(serviceId, instanceId, eventId);
        connection_->removeEventHandler(serviceId, instanceId, eventGroupId, eventId, eventHandler, major, minor);
    }
}

void ProxyBase::subscribeForSelective(
             service_id_t serviceId,
             instance_id_t instanceId,
             eventgroup_id_t eventGroupId,
             event_id_t eventId,
             std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
             uint32_t _tag,
             major_version_t major) {
    connection_->subscribeForSelective(serviceId, instanceId, eventGroupId, eventId, eventHandler, _tag, major);
}

void ProxyBase::subscribeForSelective(
             service_id_t serviceId,
             instance_id_t instanceId,
             eventgroup_id_t eventGroupId,
             std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
             uint32_t _tag,
             major_version_t major) {
    connection_->subscribeForSelective(serviceId, instanceId, eventGroupId, vsomeip::ANY_EVENT, eventHandler, _tag, major);
}

void ProxyBase::registerEvent(
            service_id_t serviceId,
            instance_id_t instanceId,
            event_id_t eventId,
            eventgroup_id_t eventGroupId,
            bool isField) {
    connection_->requestEvent(serviceId, instanceId, eventId, eventGroupId, isField);
}

void ProxyBase::unregisterEvent(
            service_id_t serviceId,
            instance_id_t instanceId,
            event_id_t eventId) {
    connection_->releaseEvent(serviceId, instanceId, eventId);
}

} // namespace SomeIP
} // namespace CommonAPI
