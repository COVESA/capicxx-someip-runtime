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
#include <CommonAPI/SomeIP/Proxy.hpp>
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
            ProxyConnection::EventHandler* eventHandler, major_version_t major,
            bool isField, bool isSelective);

    void removeEventHandler(service_id_t serviceId, instance_id_t instanceId,
            eventgroup_id_t eventGroupId, event_id_t eventId,
            ProxyConnection::EventHandler* eventHandler,  major_version_t major, minor_version_t minor);

    void subscribeForSelective(service_id_t serviceId, instance_id_t instanceId,
            eventgroup_id_t eventGroupId, ProxyConnection::EventHandler* eventHandler,
			uint32_t _tag, major_version_t major);

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

    virtual void releaseService(const Address &_address);

    virtual void registerEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event, const std::set<eventgroup_id_t> &_eventGroups, bool _isField);
    virtual void unregisterEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event);

    virtual void requestEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event, eventgroup_id_t _eventGroup, bool _isField);
    virtual void releaseEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event);

    virtual const std::shared_ptr<StubManager> getStubManager();
    virtual void setStubMessageHandler(MessageHandler_t stubMessageHandler);
    virtual bool isStubMessageHandlerSet();

    virtual void processMsgQueueEntry(Watch::MsgQueueEntry &_msgQueueEntry);
    virtual void processAvblQueueEntry(Watch::AvblQueueEntry &_avblQueueEntry);
    virtual void processFunctionQueueEntry(Watch::FunctionQueueEntry &_functionQueueEntry);

    virtual const ConnectionId_t& getConnectionId();

    virtual void queueSelectiveErrorHandler(service_id_t serviceId,
                                              instance_id_t instanceId);

    virtual void getInitialEvent(service_id_t serviceId,
                                 instance_id_t instanceId,
                                 eventgroup_id_t eventGroupId,
                                 event_id_t eventId,
                                 major_version_t major);

    virtual void incrementConnection();
    virtual void decrementConnection();

    virtual void proxyPushMessage(const Message &_message,
                                  std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler);

    virtual void proxyPushFunction(std::function<void(const uint32_t)> _function, uint32_t _value);

private:
    void proxyReceive(const std::shared_ptr<vsomeip::message> &_message);
    void handleProxyReceive(const std::shared_ptr<vsomeip::message> &_message);
    void stubReceive(const std::shared_ptr<vsomeip::message> &_message);
    void handleStubReceive(const std::shared_ptr<vsomeip::message> &_message);
    void onConnectionEvent(state_type_e _state);
    void onAvailabilityChange(service_id_t _service, instance_id_t _instance,
            bool _is_available);
    void handleAvailabilityChange(const service_id_t _service, const instance_id_t _instance,
                                  bool _is_available);
    void dispatch();
    void cleanup();

    void eventInitialValueCallback(const CallStatus callStatus,
                const Message& message, EventHandler *_eventHandler,
                const uint32_t tag);

    void addSelectiveErrorListener(service_id_t serviceId,
            instance_id_t instanceId,
            eventgroup_id_t eventGroupId);

    std::thread* dispatchThread_;

    std::weak_ptr<MainLoopContext> mainLoopContext_;
    DispatchSource* dispatchSource_;
    Watch* watch_;

    ConnectionStatusEvent connectionStatusEvent_;

    state_type_e connectionStatus_;
    mutable std::mutex connectionMutex_;
    mutable std::condition_variable connectionCondition_;

    std::shared_ptr<StubManager> stubManager_;
    std::mutex stubManagerGuard_;
    std::function<bool(const Message&)> stubMessageHandler_;

    std::shared_ptr<vsomeip::application> application_;

    mutable std::mutex sendAndBlockMutex_;
    mutable std::map<session_id_t, Message> sendAndBlockAnswers_;
    mutable std::condition_variable sendAndBlockCondition_;

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
                std::map<instance_id_t,
                    std::map<event_id_t, std::set<eventgroup_id_t> > > > subscriptions_map_t;
    subscriptions_map_t subscriptions_;

    typedef std::tuple<Message, ProxyConnection::EventHandler*, uint32_t> inital_event_tuple_t;
    std::map<service_id_t,
                std::map<instance_id_t, std::vector<inital_event_tuple_t> > > inital_event_requests;

    uint32_t activeConnections_;
    std::mutex activeConnectionsMutex_;

    std::map<service_id_t,
        std::map<instance_id_t,
            std::map<eventgroup_id_t,
                std::queue<std::pair<ProxyConnection::EventHandler*, std::set<uint32_t>>>>>> selectiveErrorHandlers_;

    std::map<service_id_t,
        std::map<instance_id_t,
            std::map<eventgroup_id_t,
                std::map<ProxyConnection::EventHandler*, std::set<uint32_t>>>>> pendingSelectiveErrorHandlers_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CONNECTION_HPP_
