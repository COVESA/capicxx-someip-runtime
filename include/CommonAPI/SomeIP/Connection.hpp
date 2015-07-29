// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_CONNECTION_HPP_
#define COMMONAPI_SOMEIP_CONNECTION_HPP_

#include <map>
#include <set>

#include <vsomeip/application.hpp>

#include <CommonAPI/MainLoopContext.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/StubManager.hpp>
#include <CommonAPI/SomeIP/DispatchSource.hpp>
#include <CommonAPI/SomeIP/Watch.hpp>

namespace CommonAPI {
namespace SomeIP {

class Connection:
        public ProxyConnection,
        public std::enable_shared_from_this<Connection> {
public:
    Connection(const std::string &_name);
    Connection(const Connection&) = delete;
    virtual ~Connection();

    Connection& operator=(const Connection&) = delete;

    bool connect(bool startDispatchThread = true);
    void disconnect();

    virtual bool isConnected() const;
    virtual void waitUntilConnected();

    virtual ConnectionStatusEvent& getConnectionStatusEvent();

    virtual bool sendMessage(const Message& message, uint32_t* allocatedSerial =
    NULL) const;

    virtual std::future<CallStatus> sendMessageWithReplyAsync(
            const Message& message,
            std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler,
            const CommonAPI::CallInfo *_info) const;

    virtual Message sendMessageWithReplyAndBlock(const Message& message,
            const CommonAPI::CallInfo *_info) const;

    virtual bool sendEvent(const Message &message, uint32_t *allocatedSerial =
    NULL) const;

    virtual bool sendEvent(const Message &message, client_id_t _client,
            uint32_t *allocatedSerial = NULL) const;

    void addEventHandler(service_id_t serviceId, instance_id_t instanceId,
            eventgroup_id_t eventGroupId, event_id_t eventId,
            ProxyConnection::EventHandler* eventHandler);

    void removeEventHandler(service_id_t serviceId, instance_id_t instanceId,
            eventgroup_id_t eventGroupId, event_id_t eventId,
            ProxyConnection::EventHandler* eventHandler);

    virtual bool attachMainLoopContext(std::weak_ptr<MainLoopContext>);

    virtual bool isAvailable(const Address &_address);

    virtual AvailabilityHandlerId_t registerAvailabilityHandler(
            const Address &_address, AvailabilityHandler_t _handler);
    virtual void unregisterAvailabilityHandler(const Address &_address,
            AvailabilityHandlerId_t _handlerId);

    virtual void registerSubsciptionHandler(const Address &_address,
            const eventgroup_id_t _eventgroup, SubsciptionHandler_t _handler);
    virtual void unregisterSubsciptionHandler(const Address &_address,
            const eventgroup_id_t _eventgroup);

    virtual void registerService(const Address &_address);
    virtual void unregisterService(const Address &_addess);

    virtual void requestService(const Address &_address, bool _hasSelective =
            false);

    virtual const std::shared_ptr<StubManager> getStubManager();
    virtual void setStubMessageHandler(MessageHandler_t stubMessageHandler);
    virtual bool isStubMessageHandlerSet();

    virtual void processMsgQueueEntry(Watch::msgQueueEntry &_msgQueueEntry);

    virtual const ConnectionId_t& getConnectionId();

    virtual void sendPendingSubscriptions(service_id_t serviceId,
            instance_id_t instanceId) const;

private:
    void proxyReceive(const std::shared_ptr<vsomeip::message> &_message);
    void handleProxyReceive(const std::shared_ptr<vsomeip::message> &_message);
    void stubReceive(const std::shared_ptr<vsomeip::message> &_message);
    void handleStubReceive(const std::shared_ptr<vsomeip::message> &_message);
    void onConnectionEvent(event_type_e _event);
    void onAvailabilityChange(service_id_t _service, instance_id_t _instance,
            bool _is_available);
    void dispatch();
    void cleanup();

    std::thread* dispatchThread_;

    std::weak_ptr<MainLoopContext> mainLoopContext_;
    std::shared_ptr<DispatchSource> dispatchSource_;
    std::shared_ptr<Watch> watch_;
    bool executeEndlessPoll;

    ConnectionStatusEvent connectionStatusEvent_;

    event_type_e connectionStatus_;
    mutable std::mutex connectionMutex_;
    mutable std::condition_variable connectionCondition_;

    std::shared_ptr<StubManager> stubManager_;
    std::mutex stubManagerGuard_;
    std::function<bool(const Message&)> stubMessageHandler_;

    std::shared_ptr<vsomeip::application> application_;

    mutable std::mutex sendAndBlockMutex_;
    mutable std::pair<session_id_t, Message> sendAndBlockAnswer_;
    mutable std::condition_variable sendAndBlockCondition_;
    mutable bool sendAndBlockWait_;

    std::shared_ptr<std::thread> asyncAnswersCleanupThread_;
    std::mutex cleanupMutex_;
    mutable std::condition_variable cleanupCondition_;
    bool cleanupCancelled_;

    mutable std::mutex sendReceiveMutex_;
    typedef std::map<session_id_t,
            std::tuple<
                    std::chrono::time_point<std::chrono::high_resolution_clock>,
                    std::shared_ptr<vsomeip::message>,
                    std::unique_ptr<MessageReplyAsyncHandler> > > async_answers_map_t;
    mutable async_answers_map_t asyncAnswers_;

    mutable std::mutex eventHandlerMutex_;
    typedef std::map<service_id_t,
            std::map<instance_id_t,
                    std::map<event_id_t,
                            std::set<ProxyConnection::EventHandler*>>>> events_map_t;
    mutable events_map_t eventHandlers_;

    mutable std::mutex availabilityMutex_;
    typedef std::map<service_id_t,
            std::map<instance_id_t,
                    std::map<AvailabilityHandlerId_t, AvailabilityHandler_t> > > availability_map_t;
    availability_map_t availabilityHandlers_;

    typedef std::map<service_id_t,
            std::map<instance_id_t, std::set<eventgroup_id_t> > > subscriptions_map_t;
    subscriptions_map_t subscriptions_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CONNECTION_HPP_
