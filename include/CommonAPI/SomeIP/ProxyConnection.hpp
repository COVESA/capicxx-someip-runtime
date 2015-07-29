// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_CONNECTION_HPP_
#define COMMONAPI_SOMEIP_PROXY_CONNECTION_HPP_

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <CommonAPI/Attribute.hpp>
#include <CommonAPI/Event.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/SomeIP/Constants.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

class Address;
class Proxy;
class StubManager;

class ProxyConnection {
 public:
    class MessageReplyAsyncHandler {
     public:
       virtual ~MessageReplyAsyncHandler() { }
       virtual std::future< CallStatus > getFuture() = 0;
       virtual void onMessageReply(const CallStatus&, const Message&) = 0;
    };

    class EventHandler {
     public:
        virtual ~EventHandler() { }
        virtual void onEventMessage(const Message&) = 0;
    };

    typedef std::tuple< service_id_t, instance_id_t, eventgroup_id_t, event_id_t > EventHandlerIds;
    typedef std::unordered_multimap< EventHandlerIds, EventHandler * > EventHandlerTable;
    typedef EventHandlerIds EventHandlerToken;

    typedef Event< AvailabilityStatus > ConnectionStatusEvent;

    virtual ~ProxyConnection() {}

    virtual bool isConnected() const = 0;
    virtual void waitUntilConnected() = 0;

    virtual ConnectionStatusEvent& getConnectionStatusEvent() = 0;

    virtual bool sendMessage(const Message &message, uint32_t *allocatedSerial = NULL) const = 0;

    virtual std::future< CallStatus > sendMessageWithReplyAsync(
            const Message &message,
            std::unique_ptr< MessageReplyAsyncHandler > messageReplyAsyncHandler,
			const CommonAPI::CallInfo *_info) const = 0;

    virtual Message sendMessageWithReplyAndBlock(
            const Message& message,
			const CommonAPI::CallInfo *_info) const = 0;

    virtual bool sendEvent(
            const Message &message,
            uint32_t *allocatedSerial = NULL) const = 0;

    virtual bool sendEvent(
                const Message &message, client_id_t _client,
                uint32_t *allocatedSerial = NULL) const = 0;

    virtual void addEventHandler(
            service_id_t serviceId,
            instance_id_t instanceId,
            eventgroup_id_t eventGroupId,
            event_id_t eventId,
            ProxyConnection::EventHandler* eventHandler) = 0;

    virtual void removeEventHandler(
            service_id_t serviceId,
            instance_id_t instanceId,
            eventgroup_id_t eventGroupId,
            event_id_t eventId,
            ProxyConnection::EventHandler* eventHandler) = 0;

    virtual bool isAvailable(const Address &_address) = 0;

    virtual AvailabilityHandlerId_t registerAvailabilityHandler(
            const Address &_address, AvailabilityHandler_t _handler) = 0;
    virtual void unregisterAvailabilityHandler(
            const Address &_address, AvailabilityHandlerId_t _handlerId) = 0;

    virtual void registerSubsciptionHandler(const Address &_address,
    		const eventgroup_id_t _eventgroup, SubsciptionHandler_t _handler) = 0;

    virtual void unregisterSubsciptionHandler(const Address &_address,
    		const eventgroup_id_t _eventgroup) = 0;

    virtual void registerService(const Address &_address) = 0;
    virtual void unregisterService(const Address &_address) = 0;

    virtual void requestService(const Address &_address, bool _hasSelective = false) = 0;

    virtual const std::shared_ptr<StubManager> getStubManager() = 0;
    virtual void setStubMessageHandler(std::function<bool(const Message&)> stubMessageHandler) = 0;
    virtual bool isStubMessageHandlerSet() = 0;

    virtual void sendPendingSubscriptions(service_id_t serviceId, instance_id_t instanceId) const = 0;
};


} // namespace SomeIP
} // namespace CommonAPI

#endif //COMMONAPI_SOMEIP_PROXY_CONNECTION_HPP_
