// Copyright (C) 2013-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

template<typename Event_, typename... Arguments_>
class SelectiveEvent: public Event<Event_, Arguments_...> {
public:
    typedef typename Event<Event_, Arguments_...>::Listener Listener;
    typedef typename Event<Event_, Arguments_...>::Handler Handler;
    typedef Event<Event_, Arguments_...> EventBase;

    SelectiveEvent(ProxyBase &_proxy,
            const eventgroup_id_t _eventgroupId,
            const event_id_t _eventId,
            bool _isField,
            const bool _isLittleEndian,
            std::tuple<Arguments_...> _arguments)
        : EventBase(_proxy, _eventgroupId, _eventId, _isField, _isLittleEndian, _arguments) {
    }

    virtual ~SelectiveEvent() {}

    virtual void onError(const uint16_t _errorCode, const uint32_t _tag) {
        this->notifySpecificError(_tag, static_cast<CommonAPI::CallStatus>(_errorCode));
    }

protected:
    virtual void onFirstListenerAdded(const Listener&) {
        if (!this->handler_) {
            this->handler_ = std::make_shared<Handler>(this->proxy_, this);
        }
        auto major = this->proxy_.getSomeIpAddress().getMajorVersion();
        this->proxy_.addEventHandler(this->serviceId_, this->instanceId_, this->eventgroupId_,
                this->eventId_, false, this->handler_, major, true);
    }

    virtual void onListenerAdded(const Listener &_listener, const uint32_t _subscription) {
        (void) _listener;
        auto major = this->proxy_.getSomeIpAddress().getMajorVersion();
        this->proxy_.subscribe(this->serviceId_, this->instanceId_, this->eventgroupId_,
                this->eventId_, this->handler_, _subscription, major);
    }

    virtual void onLastListenerRemoved(const Listener&) {
        auto major = this->proxy_.getSomeIpAddress().getMajorVersion();
        auto minor = this->proxy_.getSomeIpAddress().getMinorVersion();
        this->proxy_.removeEventHandler(this->serviceId_, this->instanceId_, this->eventgroupId_,
                this->eventId_, this->handler_.get(), major, minor);
    }
};


} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_SELECTIVE_EVENT_HPP_
