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

#include <set>

namespace CommonAPI {
namespace SomeIP {

template <typename Events_, typename... Arguments_>
class Event: public Events_  {
public:
    typedef typename Events_::ArgumentsTuple ArgumentsTuple;
    typedef typename Events_::Listener Listener;
    typedef typename Events_::Subscription Subscription;

    Event(ProxyBase &_proxy,
          const eventgroup_id_t _eventgroupId,
          const event_id_t _eventId,
          bool _isField,
          bool _isLittleEndian,
          std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          handler_(std::make_shared<Handler>(_proxy, this)),
          serviceId_(_proxy.getSomeIpAddress().getService()),
          instanceId_(_proxy.getSomeIpAddress().getInstance()),
          eventId_(_eventId),
          eventgroupId_(_eventgroupId),
          isField_(_isField),
          isLittleEndian_(_isLittleEndian),
          getMethodId_(0),
          getReliable_(false),
          arguments_(_arguments) {
    }

    Event(ProxyBase &_proxy,
          const eventgroup_id_t _eventgroupId,
          const event_id_t _eventId,
          bool _isField,
          const bool _isLittleEndian,
          const method_id_t _methodId,
          const bool _getReliable,
          std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          handler_(std::make_shared<Handler>(_proxy, this)),
          serviceId_(_proxy.getSomeIpAddress().getService()),
          instanceId_(_proxy.getSomeIpAddress().getInstance()),
          eventId_(_eventId),
          eventgroupId_(_eventgroupId),
          isField_(_isField),
          isLittleEndian_(_isLittleEndian),
          getMethodId_(_methodId),
          getReliable_(_getReliable),
          arguments_(_arguments) {
    }

    virtual ~Event() {
        auto major = proxy_.getSomeIpAddress().getMajorVersion();
        auto minor = proxy_.getSomeIpAddress().getMinorVersion();
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, handler_.get(), major, minor);
    }

    virtual void onError(const uint16_t _errorCode, const uint32_t _tag) {
        (void) _errorCode;
        (void) _tag;
    }

protected:

    class Handler : public ProxyConnection::EventHandler,
                    public std::enable_shared_from_this<Handler> {
    public:
        Handler(ProxyBase&_proxy,
                Event<Events_, Arguments_ ...>* _event) :
            proxy_(_proxy),
            event_(_event) {

        }

        virtual void onEventMessage(const Message &_message) {
            notificationMutex_.lock();
            event_->handleEventMessage(_message, typename make_sequence<sizeof...(Arguments_)>::type());
            notificationMutex_.unlock();
        }

        virtual void onInitialValueEventMessage(const Message&_message, const uint32_t tag) {
            notificationMutex_.lock();
            event_->handleEventMessage(tag, _message, typename make_sequence<sizeof...(Arguments_)>::type());
            notificationMutex_.unlock();
        }

        virtual void onError(const uint16_t _errorCode, const uint32_t _tag) {
            event_->onError(_errorCode, _tag);
        }

    private :
        ProxyBase& proxy_;
        Event<Events_, Arguments_ ...>* event_;
        std::mutex notificationMutex_;
    };

    virtual void onFirstListenerAdded(const Listener&) {
        auto major = proxy_.getSomeIpAddress().getMajorVersion();
        proxy_.addEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, isField_, handler_, major);
    }

    virtual void onListenerAdded(const Listener &_listener, const Subscription _subscription) {
        (void)_listener;
        {
            std::lock_guard<std::mutex> itsLock(listeners_mutex_);
            listeners_.insert(_subscription);
        }

        if (isField_) {
            auto major = proxy_.getSomeIpAddress().getMajorVersion();
            proxy_.getInitialEvent(serviceId_, instanceId_, eventgroupId_, eventId_, major);
        }
    }

    virtual void onLastListenerRemoved(const Listener&) {
        auto major = proxy_.getSomeIpAddress().getMajorVersion();
        auto minor = proxy_.getSomeIpAddress().getMinorVersion();
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, handler_.get(), major, minor);
    }

    virtual void onListenerRemoved(const Listener&) {

    }

    template<int ... Indices_>
    inline void handleEventMessage(const Message &_message,
                                   index_sequence<Indices_...>) {

        InputStream InputStream(_message, isLittleEndian_);
        if (SerializableArguments<Arguments_...>::deserialize(
                InputStream, std::get<Indices_>(arguments_)...)) {
            if(_message.isInitialValue()) {
                std::set<Subscription> subscribers;
                {
                    std::lock_guard<std::mutex> itsLock(listeners_mutex_);
                    subscribers = listeners_;
                    listeners_.clear();
                }

                for(auto const &subscription : subscribers) {
                    this->notifySpecificListener(subscription, std::get<Indices_>(arguments_)...);
                }
            } else {
                {
                    std::lock_guard<std::mutex> itsLock(listeners_mutex_);
                    listeners_.clear();
                }
                this->notifyListeners(std::get<Indices_>(arguments_)...);
            }
        } else {
            COMMONAPI_ERROR("CommonAPI::SomeIP::Event: deserialization failed!");
        }
    }

    template<int ... Indices_>
    inline void handleEventMessage(uint32_t _tag, const Message &_message,
                                   index_sequence<Indices_...>) {
        InputStream InputStream(_message, isLittleEndian_);
        if (SerializableArguments<Arguments_...>::deserialize(
                InputStream, std::get<Indices_>(arguments_)...)) {
            this->notifySpecificListener(_tag, std::get<Indices_>(arguments_)...);
        } else {
            COMMONAPI_ERROR("CommonAPI::SomeIP::Event: deserialization failed!");
        }
    }

    ProxyBase &proxy_;
    std::shared_ptr<Handler> handler_;

    const service_id_t serviceId_;
    const instance_id_t instanceId_;
    const event_id_t eventId_;
    const eventgroup_id_t eventgroupId_;
    const bool isField_;
    const bool isLittleEndian_;
    const method_id_t getMethodId_;
    const bool getReliable_;
    std::tuple<Arguments_...> arguments_;
    std::mutex listeners_mutex_;
    std::set<Subscription> listeners_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_EVENT_HPP_

