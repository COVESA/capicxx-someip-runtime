// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/ProxyBase.hpp>
#include <CommonAPI/SomeIP/Message.hpp>

namespace CommonAPI {
namespace SomeIP {

ProxyBase::ProxyBase(const std::shared_ptr< ProxyConnection > &connection)
    : commonApiDomain_("local"),
      connection_(connection) {
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
        ProxyConnection::EventHandler* eventHandler,
        major_version_t major,
        bool _isSelective) {

    eventHandlerAdded_.insert(eventId);
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
    if (eventHandlerAdded_.find(eventId) != eventHandlerAdded_.end()) {
        connection_->releaseEvent(serviceId, instanceId, eventId);
        connection_->removeEventHandler(serviceId, instanceId, eventGroupId, eventId, eventHandler, major, minor);
        eventHandlerAdded_.erase(eventId);
    }
}

void ProxyBase::subscribeForSelective(
             service_id_t serviceId,
             instance_id_t instanceId,
             eventgroup_id_t eventGroupId,
             ProxyConnection::EventHandler* eventHandler,
             uint32_t _tag,
             major_version_t major) {
	connection_->subscribeForSelective(serviceId, instanceId, eventGroupId, eventHandler, _tag, major);
}

} // namespace SomeIP
} // namespace CommonAPI
