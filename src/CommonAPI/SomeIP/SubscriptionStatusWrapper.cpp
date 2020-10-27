// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <CommonAPI/SomeIP/SubscriptionStatusWrapper.hpp>

namespace CommonAPI {
namespace SomeIP {

// Public

SubscriptionStatusWrapper::SubscriptionStatusWrapper(service_id_t serviceId, instance_id_t instanceId,
        eventgroup_id_t eventgroupId, event_id_t eventId)
            : serviceId_(serviceId), instanceId_(instanceId),
              eventgroupId_(eventgroupId), eventId_(eventId) {
}

SubscriptionStatusWrapper::~SubscriptionStatusWrapper() {
}

void SubscriptionStatusWrapper::pushOnPendingHandlerQueue() {
    for (auto its_handler : allHandlers_) {
        for (uint32_t its_tag : its_handler.second.second) {
            auto its_pair = std::make_pair(its_handler.second.first, its_tag);
            pendingHandlerQueue_.push(its_pair);
        }
    }
}

bool SubscriptionStatusWrapper::pendingHandlerQueueEmpty() {
    return pendingHandlerQueue_.size() == 0;
}

std::pair<std::weak_ptr<ProxyConnection::EventHandler>, uint32_t >
    SubscriptionStatusWrapper::popAndFrontPendingHandler() {
    auto result = pendingHandlerQueue_.front();
    pendingHandlerQueue_.pop();
    return result;
}

void SubscriptionStatusWrapper::addHandler(const std::weak_ptr<ProxyConnection::EventHandler> _handler,
        const uint32_t _tag) {
    auto its_handler = _handler.lock();
    if (its_handler) {
        auto found_handler = allHandlers_.find(its_handler.get());
        if (found_handler != allHandlers_.end()) {
            found_handler->second.second.insert(_tag);
        } else {
            std::set<uint32_t> tags;
            tags.insert(_tag);
            allHandlers_[its_handler.get()] = std::make_pair(_handler, tags);
        }
        pushOnPendingHandlerQueue(_handler, _tag);
    }
}

void SubscriptionStatusWrapper::removeHandler(
        ProxyConnection::EventHandler* _handler) {
    allHandlers_.erase(_handler);
}

bool SubscriptionStatusWrapper::hasHandler(ProxyConnection::EventHandler* _handler,
        uint32_t _tag) {
    auto its_handler = allHandlers_.find(_handler);
    if (its_handler != allHandlers_.end()) {
        auto its_tag = its_handler->second.second.find(_tag);
        if (its_tag != its_handler->second.second.end()) {
            return true;
        }
    }
    return false;
}

// Private

void SubscriptionStatusWrapper::pushOnPendingHandlerQueue(
        const std::weak_ptr<ProxyConnection::EventHandler> _handler,
        const uint32_t _tag) {
    pendingHandlerQueue_.push(std::make_pair(_handler, _tag));
}

} // namespace SomeIP
} // namespace CommonAPI
