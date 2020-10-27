// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_SOMEIP_SUBSCRIPTIONSTATUSWRAPPER_HPP_
#define COMMONAPI_SOMEIP_SUBSCRIPTIONSTATUSWRAPPER_HPP_

#include <queue>
#include <utility>
#include <memory>

#include "Types.hpp"
#include "ProxyConnection.hpp"

namespace CommonAPI {
namespace SomeIP {

class SubscriptionStatusWrapper {
    public:
        SubscriptionStatusWrapper(service_id_t serviceId, instance_id_t instanceId,
                eventgroup_id_t eventgroupId, event_id_t eventId);

        virtual ~SubscriptionStatusWrapper();

        void pushOnPendingHandlerQueue();

        void addHandler(const std::weak_ptr<ProxyConnection::EventHandler> _handler,
                const uint32_t _tag);

        void removeHandler(ProxyConnection::EventHandler* _handler);

        bool hasHandler(ProxyConnection::EventHandler* _handler, uint32_t _tag);

        bool pendingHandlerQueueEmpty();

        std::pair<std::weak_ptr<ProxyConnection::EventHandler>, uint32_t> popAndFrontPendingHandler();

        inline service_id_t get_service() {
            return serviceId_;
        }

        inline service_id_t get_instance() {
            return instanceId_;
        }

    private:
        void pushOnPendingHandlerQueue(const std::weak_ptr<ProxyConnection::EventHandler> _handler,
                const uint32_t _tag);

        service_id_t serviceId_;
        instance_id_t instanceId_;
        eventgroup_id_t eventgroupId_;
        event_id_t eventId_;

        std::queue<std::pair<std::weak_ptr<ProxyConnection::EventHandler>,
                                            uint32_t> > pendingHandlerQueue_;

        std::map<ProxyConnection::EventHandler*,
            std::pair<std::weak_ptr<ProxyConnection::EventHandler>, std::set<uint32_t> > > allHandlers_;
};

}
}
#endif // COMMONAPI_SOMEIP_SUBSCRIPTIONSTATUSWRAPPER_HPP_
