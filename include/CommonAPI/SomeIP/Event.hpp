// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
          const event_type_e _eventType,
          const reliability_type_e _reliabilityType,
          const bool _isLittleEndian,
          std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          handler_(),
          serviceId_(_proxy.getSomeIpAlias().getService()),
          instanceId_(_proxy.getSomeIpAlias().getInstance()),
          eventId_(_eventId),
          eventgroupId_(_eventgroupId),
          eventType_(_eventType),
          reliabilityType_(_reliabilityType),
          isLittleEndian_(_isLittleEndian),
          arguments_(_arguments) {
        proxy_.registerEvent(serviceId_, instanceId_, eventId_, eventgroupId_,
                eventType_, reliabilityType_);
    }

    virtual ~Event() {
        auto major = proxy_.getSomeIpAlias().getMajorVersion();
        auto minor = proxy_.getSomeIpAlias().getMinorVersion();
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, handler_.get(), major, minor);
        proxy_.unregisterEvent(serviceId_, instanceId_, eventId_);
    }

    virtual void onError(const uint16_t _errorCode, const uint32_t _tag) {
        this->notifySpecificError(_tag, static_cast<CommonAPI::CallStatus>(_errorCode));
    }

protected:

    class Handler : public ProxyConnection::EventHandler,
                    public std::enable_shared_from_this<Handler> {
    public:
        Handler(ProxyBase&_proxy,
                Event<Events_, Arguments_ ...>* _event) :
            proxy_(_proxy.getWeakPtr()),
            event_(_event) {

        }

        virtual void onEventMessage(const Message &_message) {
            std::lock_guard<std::mutex> itsLock(notificationMutex_);
            if (auto ptr = proxy_.lock()) {
                event_->handleEventMessage(_message, typename make_sequence<sizeof...(Arguments_)>::type());
            }
        }

        virtual void onError(const uint16_t _errorCode, const uint32_t _tag) {
            if (auto ptr = proxy_.lock()) {
                event_->onError(_errorCode, _tag);
            }
        }

    private :
        std::weak_ptr<ProxyBase> proxy_;
        Event<Events_, Arguments_ ...>* event_;
        std::mutex notificationMutex_;
    };

    virtual void onFirstListenerAdded(const Listener&) {
        if (!handler_) {
            handler_ = std::make_shared<Handler>(proxy_, this);
        }
        auto major = proxy_.getSomeIpAlias().getMajorVersion();
        proxy_.addEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_,
                eventType_, reliabilityType_, handler_, major);
    }

    virtual void onListenerAdded(const Listener &_listener, const Subscription _subscription) {
        (void)_listener;
        {
            std::lock_guard<std::mutex> itsLock(listeners_mutex_);
            listeners_.insert(_subscription);
        }

        auto major = proxy_.getSomeIpAlias().getMajorVersion();
        proxy_.subscribe(serviceId_, instanceId_, eventgroupId_, eventId_,
                handler_, _subscription, major);
    }

    virtual void onLastListenerRemoved(const Listener&) {
        auto major = proxy_.getSomeIpAlias().getMajorVersion();
        auto minor = proxy_.getSomeIpAlias().getMinorVersion();
        proxy_.removeEventHandler(serviceId_, instanceId_, eventgroupId_, eventId_, handler_.get(), major, minor);
    }

    virtual void onListenerRemoved(const Listener&) {

    }

    template <size_t ... Indices_>
    inline void handleEventMessage(const Message &_message,
                                   index_sequence<Indices_...>) {

        InputStream InputStream(_message, isLittleEndian_);
        if (SerializableArguments<Arguments_...>::deserialize(
                InputStream, std::get<Indices_>(arguments_)...)) {
            if (_message.isInitialValue()) {
                std::set<Subscription> subscribers;
                {
                    std::lock_guard<std::mutex> itsLock(listeners_mutex_);
                    subscribers = listeners_;
                    listeners_.clear();
                }
                for(auto const &subscription : subscribers) {
                    if(!_message.isValidCRC()) {
                        this->notifySpecificError(subscription, CommonAPI::CallStatus::INVALID_VALUE);
                    } else {
                        this->notifySpecificListener(subscription, std::get<Indices_>(arguments_)...);
                    }
                }
            } else {
                if(!_message.isValidCRC()) {
                    this->notifyErrorListeners(CommonAPI::CallStatus::INVALID_VALUE);
                } else {
                    {
                        std::lock_guard<std::mutex> itsLock(listeners_mutex_);
                        listeners_.clear();
                    }
                    this->notifyListeners(std::get<Indices_>(arguments_)...);
                }
            }
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
    const event_type_e eventType_;
    const reliability_type_e reliabilityType_;
    const bool isLittleEndian_;
    std::tuple<Arguments_...> arguments_;
    std::mutex listeners_mutex_;
    std::set<Subscription> listeners_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_EVENT_HPP_
