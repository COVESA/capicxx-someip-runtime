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

void ProxyBase::sendIdentifyRequest(Message& message) {
    connection_->sendMessage(message);
}

void ProxyBase::addEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler) {
    connection_->addEventHandler(serviceId, instanceId, eventGroupId, eventId, eventHandler);
}

void ProxyBase::removeEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler) {
    connection_->removeEventHandler(serviceId, instanceId, eventGroupId, eventId, eventHandler);
}

} // namespace SomeIP
} // namespace CommonAPI
