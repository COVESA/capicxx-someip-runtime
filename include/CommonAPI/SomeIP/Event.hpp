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

template <typename Events_, typename... Arguments_>
class Event: public Events_, public ProxyConnection::EventHandler {
public:
    typedef typename Events_::ArgumentsTuple ArgumentsTuple;
    typedef typename Events_::Listener Listener;
    typedef typename Events_::Subscription Subscription;

    Event(ProxyBase &_proxy,
          const eventgroup_id_t _eventgroupId,
          const event_id_t _eventId,
          bool _isField,
          std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          serviceId_(_proxy.getSomeIpAddress().getService()),
          instanceId_(_proxy.getSomeIpAddress().getInstance()),
          eventId_(_eventId),
          eventgroupId_(_eventgroupId),
          isField_(_isField),
          getMethodId_(0),
          getReliable_(false),
          arguments_(_arguments) {
    }

    Event(ProxyBase &_proxy,
          const eventgroup_id_t _eventgroupId,
          const event_id_t _eventId,
          bool _isField,
          const method_id_t _methodId,
          const bool _getReliable,
          std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          serviceId_(_proxy.getSomeIpAddress().getService()),
          instanceId_(_proxy.getSomeIpAddress().getInstance()),
          eventId_(_eventId),
          eventgroupId_(_eventgroupId),
          isField_(_isField),
          getMethodId_(_methodId),
          getReliable_(_getReliable),
          arguments_(_arguments) {
    }

    virtual ~Event() {
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, this);
    }

    virtual void onEventMessage(const Message &_message) {
        handleEventMessage(_message, typename make_sequence<sizeof...(Arguments_)>::type());
    }

    virtual void onInitialValueEventMessage(const Message&_message, const uint32_t tag) {
        handleEventMessage(tag, _message, typename make_sequence<sizeof...(Arguments_)>::type());
    }

protected:
    virtual void onFirstListenerAdded(const Listener&) {
        auto major = proxy_.getSomeIpAddress().getMajorVersion();
        proxy_.addEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, isField_, this, major);
    }

    virtual void onListenerAdded(const Listener &_listener, const Subscription _subscription) {
        (void)_listener;
        if (0 != getMethodId_) {
            Message message = proxy_.createMethodCall(getMethodId_, getReliable_);
            proxy_.getInitialEvent(serviceId_, instanceId_, message, this, _subscription);
        }
    }

    virtual void onLastListenerRemoved(const Listener&) {
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, this);
    }

    template<int ... Indices_>
    inline void handleEventMessage(const Message &_message,
                                   index_sequence<Indices_...>) {
        InputStream InputStream(_message);
        if (SerializableArguments<Arguments_...>::deserialize(
                InputStream, std::get<Indices_>(arguments_)...)) {
            this->notifyListeners(std::get<Indices_>(arguments_)...);
        } else {
            COMMONAPI_ERROR("CommonAPI::SomeIP::Event: deserialization failed!");
        }
    }

    template<int ... Indices_>
    inline void handleEventMessage(uint32_t _tag, const Message &_message,
                                   index_sequence<Indices_...>) {
        InputStream InputStream(_message);
        if (SerializableArguments<Arguments_...>::deserialize(
                InputStream, std::get<Indices_>(arguments_)...)) {
            this->notifySpecificListener(_tag, std::get<Indices_>(arguments_)...);
        } else {
            COMMONAPI_ERROR("CommonAPI::SomeIP::Event: deserialization failed!");
        }
    }

    ProxyBase &proxy_;
    const service_id_t serviceId_;
    const instance_id_t instanceId_;
    const event_id_t eventId_;
    const eventgroup_id_t eventgroupId_;
    const bool isField_;
    const method_id_t getMethodId_;
    const bool getReliable_;
    std::tuple<Arguments_...> arguments_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_EVENT_HPP_

