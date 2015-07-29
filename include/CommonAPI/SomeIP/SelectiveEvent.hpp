// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_SELECTIVE_EVENT_HPP_
#define COMMONAPI_SOMEIP_SELECTIVE_EVENT_HPP_

#include <CommonAPI/SomeIP/Event.hpp>

namespace CommonAPI {
namespace SomeIP {

template<typename _Event, typename... _Arguments>
class SelectiveEvent: public Event<_Event, _Arguments...> {
public:
    typedef typename Event<_Event, _Arguments...>::Listener Listener;
    typedef Event<_Event, _Arguments...> EventBase;

    SelectiveEvent(ProxyBase &_proxy,
            const eventgroup_id_t _eventgroupId,
            const event_id_t _eventId,
            std::tuple<_Arguments...> _arguments)
        : EventBase(_proxy, _eventgroupId, _eventId, _arguments) {
    }

    virtual ~SelectiveEvent() {}

protected:
    virtual void onFirstListenerAdded(const Listener&) {
        Message message = this->proxy_.createMethodCall(0, false);
        this->proxy_.sendIdentifyRequest(message);

        // TODO: can we send both ID request calls here?!
        message = this->proxy_.createMethodCall(0, true);
                this->proxy_.sendIdentifyRequest(message);

        this->proxy_.addEventHandler(this->serviceId_, this->instanceId_, this->eventgroupId_,
                this->eventId_, this);
    }

    virtual void onLastListenerRemoved(const Listener&) {
        this->proxy_.removeEventHandler(this->serviceId_, this->instanceId_, this->eventgroupId_,
                this->eventId_, this);
    }
};


} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_SELECTIVE_EVENT_HPP_
