// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXYCONNECTION_HPP_
#define COMMONAPI_SOMEIP_PROXYCONNECTION_HPP_

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
        virtual void onError(const uint16_t, const uint32_t) {};
    };

    typedef Event< AvailabilityStatus > ConnectionStatusEvent;

    virtual ~ProxyConnection() {}

    virtual bool isConnected() const = 0;
    virtual void waitUntilConnected() = 0;

    virtual ConnectionStatusEvent& getConnectionStatusEvent() = 0;

    virtual bool sendMessage(const Message &message, uint32_t *allocatedSerial = NULL) const = 0;

    virtual bool sendMessageWithReplyAsync(
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
            std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
            major_version_t major,
            event_type_e eventType) = 0;

    virtual void removeEventHandler(
            service_id_t serviceId,
            instance_id_t instanceId,
            eventgroup_id_t eventGroupId,
            event_id_t eventId,
            ProxyConnection::EventHandler* eventHandler,
            major_version_t major,
            minor_version_t minor) = 0;

    virtual bool isAvailable(const Address &_address) = 0;

    virtual AvailabilityHandlerId_t registerAvailabilityHandler(
            const Address &_address, AvailabilityHandler_t _handler,
            std::weak_ptr<Proxy> _proxy, void* _data) = 0;
    virtual void unregisterAvailabilityHandler(
            const Address &_address, AvailabilityHandlerId_t _handlerId) = 0;

    virtual void registerSubscriptionHandler(const Address &_address,
            const eventgroup_id_t _eventgroup, AsyncSubscriptionHandler_t _handler) = 0;

    virtual void unregisterSubscriptionHandler(const Address &_address,
            const eventgroup_id_t _eventgroup) = 0;

    virtual void registerService(const Address &_address) = 0;
    virtual void unregisterService(const Address &_address) = 0;

    virtual void requestService(const Address &_address) = 0;

    virtual void releaseService(const Address &_address) = 0;

    virtual void registerEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event, const std::set<eventgroup_id_t> &_eventGroups,
            event_type_e _type, reliability_type_e _reliability) = 0;
    virtual void unregisterEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event) = 0;

    virtual void requestEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event, eventgroup_id_t _eventGroup, event_type_e _type,
            reliability_type_e _reliability) = 0;
    virtual void releaseEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event) = 0;

    virtual const std::shared_ptr<StubManager> getStubManager() = 0;
    virtual void setStubMessageHandler(std::function<bool(const Message&)> stubMessageHandler) = 0;
    virtual bool isStubMessageHandlerSet() = 0;

    virtual void queueSelectiveErrorHandler(service_id_t serviceId,
                                          instance_id_t instanceId) = 0;

    virtual void proxyPushMessageToMainLoop(const Message &_message,
                                  std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler) = 0;

    template<class Connection, class Function, class... Arguments>
    void proxyPushFunctionToMainLoop(Function&& _function, Arguments&& ... _args) {
        static_cast<Connection*>(this)->proxyPushFunctionToMainLoop(std::forward<Function>(_function), std::forward<Arguments>(_args) ...);
    }

    virtual const ConnectionId_t& getConnectionId() = 0;

    virtual void getAvailableInstances(service_id_t _serviceId, std::vector<std::string> *_instances) = 0;

    virtual void subscribe(
                    service_id_t serviceId,
                    instance_id_t instanceId,
                    eventgroup_id_t eventGroupId,
                    event_id_t eventId,
                    std::weak_ptr<ProxyConnection::EventHandler> _eventHandler,
                    uint32_t _tag,
                    major_version_t major) = 0;
};


} // namespace SomeIP
} // namespace CommonAPI

#endif //COMMONAPI_SOMEIP_PROXYCONNECTION_HPP_
