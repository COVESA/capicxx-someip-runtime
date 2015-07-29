// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_EVENT_HPP_
#define COMMONAPI_SOMEIP_EVENT_HPP_

#include <CommonAPI/Event.hpp>
#include <CommonAPI/Logger.hpp>

namespace CommonAPI {
namespace SomeIP {

template <typename _Event, typename... _Arguments>
class Event: public _Event, public ProxyConnection::EventHandler {
public:
    typedef typename _Event::ArgumentsTuple ArgumentsTuple;
    typedef typename _Event::Listener Listener;

    Event(ProxyBase &_proxy,
          const eventgroup_id_t _eventgroupId,
          const event_id_t _eventId,
          std::tuple<_Arguments...> _arguments)
        : proxy_(_proxy),
          serviceId_(_proxy.getSomeIpAddress().getService()),
          instanceId_(_proxy.getSomeIpAddress().getInstance()),
          eventgroupId_(_eventgroupId),
          eventId_(_eventId),
          arguments_(_arguments) {
    }

    virtual ~Event() {
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, this);
    }

    virtual void onEventMessage(const Message &_message) {
        return handleEventMessage(_message, typename make_sequence<sizeof...(_Arguments)>::type());
    }

protected:
    virtual void onFirstListenerAdded(const Listener&) {
        proxy_.addEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, this);
    }

    virtual void onLastListenerRemoved(const Listener&) {
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, this);
    }

    template<int ... _Indices>
    inline void handleEventMessage(const Message &_message,
                                                 index_sequence<_Indices...>) {
        InputStream InputStream(_message);
        if (SerializableArguments<_Arguments...>::deserialize(
                InputStream, std::get<_Indices>(arguments_)...)) {
            this->notifyListeners(std::get<_Indices>(arguments_)...);
        } else {
            COMMONAPI_ERROR("CommonAPI::SomeIP::Event: deserialization failed!");
        }
    }

    ProxyBase &proxy_;
    const service_id_t serviceId_;
    const instance_id_t instanceId_;
    const eventgroup_id_t eventgroupId_;
    const event_id_t eventId_;
    std::tuple<_Arguments...> arguments_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_EVENT_HPP_

