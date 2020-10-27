// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include <atomic>
#include <condition_variable>

#include <vsomeip/application.hpp>

#include <CommonAPI/MainLoopContext.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/StubManager.hpp>
#include <CommonAPI/SomeIP/DispatchSource.hpp>
#include <CommonAPI/SomeIP/Watch.hpp>
#include <CommonAPI/SomeIP/SubscriptionStatusWrapper.hpp>

namespace CommonAPI {
namespace SomeIP {

class Connection;

enum class commDirectionType : uint8_t {
    PROXYRECEIVE = 0x00,
    STUBRECEIVE = 0x01,
};

struct QueueEntry {

    QueueEntry() { }
    virtual ~QueueEntry() { }

    virtual void process(std::shared_ptr<Connection> _connection) = 0;
};

struct MsgQueueEntry : QueueEntry {

    MsgQueueEntry(std::shared_ptr<vsomeip::message> _message,
                  commDirectionType _directionType) :
                      message_(_message),
                      directionType_(_directionType) { }

    std::shared_ptr<vsomeip::message> message_;
    commDirectionType directionType_;

    void process(std::shared_ptr<Connection> _connection);
};

struct AvblQueueEntry : QueueEntry {

    AvblQueueEntry(service_id_t _service,
                   instance_id_t _instance,
                   bool _isAvailable) :
                       service_(_service),
                       instance_(_instance),
                       isAvailable_(_isAvailable) { }

    service_id_t service_;
    instance_id_t instance_;

    bool isAvailable_;

    void process(std::shared_ptr<Connection> _connection);
};

struct ErrQueueEntry : QueueEntry {

    ErrQueueEntry(std::weak_ptr<ProxyConnection::EventHandler> _eventHandler,
                  uint16_t _errorCode, uint32_t _tag,
                  service_id_t _service, instance_id_t _instance,
                  eventgroup_id_t _eventGroup,
                  event_id_t _event) :
                      eventHandler_(_eventHandler),
                      errorCode_(_errorCode),
                      tag_(_tag),
                      service_(_service),
                      instance_(_instance),
                      eventGroup_(_eventGroup),
                      event_(_event) {}
    virtual ~ErrQueueEntry() {}

    std::weak_ptr<ProxyConnection::EventHandler> eventHandler_;
    uint16_t errorCode_;
    uint32_t tag_;
    service_id_t service_;
    instance_id_t instance_;
    eventgroup_id_t eventGroup_;
    event_id_t event_;

    void process(std::shared_ptr<Connection> _connection);
};

template<class Function, class... Arguments>
struct FunctionQueueEntry : QueueEntry {

    using bindType = decltype(std::bind(std::declval<Function>(),std::declval<Arguments>()...));

    FunctionQueueEntry(Function&& _function,
                       Arguments&& ... _args):
                           bind_(std::forward<Function>(_function), std::forward<Arguments>(_args)...) { }

    bindType bind_;

    void process(std::shared_ptr<Connection> _connection);
};

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

    virtual bool sendMessageWithReplyAsync(
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
            std::weak_ptr<ProxyConnection::EventHandler> eventHandler, major_version_t major,
            event_type_e eventType);

    void removeEventHandler(service_id_t serviceId, instance_id_t instanceId,
            eventgroup_id_t eventGroupId, event_id_t eventId,
            ProxyConnection::EventHandler* _eventHandler,  major_version_t major, minor_version_t minor);

    virtual bool attachMainLoopContext(std::weak_ptr<MainLoopContext>);

    virtual bool isAvailable(const Address &_address);

    virtual AvailabilityHandlerId_t registerAvailabilityHandler(
            const Address &_address, AvailabilityHandler_t _handler,
            std::weak_ptr<Proxy> _proxy, void* _data);
    virtual void unregisterAvailabilityHandler(const Address &_address,
            AvailabilityHandlerId_t _handlerId);

    virtual void registerSubscriptionHandler(const Address &_address,
            const eventgroup_id_t _eventgroup, AsyncSubscriptionHandler_t _handler);
    virtual void unregisterSubscriptionHandler(const Address &_address,
            const eventgroup_id_t _eventgroup);

    virtual void registerService(const Address &_address);
    virtual void unregisterService(const Address &_addess);

    virtual void requestService(const Address &_address);

    virtual void releaseService(const Address &_address);

    virtual void registerEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event, const std::set<eventgroup_id_t> &_eventGroups,
            event_type_e _type, reliability_type_e _reliability);
    virtual void unregisterEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event);

    virtual void requestEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event, eventgroup_id_t _eventGroup, event_type_e _type,
            reliability_type_e _reliability);
    virtual void releaseEvent(service_id_t _service, instance_id_t _instance,
            event_id_t _event);

    virtual const std::shared_ptr<StubManager> getStubManager();
    virtual void setStubMessageHandler(MessageHandler_t stubMessageHandler);
    virtual bool isStubMessageHandlerSet();

    virtual void processMsgQueueEntry(MsgQueueEntry &_msgQueueEntry);
    virtual void processAvblQueueEntry(AvblQueueEntry &_avblQueueEntry);
    virtual void processErrQueueEntry(ErrQueueEntry &_errQueueEntry);

    template<class Function, class... Arguments>
    void processFunctionQueueEntry(FunctionQueueEntry<Function, Arguments ...> &_functionQueueEntry);

    virtual const ConnectionId_t& getConnectionId();

    virtual void queueSelectiveErrorHandler(service_id_t serviceId,
                                              instance_id_t instanceId);

    virtual void incrementConnection();
    virtual void decrementConnection();

    virtual void proxyPushMessageToMainLoop(const Message &_message,
                                  std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler);

    template<class Function, class... Arguments>
    void proxyPushFunctionToMainLoop(Function&& _function, Arguments&& ... _args);

    virtual void getAvailableInstances(service_id_t _serviceId, std::vector<std::string> *_instances);

    void subscribe(service_id_t serviceId, instance_id_t instanceId,
                eventgroup_id_t eventGroupId, event_id_t eventId,
                std::weak_ptr<ProxyConnection::EventHandler> _eventHandler,
                uint32_t _tag, major_version_t major);

private:
    void receive(const std::shared_ptr<vsomeip::message> &_message);
    void handleProxyReceive(const std::shared_ptr<vsomeip::message> &_message);
    void handleStubReceive(const std::shared_ptr<vsomeip::message> &_message);
    void onConnectionEvent(state_type_e _state);
    void onAvailabilityChange(service_id_t _service, instance_id_t _instance,
            bool _is_available);
    void handleAvailabilityChange(const service_id_t _service, const instance_id_t _instance,
                                  bool _is_available);
    void dispatch();
    void cleanup();

    void addSubscriptionStatusListener(service_id_t serviceId,
            instance_id_t instanceId,
            eventgroup_id_t eventGroupId,
            event_id_t _eventId, event_type_e eventType);

    void queueSubscriptionStatusHandler(service_id_t serviceId,
            instance_id_t instanceId);

    void doDisconnect();

    void insertSubscriptionStatusListener(service_id_t serviceId, instance_id_t instanceId,
            eventgroup_id_t eventGroupId, event_id_t eventId,
            std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
            uint32_t _tag);

    void cancelAsyncAnswers(const service_id_t _service, const instance_id_t _instance);

    std::shared_ptr<std::thread> dispatchThread_;

    std::weak_ptr<MainLoopContext> mainLoopContext_;
    DispatchSource* dispatchSource_;
    Watch* watch_;

    ConnectionStatusEvent connectionStatusEvent_;

    std::atomic<state_type_e> connectionStatus_;
    mutable std::mutex connectionMutex_;
    mutable std::condition_variable connectionCondition_;

    std::shared_ptr<StubManager> stubManager_;
    std::mutex stubManagerGuard_;
    std::function<bool(const Message&)> stubMessageHandler_;

    std::shared_ptr<vsomeip::application> application_;

    mutable std::map<session_id_fake_t, Message> sendAndBlockAnswers_;
    mutable std::condition_variable_any sendAndBlockCondition_;

    std::shared_ptr<std::thread> asyncAnswersCleanupThread_;
    mutable std::mutex cleanupMutex_;
    mutable std::condition_variable cleanupCondition_;
    std::atomic<bool> cleanupCancelled_;

    mutable std::recursive_mutex sendReceiveMutex_;
    typedef std::map<session_id_fake_t,
            std::tuple<
                    std::chrono::steady_clock::time_point,
                    std::shared_ptr<vsomeip::message>,
                    std::unique_ptr<MessageReplyAsyncHandler>,
                    Timeout_t > > async_answers_map_t;
    mutable async_answers_map_t asyncAnswers_;
    mutable async_answers_map_t asyncTimeouts_;

    mutable std::mutex eventHandlerMutex_;
    typedef std::map<service_id_t,
            std::map<instance_id_t,
                    std::map<event_id_t,
                            std::map<ProxyConnection::EventHandler*,
                                std::weak_ptr<ProxyConnection::EventHandler>>>>> events_map_t;
    mutable events_map_t eventHandlers_;

    mutable std::mutex availabilityMutex_;
    typedef std::map<service_id_t,
            std::map<instance_id_t,
                    std::map<AvailabilityHandlerId_t, std::tuple<AvailabilityHandler_t,
                                                          std::weak_ptr<Proxy>,
                                                          void*>> > > availability_map_t;
    availability_map_t availabilityHandlers_;

    typedef std::map<service_id_t,
                std::map<instance_id_t,
                    std::map<event_id_t, std::set<eventgroup_id_t> > > > subscriptions_map_t;
    subscriptions_map_t subscriptions_;

    uint32_t activeConnections_;
    std::mutex activeConnectionsMutex_;

    std::map<std::tuple<service_id_t, instance_id_t, eventgroup_id_t, event_id_t>,
        std::shared_ptr<SubscriptionStatusWrapper>> subscriptionStates_;

    std::map<service_id_t, std::map<instance_id_t, bool>> availabilityCalled_;

    std::mutex requestedServicesMutex_;
    std::map<service_id_t, std::map<instance_id_t, std::uint32_t>> requestedServices_;

    std::mutex requestedEventsMutex_;
    std::map<service_id_t,
        std::map<instance_id_t,
            std::map<eventgroup_id_t,
                std::map<event_id_t, std::uint32_t>>>> requestedEvents_;

    std::map<service_id_t, std::map<instance_id_t, std::map<eventgroup_id_t, AsyncSubscriptionHandler_t >>> subscription_;
    std::mutex subscriptionMutex_;

    std::map<std::shared_ptr<vsomeip::message>, session_id_fake_t> errorResponses_;

    // registered events from stubs
    std::mutex registeredEventsMutex_;
    std::map<vsomeip::service_t,
            std::map<vsomeip::instance_t, std::set<vsomeip::event_t>>> registeredEvents_;

    mutable session_id_t lastSessionId_;

    Timeout_t timeoutWarnThreshold;
};


template<class Function, class... Arguments>
void FunctionQueueEntry<Function, Arguments ...>::process(std::shared_ptr<Connection> _connection) {
    _connection->processFunctionQueueEntry(*this);
}

template<class Function, class... Arguments>
void Connection::processFunctionQueueEntry(FunctionQueueEntry<Function, Arguments ...> &_functionQueueEntry) {
    _functionQueueEntry.bind_();
}

template<class Function, class... Arguments>
void Connection::proxyPushFunctionToMainLoop(Function&& _function, Arguments&& ... _args) {
    if (auto lockedContext = mainLoopContext_.lock()) {
        std::shared_ptr<FunctionQueueEntry<Function, Arguments ...>> functionQueueEntry = std::make_shared<FunctionQueueEntry<Function, Arguments ...>>(
                std::forward<Function>(_function), std::forward<Arguments>(_args) ...);
        watch_->pushQueue(functionQueueEntry);
    }
    else {
        std::thread t(std::forward<Function>(_function), std::forward<Arguments>(_args) ...);
        t.detach();
    }
}

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CONNECTION_HPP_
