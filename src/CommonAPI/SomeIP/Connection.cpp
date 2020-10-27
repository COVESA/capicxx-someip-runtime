// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <mutex>
#include <map>
#include <tuple>

#include <vsomeip/vsomeip.hpp>

#include <CommonAPI/Runtime.hpp>
#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/Factory.hpp>
#include <CommonAPI/SomeIP/Constants.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/Defines.hpp>
#include <CommonAPI/SomeIP/ProxyAsyncEventCallbackHandler.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>

namespace CommonAPI {
namespace SomeIP {

void MsgQueueEntry::process(std::shared_ptr<Connection> _connection) {
    _connection->processMsgQueueEntry(*this);
}

void AvblQueueEntry::process(std::shared_ptr<Connection> _connection) {
    _connection->processAvblQueueEntry(*this);
}

void ErrQueueEntry::process(std::shared_ptr<Connection> _connection) {
    _connection->processErrQueueEntry(*this);
}

void Connection::receive(const std::shared_ptr<vsomeip::message> &_message) {
    commDirectionType itsDirection =
            (_message->get_message_type() < vsomeip::message_type_e::MT_NOTIFICATION ?
            commDirectionType::STUBRECEIVE : commDirectionType::PROXYRECEIVE);

    // avoid blocking the mainloop when a synchronous call in a callback was done
    bool isSendAndBlockAnswer = false;
    if (itsDirection == commDirectionType::PROXYRECEIVE) {
        std::lock_guard<std::recursive_mutex> itsLock(sendReceiveMutex_);
        if(_message->get_message_type() != vsomeip::message_type_e::MT_NOTIFICATION &&
                sendAndBlockAnswers_.find(_message->get_session()) != sendAndBlockAnswers_.end()) {
            isSendAndBlockAnswer = true;
        }
    }

    if (auto lockedContext = mainLoopContext_.lock() && !isSendAndBlockAnswer) {
        (void)lockedContext;
        std::shared_ptr<MsgQueueEntry> msg_queue_entry
            = std::make_shared<MsgQueueEntry>(_message, itsDirection);
        watch_->pushQueue(msg_queue_entry);
    } else {
        if (itsDirection == commDirectionType::PROXYRECEIVE) {
            handleProxyReceive(_message);
        } else {
            handleStubReceive(_message);
        }
    }
}

void Connection::handleProxyReceive(const std::shared_ptr<vsomeip::message> &_message) {
    sendReceiveMutex_.lock();

    session_id_fake_t sessionId = _message->get_session();
    if (!sessionId) {
        auto found_Message = errorResponses_.find(_message);
        if (found_Message != errorResponses_.end()) {
            sessionId = found_Message->second;
            errorResponses_.erase(found_Message);
        }
    }

    // handle events
    if(_message->get_message_type() == message_type_e::MT_NOTIFICATION) {
        service_id_t serviceId = _message->get_service();
        instance_id_t instanceId = _message->get_instance();
        event_id_t eventId = _message->get_method();

        std::map<ProxyConnection::EventHandler*, std::weak_ptr<ProxyConnection::EventHandler>> handlers;
        {
            std::lock_guard<std::mutex> eventLock(eventHandlerMutex_);
            auto foundService = eventHandlers_.find(serviceId);
            if (foundService != eventHandlers_.end()) {
                auto foundInstance = foundService->second.find(instanceId);
                if (foundInstance != foundService->second.end()) {
                    auto foundEvent = foundInstance->second.find(eventId);
                    if (foundEvent != foundInstance->second.end()) {
                        for (auto handler : foundEvent->second)
                            handlers.insert(handler);
                    }
                }
            }
        }
        sendReceiveMutex_.unlock();

        // We must not hold the lock when calling the handlers!
        for (auto handler : handlers) {
            if(auto itsHandler = handler.second.lock())
                itsHandler->onEventMessage(Message(_message));
        }

        return;
    }

    // handle sync method calls
    auto foundSession = sendAndBlockAnswers_.find(sessionId);
    if (foundSession != sendAndBlockAnswers_.end()) {
        foundSession->second = Message(_message);
        sendAndBlockCondition_.notify_all();
        sendReceiveMutex_.unlock();
        return;
    }

    // handle async method calls
    async_answers_map_t::iterator foundAsyncHandler = asyncAnswers_.find(sessionId);
    if(foundAsyncHandler != asyncAnswers_.end()) {
        std::unique_ptr<MessageReplyAsyncHandler> itsHandler
            = std::move(std::get<2>(foundAsyncHandler->second));
        asyncAnswers_.erase(sessionId);
        sendReceiveMutex_.unlock();

        CallStatus callStatus;
        if(_message->get_return_code() == vsomeip::return_code_e::E_OK) {
            callStatus = CallStatus::SUCCESS;
        } else if(_message->get_return_code() == vsomeip::return_code_e::E_NOT_REACHABLE) {
            callStatus = CallStatus::NOT_AVAILABLE;
        } else {
            callStatus = CallStatus::REMOTE_ERROR;
        }

        try {
            itsHandler->onMessageReply(callStatus, Message(_message));
        } catch (const std::exception& e) {
            COMMONAPI_ERROR("Message reply failed(", e.what(), ")");
        }
        return;
    }

    // handle async call timeouts in Mainloop mode
    async_answers_map_t::iterator foundTimeoutHandler = asyncTimeouts_.find(sessionId);
    if(foundTimeoutHandler != asyncTimeouts_.end()) {
        std::unique_ptr<MessageReplyAsyncHandler> itsHandler
            = std::move(std::get<2>(foundTimeoutHandler->second));
        asyncTimeouts_.erase(sessionId);
        sendReceiveMutex_.unlock();

        try {
            itsHandler->onMessageReply(CallStatus::REMOTE_ERROR, Message(_message));
        } catch (const std::exception& e) {
            COMMONAPI_ERROR("Message reply failed on timeout(", e.what(), ")");
        }
    } else {
        sendReceiveMutex_.unlock();
    }
}

void Connection::handleStubReceive(const std::shared_ptr<vsomeip::message> &_message) {
    if (stubMessageHandler_)
        stubMessageHandler_(Message(_message));
}

void Connection::onConnectionEvent(state_type_e state) {
    std::lock_guard<std::mutex> itsLock(connectionMutex_);
    connectionStatus_ = state;
    connectionCondition_.notify_one();
}

void Connection::onAvailabilityChange(service_id_t _service, instance_id_t _instance,
           bool _is_available) {
    {
        std::lock_guard<std::mutex> itsLock(availabilityMutex_);
        auto its_service = availabilityCalled_.find(_service);
        if (its_service != availabilityCalled_.end()) {
            auto its_instance = its_service->second.find(_instance);
            if (its_instance != its_service->second.end() &&
                    its_instance->second && !_is_available) {
                queueSubscriptionStatusHandler(_service, _instance);
            }
        }
    }
    if (auto lockedContext = mainLoopContext_.lock()) {
        std::shared_ptr<AvblQueueEntry> avbl_queue_entry = std::make_shared<AvblQueueEntry>(
                _service, _instance, _is_available);
        watch_->pushQueue(avbl_queue_entry);
    }
    else {
        handleAvailabilityChange(_service, _instance, _is_available);
    }
}

void Connection::cancelAsyncAnswers(const service_id_t _service, const instance_id_t _instance) {
    async_answers_map_t asyncAnswersToProcess;
    {
        std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
        auto it = asyncAnswers_.begin();
        while (it != asyncAnswers_.end()) {
            if(_service == std::get<1>(it->second)->get_service()
                    && _instance == std::get<1>(it->second)->get_instance()) {
                asyncAnswersToProcess[it->first] = std::move(it->second);
                it = asyncAnswers_.erase(it);
            } else {
                it++;
            }
        }
    }
    for (auto& its_answer : asyncAnswersToProcess) {
        std::shared_ptr<vsomeip::message> itsRequest(
                std::get<1>(its_answer.second));
        std::shared_ptr<vsomeip::message> itsResponse
            = vsomeip::runtime::get()->create_response(std::get<1>(its_answer.second));
        itsResponse->set_message_type(
                vsomeip::message_type_e::MT_ERROR);
        itsResponse->set_return_code(
                vsomeip::return_code_e::E_TIMEOUT);
        if (auto lockedContext = mainLoopContext_.lock()) {
            {
                std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
                asyncTimeouts_[its_answer.first] = std::move(its_answer.second);
            }
            std::shared_ptr<MsgQueueEntry> msg_queue_entry
                = std::make_shared<MsgQueueEntry>(itsResponse,
                        commDirectionType::PROXYRECEIVE);
            watch_->pushQueue(msg_queue_entry);
        } else {
            try {
                std::get<2>(its_answer.second)->onMessageReply(
                        CallStatus::REMOTE_ERROR, Message(itsResponse));
            } catch (const std::exception& e) {
                COMMONAPI_ERROR("Message reply failed when service became unavailable(", e.what(), ")");
            }
        }
    }
}

void Connection::handleAvailabilityChange(const service_id_t _service,
        instance_id_t _instance, bool _is_available) {
    if (!_is_available) {
        // cancel sync calls
        {
            std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
            sendAndBlockCondition_.notify_all();
        }
        // cancel asynchronous calls
        cancelAsyncAnswers(_service, _instance);
    }

    std::list<std::tuple<AvailabilityHandler_t,
                         std::weak_ptr<Proxy>,
                         void*>> itsHandlers;
    {
        std::lock_guard<std::mutex> itsLock(availabilityMutex_);
        auto foundService = availabilityHandlers_.find(_service);
        if (foundService != availabilityHandlers_.end()) {
            auto foundInstance = foundService->second.find(_instance);
            if (foundInstance != foundService->second.end()) {
                for (auto &h : foundInstance->second)
                    itsHandlers.push_back(h.second);
            }
            auto foundWildcardInstance = foundService->second.find(
                    vsomeip::ANY_INSTANCE);
            if (foundWildcardInstance != foundService->second.end()) {
                for (auto &h : foundWildcardInstance->second)
                    itsHandlers.push_back(h.second);
            }
        }
        availabilityCalled_[_service][_instance] = true;
    }

    for (auto h : itsHandlers) {
        if(auto itsProxy = std::get<1>(h).lock())
            std::get<0>(h)(itsProxy, _service, _instance, _is_available,
                    std::get<2>(h));
    }
}

void Connection::dispatch() {
    application_->start();
}

void Connection::cleanup() {
    std::unique_lock<std::mutex> itsLock(cleanupMutex_);

    int timeout = std::numeric_limits<int>::max(); // not really, but nearly "forever"
    while (!cleanupCancelled_) {
        if (std::cv_status::timeout ==
            cleanupCondition_.wait_for(itsLock, std::chrono::milliseconds(timeout))) {
            const std::chrono::steady_clock::time_point now =
                    (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            async_answers_map_t asyncAnswersToProcess;
            {
                std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
                auto it = asyncAnswers_.begin();
                while (it != asyncAnswers_.end()) {
                    if(now > std::get<0>(it->second)){
                        const int warnThreshold(std::get<3>(it->second));
                        if (0 == warnThreshold) {
                            asyncAnswersToProcess[it->first] = std::move(it->second);
                            it = asyncAnswers_.erase(it);
                        } else {
                            auto & message = std::get<1>(it->second);
                            std::stringstream log;
                            log << "(" << std::setw(4) << std::setfill('0') << std::hex << message->get_client()
                                << ":" << std::setw(4) << std::setfill('0') << std::hex << message->get_session()
                                << ") response delayed " << std::setw(4) << std::setfill('0') << std::hex << message->get_service()
                                << "." << std::setw(4) << std::setfill('0') << std::hex << message->get_instance()
                                << "." << std::setw(4) << std::setfill('0') << std::hex << message->get_method()
                                << " - " << std::dec << (warnThreshold*3) << " of " << (warnThreshold<<2) << " ms";
                            COMMONAPI_WARNING(log.str());
                            std::get<0>(it->second) += std::chrono::milliseconds(warnThreshold);
                            std::get<3>(it->second) = 0;
                        }
                    } else {
                        it++;
                    }
                }
            }

            for (auto& its_answer : asyncAnswersToProcess) {
                std::shared_ptr<vsomeip::message> response
                    = vsomeip::runtime::get()->create_response(std::get<1>(its_answer.second));
                response->set_message_type(vsomeip::message_type_e::MT_ERROR);
                response->set_return_code(vsomeip::return_code_e::E_TIMEOUT);
                if (auto lockedContext = mainLoopContext_.lock()) {
                    {
                        std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
                        asyncTimeouts_[its_answer.first] = std::move(its_answer.second);
                    }
                    std::shared_ptr<MsgQueueEntry> msg_queue_entry =
                            std::make_shared<MsgQueueEntry>(
                                    response, commDirectionType::PROXYRECEIVE);
                    itsLock.unlock();
                    watch_->pushQueue(msg_queue_entry);
                    itsLock.lock();
                } else {
                    try {
                        itsLock.unlock();
                        std::get<2>(its_answer.second)->onMessageReply(
                                CallStatus::REMOTE_ERROR, Message(response));
                        itsLock.lock();
                    } catch (const std::exception& e) {
                        COMMONAPI_ERROR("Message reply failed on cleanup(", e.what(), ")");
                    }
                }
            }
        }

        {
            // Update the upcoming timeout
            timeout = std::numeric_limits<int>::max();
            std::chrono::steady_clock::time_point now =
                    (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
            for (auto it = asyncAnswers_.begin(); it != asyncAnswers_.end(); it++) {
                int remaining =
                        (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::get<0>(it->second) - now).count();
                if (timeout > remaining)
                    timeout = remaining;
            }
        }
    }
}

Connection::Connection(const std::string &_name)
      : dispatchSource_(NULL),
        watch_(NULL),
        connectionStatus_(state_type_e::ST_DEREGISTERED),
        application_(vsomeip::runtime::get()->create_application(_name)),
        asyncAnswersCleanupThread_(NULL),
        cleanupCancelled_(false),
        activeConnections_(0),
        lastSessionId_(0),
        timeoutWarnThreshold(std::numeric_limits<Timeout_t>::max()) {
    std::string appId = Runtime::getProperty("LogApplication");
    std::string contextId = Runtime::getProperty("LogContext");

#if defined(COMMONAPI_LOGLEVEL) && (COMMONAPI_LOGLEVEL_WARNING <= COMMONAPI_LOGLEVEL)
    const char * pEnv = std::getenv("COMMONAPI_SEND_TIMEOUT_WARN_THRESHOLD");
    if (pEnv) {
        const int tmp = std::stoi(pEnv);
        if (0 < tmp) {
            timeoutWarnThreshold = 1000 * tmp;
        }
    }
#endif

    if (appId != "")
        vsomeip::runtime::set_property("LogApplication", appId);

    if (contextId != "")
        vsomeip::runtime::set_property("LogContext", contextId);
}

Connection::~Connection() {
    if (auto lockedContext = mainLoopContext_.lock()) {
        lockedContext->deregisterDispatchSource(dispatchSource_);
        lockedContext->deregisterWatch(watch_);
    }
    bool shouldDisconnect(false);
    {
        std::lock_guard<std::mutex> itsLock(connectionMutex_);
        shouldDisconnect = connectionStatus_ == state_type_e::ST_REGISTERED;
    }
    if (shouldDisconnect) {
        doDisconnect();
    }
}

bool Connection::attachMainLoopContext(std::weak_ptr<MainLoopContext> mainLoopContext) {
    if (mainLoopContext_.lock() == mainLoopContext.lock())
        return true;

    bool result = false;

    mainLoopContext_ = mainLoopContext;

    if (auto lockedContext = mainLoopContext_.lock()) {
        watch_ = new Watch(shared_from_this());
        dispatchSource_ = new DispatchSource(watch_);

        lockedContext->registerDispatchSource(dispatchSource_);
        lockedContext->registerWatch(watch_);

        lockedContext->wakeup();

        result = true;
    }

    return result;
}

bool Connection::connect(bool) {
    if (!application_->init())
        return false;

    std::function<void(state_type_e)> connectionHandler = std::bind(&Connection::onConnectionEvent,
                                                                    shared_from_this(),
                                                                    std::placeholders::_1);
    application_->register_state_handler(connectionHandler);

    asyncAnswersCleanupThread_ = std::make_shared<std::thread>(&Connection::cleanup, this);
    dispatchThread_ = std::make_shared<std::thread>(&Connection::dispatch, this);
    return true;
}

void Connection::doDisconnect() {
    if (asyncAnswersCleanupThread_) {
        {
            std::lock_guard<std::mutex> lg(cleanupMutex_);
            cleanupCancelled_ = true;
            cleanupCondition_.notify_one();
        }
        if (asyncAnswersCleanupThread_->joinable())
            asyncAnswersCleanupThread_->join();
    }

    application_->stop();
    if(dispatchThread_) {
        if (dispatchThread_->joinable())
            dispatchThread_->join();
    }
    application_->clear_all_handler();

    {
        std::lock_guard<std::mutex> itsLock(connectionMutex_);
        connectionStatus_ = state_type_e::ST_DEREGISTERED;
    }
}
void Connection::disconnect() {
    doDisconnect();
    Factory::get()->releaseConnection(application_->get_name());
}

bool Connection::isConnected() const {
    return (connectionStatus_ == state_type_e::ST_REGISTERED);
}

void Connection::waitUntilConnected() {
    std::unique_lock<std::mutex> itsLock(connectionMutex_);
    while (!isConnected())
        connectionCondition_.wait(itsLock);
}

ProxyConnection::ConnectionStatusEvent& Connection::getConnectionStatusEvent() {
    return connectionStatusEvent_;
}

bool Connection::sendMessage(const Message& message, uint32_t*) const {
    if (!isConnected())
        return false;

    if (message.isRequestType())
        message.message_->set_message_type(message_type_e::MT_REQUEST_NO_RETURN);

    application_->send(message.message_);
    return true;
}

bool Connection::sendEvent(const Message &message, uint32_t *) const {
    application_->notify(message.getServiceId(), message.getInstanceId(),
            message.getMethodId(), message.message_->get_payload(), true);

    return true;
}

bool Connection::sendEvent(const Message &message, client_id_t _client,
        uint32_t *) const {
    application_->notify_one(message.getServiceId(), message.getInstanceId(), message.getMethodId(),
            message.message_->get_payload(), _client, true);

    return true;
}

bool Connection::sendMessageWithReplyAsync(
        const Message& message,
        std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler,
        const CommonAPI::CallInfo *_info) const {


    if (!isConnected())
        return false;


    {
        std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
        application_->send(message.message_);
        const vsomeip::session_t itsSession = message.getSessionId();
        if (lastSessionId_ == itsSession) {
            return false;
        }
        lastSessionId_ = itsSession;

        if (_info->sender_ != 0) {
            COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_,
                    " - ClientID: ", message.getClientId(),
                    ", SessionID: ", message.getSessionId());
        }

#if defined(COMMONAPI_LOGLEVEL) && (COMMONAPI_LOGLEVEL_WARNING <= COMMONAPI_LOGLEVEL)
        const Timeout_t warnThreshold(timeoutWarnThreshold<=_info->timeout_?_info->timeout_>>2:0);
#else
        static const Timeout_t warnThreshold(0);
#endif
        auto timeoutTime =
                (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(_info->timeout_-warnThreshold);

        asyncAnswers_[itsSession] = std::make_tuple(
                timeoutTime, message.message_,
                std::move(messageReplyAsyncHandler), warnThreshold);
    }
    {
        std::lock_guard<std::mutex> itsLock(cleanupMutex_);
        cleanupCondition_.notify_one();
    }


    return true;
}

Message Connection::sendMessageWithReplyAndBlock(
        const Message& message,
        const CommonAPI::CallInfo *_info) const {

    if (!isConnected())
        return Message();

    std::unique_lock<std::recursive_mutex> lock(sendReceiveMutex_);

    std::pair<std::map<session_id_fake_t, Message>::iterator, bool> itsAnswer;
    application_->send(message.message_);
    if (_info->sender_ != 0) {
        COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_,
                    " - ClientID: ", message.getClientId(),
                    ", SessionID: ", message.getSessionId());
    }

    itsAnswer = sendAndBlockAnswers_.emplace(message.getSessionId(), Message());
    Message itsResult;
    if (!itsAnswer.second) {
        return itsResult;
    }

    std::cv_status waitStatus = std::cv_status::no_timeout;


    std::chrono::steady_clock::time_point elapsed(
            std::chrono::steady_clock::now()
            + std::chrono::milliseconds(_info->timeout_));

    // Wait until the answer was received.
    // As the sendReceiveMutex_ is locked in Connection::handleProxyReceive the
    // answer can only be received if the mutex is released here. Thus it's
    // necessary to call wait on the condition in every case.
    do {
        if (!application_->is_available(message.getServiceId(), message.getInstanceId())) {
            waitStatus = std::cv_status::timeout;
            break;
        }
        waitStatus = sendAndBlockCondition_.wait_until(lock, elapsed);
        if (itsAnswer.first->second || (waitStatus == std::cv_status::timeout && (elapsed < std::chrono::steady_clock::now())))
            break;
    } while (!itsAnswer.first->second);

    // If there was an answer (thus, we did not run into the timeout),
    // move it to itsResult
    if (itsAnswer.first->second)  {
        itsResult = std::move(itsAnswer.first->second);
    }

    sendAndBlockAnswers_.erase(itsAnswer.first);

    return itsResult;
}

void Connection::addEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
        major_version_t major,
        event_type_e eventType) {

    (void)major;
    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    if(auto itsHandler = eventHandler.lock()) {
        eventHandlers_[serviceId][instanceId][eventId][itsHandler.get()] = eventHandler;
        const bool inserted(std::get<1>(subscriptions_[serviceId][instanceId][eventId].insert(eventGroupId)));
        if(inserted) {
            addSubscriptionStatusListener(serviceId, instanceId, eventGroupId,
                    eventId, eventType);
        }
    }
}

void Connection::removeEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler,
        major_version_t major,
        minor_version_t minor) {
    (void)major;
    (void)minor;
    bool lastSubscriber(false);
    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    auto foundService = eventHandlers_.find(serviceId);
    if (foundService != eventHandlers_.end()) {
        auto foundInstance = foundService->second.find(instanceId);
        if (foundInstance != foundService->second.end()) {
            auto foundEventId = foundInstance->second.find(eventId);
            if (foundEventId != foundInstance->second.end()) {
                foundEventId->second.erase(eventHandler);
                if (foundEventId->second.empty()) {
                    foundInstance->second.erase(foundEventId);
                    lastSubscriber = true;
                    application_->unsubscribe(serviceId, instanceId,
                            eventGroupId, eventId);
                }
            }
        }
    }

    auto its_tuple = std::make_tuple(serviceId, instanceId, eventGroupId, eventId);
    auto its_wrapper = subscriptionStates_.find(its_tuple);
    if (its_wrapper != subscriptionStates_.end()) {
        its_wrapper->second->removeHandler(eventHandler);
    }

    if (lastSubscriber) {
        subscriptionStates_.erase(its_tuple);

        auto foundPendingService = subscriptions_.find(serviceId);
        if (foundPendingService != subscriptions_.end()) {
            auto foundPendingInstance = foundPendingService->second.find(instanceId);
            if (foundPendingInstance != foundPendingService->second.end()) {
                foundPendingInstance->second.erase(eventId);
                if (foundPendingInstance->second.empty()) {
                    foundPendingService->second.erase(foundPendingInstance);
                    if (foundPendingService->second.empty())
                        subscriptions_.erase(foundPendingService);
                }
            }
        }

        application_->unregister_subscription_status_handler(serviceId,
                instanceId, eventGroupId, eventId);
    }
}

void Connection::subscribe(service_id_t serviceId, instance_id_t instanceId,
                eventgroup_id_t eventGroupId, event_id_t eventId,
                std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
                uint32_t _tag, major_version_t major) {

    insertSubscriptionStatusListener(serviceId, instanceId, eventGroupId, eventId,
            eventHandler, _tag);

    application_->subscribe(serviceId, instanceId, eventGroupId, major, eventId);
}

void Connection::addSubscriptionStatusListener(service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId, event_type_e eventType) {

    auto statusHandler = [this] (
                const vsomeip::service_t _service, const vsomeip::instance_t _instance,
                const vsomeip::eventgroup_t _eventgroup, const vsomeip::event_t _event,
                const uint16_t errorCode) {

        // SubscriptionStatusListenerCalled!
        auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, _event);
        SubscriptionStatusWrapper subscriptionStatus(_service, _instance, _eventgroup, _event);

        std::unique_lock<std::mutex> lock(eventHandlerMutex_);
        auto its_wrapper = subscriptionStates_.find(its_tuple);
        if (its_wrapper != subscriptionStates_.end()) {
            // Copy all entries to process with lock is held
            std::vector<std::pair<std::weak_ptr<ProxyConnection::EventHandler>, uint32_t >> handlerCopy;
            while (!its_wrapper->second->pendingHandlerQueueEmpty()) {
                handlerCopy.push_back(its_wrapper->second->popAndFrontPendingHandler());
            }
            lock.unlock(); // Unlock as we locally copied all entries to process before

            // Process all copied entries without the lock is held
            for (auto its_handlerCopy : handlerCopy) {
                std::weak_ptr<ProxyConnection::EventHandler> handler = its_handlerCopy.first;
                if (auto lockedContext = mainLoopContext_.lock()) {
                    std::shared_ptr<ErrQueueEntry> err_queue_entry =
                            std::make_shared<ErrQueueEntry>(
                                    handler, errorCode, its_handlerCopy.second,
                                    _service, _instance, _eventgroup, _event);
                    watch_->pushQueue(err_queue_entry);
                } else {
                    if(auto itsHandler = handler.lock()) {
                        itsHandler->onError(errorCode, its_handlerCopy.second);
                    }
                }
            }
        }
    };
    application_->register_subscription_status_handler(serviceId, instanceId,
                eventGroupId, eventId, statusHandler,
                eventType != event_type_e::ET_SELECTIVE_EVENT ? false : true);

}

bool
Connection::isAvailable(const Address &_address) {
    {
        std::lock_guard<std::mutex> itsLock(connectionMutex_);
        if (connectionStatus_ != state_type_e::ST_REGISTERED) {
            return false;
        }
    }
    {
        bool availabilityCalled(false);
        std::lock_guard<std::mutex> itsLock(availabilityMutex_);
        auto its_service = availabilityCalled_.find(_address.getService());
        if (its_service != availabilityCalled_.end()) {
            auto its_instance = its_service->second.find(_address.getInstance());
            if (its_instance != its_service->second.end()) {
                availabilityCalled = its_instance->second;
            }
        }
        if (!availabilityCalled) {
            return false;
        }
    }
    return application_->is_available(_address.getService(), _address.getInstance(),
            _address.getMajorVersion(), ANY_MINOR_VERSION);
}

AvailabilityHandlerId_t
Connection::registerAvailabilityHandler(
        const Address &_address,
        AvailabilityHandler_t _handler,
        std::weak_ptr<Proxy> _proxy,
        void* _data) {
    static AvailabilityHandlerId_t itsHandlerId = 0;
    AvailabilityHandlerId_t itsCurrentHandlerId;
    bool isRegistered(false);

    service_id_t itsService = _address.getService();
    instance_id_t itsInstance = _address.getInstance();
    major_version_t itsMajor = _address.getMajorVersion();
    minor_version_t itsMinor = ANY_MINOR_VERSION;

    {
        std::unique_lock<std::mutex> itsLock(availabilityMutex_);
        itsHandlerId++;
        itsCurrentHandlerId = itsHandlerId;

        auto foundService = availabilityHandlers_.find(itsService);
        if (foundService != availabilityHandlers_.end()) {
            auto foundInstance = foundService->second.find(itsInstance);
            if (foundInstance != foundService->second.end()) {
                foundInstance->second[itsCurrentHandlerId] = std::make_tuple(_handler, _proxy, _data);
                isRegistered = true;
            } else {
                foundService->second[itsInstance][itsCurrentHandlerId] = std::make_tuple(_handler, _proxy, _data);
            }
        } else {
            availabilityHandlers_[itsService][itsInstance][itsCurrentHandlerId] = std::make_tuple(_handler, _proxy, _data);
        }
    }

    if (!isRegistered) {
        vsomeip::availability_handler_t itsHandler
            = std::bind(&Connection::onAvailabilityChange, shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3);
        application_->register_availability_handler(
                itsService, itsInstance, itsHandler, itsMajor, itsMinor);
    }

    return itsCurrentHandlerId;
}

void
Connection::unregisterAvailabilityHandler(
        const Address &_address, AvailabilityHandlerId_t _handlerId) {
    bool mustUnregister(false);

    service_id_t itsService = _address.getService();
    instance_id_t itsInstance = _address.getInstance();
    major_version_t itsMajor = _address.getMajorVersion();
    minor_version_t itsMinor = ANY_MINOR_VERSION;

    {
        std::unique_lock<std::mutex> itsLock(availabilityMutex_);
        auto foundService = availabilityHandlers_.find(itsService);
        if (foundService != availabilityHandlers_.end()) {
            auto foundInstance = foundService->second.find(itsInstance);
            if (foundInstance != foundService->second.end()) {
                foundInstance->second.erase(_handlerId);
                if (foundInstance->second.size() == 0) {
                    mustUnregister = true;
                    foundService->second.erase(foundInstance);
                    if (foundService->second.size() == 0) {
                        availabilityHandlers_.erase(foundService);
                    }
                }
            }
        }
    }


    if (mustUnregister) {
        application_->unregister_availability_handler(
                itsService, itsInstance, itsMajor, itsMinor);
    }
}

void
Connection::registerService(const Address &_address) {
    if(!stubMessageHandler_) {
        return;
    }

    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();
    major_version_t majorVersion = _address.getMajorVersion();
    minor_version_t minorVersion = _address.getMinorVersion();

    vsomeip::message_handler_t handler
        = std::bind(&Connection::receive, shared_from_this(), std::placeholders::_1);
    application_->register_message_handler(service, instance, ANY_METHOD, handler);
    application_->offer_service(service, instance, majorVersion, minorVersion);
}

void
Connection::unregisterService(const Address &_address) {
    if (!stubMessageHandler_)
        return;

    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();
    major_version_t major = _address.getMajorVersion();
    minor_version_t minor = _address.getMinorVersion();

    std::set<vsomeip::event_t> itsEvents;
    {
        std::lock_guard<std::mutex> itsLock(registeredEventsMutex_);
        const auto foundService = registeredEvents_.find(service);
        if (foundService != registeredEvents_.end()) {
            const auto foundInstance = foundService->second.find(instance);
            if (foundInstance != foundService->second.end()) {
                itsEvents = foundInstance->second;
                foundService->second.erase(foundInstance);
                if (!foundService->second.size()) {
                    registeredEvents_.erase(foundService);
                }
            }
        }
    }

    application_->stop_offer_service(service, instance, major, minor);
    application_->unregister_message_handler(service, instance, ANY_METHOD);
    for (const auto e : itsEvents) {
        application_->stop_offer_event(service, instance, e);
    }
}

void
Connection::requestService(const Address &_address) {
    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();
    major_version_t majorVersion = _address.getMajorVersion();
    minor_version_t minorVersion = ANY_MINOR_VERSION;

    bool found(false);
    {
        std::lock_guard<std::mutex> lock(requestedServicesMutex_);
        auto foundService = requestedServices_.find(service);
        if (foundService != requestedServices_.end()) {
            auto foundInstance = foundService->second.find((instance));
            if (foundInstance != foundService->second.end()) {
                found = true;
                foundInstance->second++;
            }
        }
        if (!found) {
            requestedServices_[service][instance] = 1;
        }
    }
    if (!found) {
        application_->request_service(service, instance,
                                      majorVersion, minorVersion);

        vsomeip::message_handler_t handler
            = std::bind(&Connection::receive, shared_from_this(), std::placeholders::_1);
        application_->register_message_handler(service, instance, ANY_METHOD, handler);
    }
}

void
Connection::releaseService(const Address &_address) {
    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();
    {
        std::lock_guard<std::mutex> lock(requestedServicesMutex_);
        auto foundService = requestedServices_.find(service);
        if (foundService != requestedServices_.end()) {
            auto foundInstance = foundService->second.find((instance));
            if (foundInstance != foundService->second.end()) {
                if (foundInstance->second > 0) {
                    foundInstance->second--;
                }
                if (!foundInstance->second) {
                    application_->release_service(service, instance);

                    foundService->second.erase(instance);
                    if (!foundService->second.size()) {
                        requestedServices_.erase(service);
                    }
                }
            }
        }
    }

}

void
Connection::registerEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event, const std::set<eventgroup_id_t> &_eventGroups,
        event_type_e _type, reliability_type_e _reliability) {
    {
        std::lock_guard<std::mutex> itsLock(registeredEventsMutex_);
        bool found(false);
        const auto foundService = registeredEvents_.find(_service);
        if (foundService != registeredEvents_.end()) {
            const auto foundInstance = foundService->second.find(_instance);
            if (foundInstance != foundService->second.end()) {
                foundInstance->second.insert(_event);
                found = true;
            }
        }

        if (!found) {
            registeredEvents_[_service][_instance].insert(_event);
        }
    }
    application_->offer_event(_service, _instance,
            _event, _eventGroups, _type, std::chrono::milliseconds::zero(),
            false,true, nullptr, _reliability);
}

void
Connection::unregisterEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event) {
    {
        std::lock_guard<std::mutex> itsLock(registeredEventsMutex_);
        const auto foundService = registeredEvents_.find(_service);
        if (foundService != registeredEvents_.end()) {
            const auto foundInstance = foundService->second.find(_instance);
            if (foundInstance != foundService->second.end()) {
                foundInstance->second.erase(_event);
                if (!foundInstance->second.size()) {
                    foundService->second.erase(foundInstance);
                    if (!foundService->second.size()) {
                        registeredEvents_.erase(foundService);
                    }
                }
            }
        }
    }
    application_->stop_offer_event(_service, _instance, _event);
}

void
Connection::requestEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event, eventgroup_id_t _eventGroup, event_type_e _type,
        reliability_type_e _reliability) {
    bool found(false);
    {
        std::lock_guard<std::mutex> lock(requestedEventsMutex_);
        auto foundService = requestedEvents_.find(_service);
        if (foundService != requestedEvents_.end()) {
            auto foundInstance = foundService->second.find(_instance);
            if (foundInstance != foundService->second.end()) {
                auto foundEventgroup = foundInstance->second.find(_eventGroup);
                if (foundEventgroup != foundInstance->second.end()) {
                    auto foundEvent = foundEventgroup->second.find(_event);
                    if (foundEvent != foundEventgroup->second.end()) {
                        found = true;
                        foundEvent->second++;
                    }
                }
            }
        }

        if (!found) {
            requestedEvents_[_service][_instance][_eventGroup][_event] = 1;
        }
    }
    if (!found) {
        std::set<eventgroup_id_t> itsEventGroups;
        itsEventGroups.insert(_eventGroup);

        application_->request_event(_service, _instance,
                _event, itsEventGroups, _type, _reliability);
    }
}

void
Connection::releaseEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event) {
    bool isLast(false);
    {
        std::lock_guard<std::mutex> lock(requestedEventsMutex_);
        auto foundService = requestedEvents_.find(_service);
        if (foundService != requestedEvents_.end()) {
            auto foundInstance = foundService->second.find(_instance);
            if (foundInstance != foundService->second.end()) {
                for (auto foundEventgroup = foundInstance->second.begin();
                        foundEventgroup != foundInstance->second.end(); ) {
                    auto foundEvent = foundEventgroup->second.find(_event);
                    if (foundEvent != foundEventgroup->second.end()) {
                        if (foundEvent->second > 0) {
                            foundEvent->second--;
                        }
                        if (!foundEvent->second) {
                            isLast = true;
                            foundEventgroup->second.erase(_event);
                        }
                    }
                    if (!foundEventgroup->second.size()) {
                        foundEventgroup = foundInstance->second.erase(foundEventgroup);
                    } else {
                        foundEventgroup++;
                    }
                }
                if (!foundInstance->second.size()) {
                    foundService->second.erase(_instance);
                }
            }
            if (!foundService->second.size()) {
                requestedEvents_.erase(_service);
            }
        }
    }
    if (isLast) {
        application_->release_event(_service, _instance, _event);
    }
}

const std::shared_ptr<StubManager> Connection::getStubManager() {
    if (!stubManager_) {
        stubManagerGuard_.lock();
        if (!stubManager_) {
            stubManager_ = std::make_shared<StubManager>(shared_from_this());
        }
        stubManagerGuard_.unlock();
    }
    return stubManager_;
}

void Connection::setStubMessageHandler(MessageHandler_t _handler) {
    stubMessageHandler_ = _handler;
}

bool Connection::isStubMessageHandlerSet() {
    return stubMessageHandler_.operator bool();
}

void Connection::processMsgQueueEntry(MsgQueueEntry &_msgQueueEntry) {
    commDirectionType commDirType = _msgQueueEntry.directionType_;

    switch(commDirType) {
    case commDirectionType::PROXYRECEIVE:
        handleProxyReceive(_msgQueueEntry.message_);
        break;
    case commDirectionType::STUBRECEIVE:
        handleStubReceive(_msgQueueEntry.message_);
        break;
    default:
        COMMONAPI_ERROR("Mainloop: Unknown communication direction!");
        break;
    }
}

void Connection::processAvblQueueEntry(AvblQueueEntry &_avblQueueEntry) {
    handleAvailabilityChange(_avblQueueEntry.service_, _avblQueueEntry.instance_,
            _avblQueueEntry.isAvailable_);
}

void Connection::processErrQueueEntry(ErrQueueEntry &_errQueueEntry) {
    auto its_tuple = std::make_tuple(_errQueueEntry.service_, _errQueueEntry.instance_,
            _errQueueEntry.eventGroup_, _errQueueEntry.event_);
    std::lock_guard<std::mutex> lock(eventHandlerMutex_);
    auto its_wrapper = subscriptionStates_.find(its_tuple);
    if (its_wrapper != subscriptionStates_.end()) {
        if(auto itsHandler = _errQueueEntry.eventHandler_.lock()) {
            if (its_wrapper->second->hasHandler(itsHandler.get(), _errQueueEntry.tag_)) {
                itsHandler->onError(_errQueueEntry.errorCode_, _errQueueEntry.tag_);
            }
        }
    }
}

const ConnectionId_t& Connection::getConnectionId() {
    return static_cast<const ConnectionId_t&>(application_->get_name());
}

void Connection::queueSelectiveErrorHandler(service_id_t serviceId,
        instance_id_t instanceId) {
    (void)serviceId;
    (void)instanceId;

    // Keep only for compatibility reasons
}

void Connection::queueSubscriptionStatusHandler(service_id_t serviceId,
                                              instance_id_t instanceId) {
    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    auto findService = subscriptions_.find(serviceId);
    if (findService != subscriptions_.end()) {
        auto findInstance = findService->second.find(instanceId);
        if (findInstance != findService->second.end()) {
            for (const auto& its_tuple : subscriptionStates_) {
                if (serviceId == std::get<0>(its_tuple.first) &&
                        instanceId == std::get<1>(its_tuple.first)) {
                    its_tuple.second->pushOnPendingHandlerQueue();
                }
            }
        }
    }
}

void Connection::registerSubscriptionHandler(const Address &_address,
        const eventgroup_id_t _eventgroup, AsyncSubscriptionHandler_t _handler) {

    std::lock_guard<std::mutex> itsLock(subscriptionMutex_);
    subscription_[_address.getService()][_address.getInstance()][_eventgroup] = _handler;

    if (auto lockedContext = mainLoopContext_.lock()) {
        (void)lockedContext;

        // create async subscription handler which notifies the stack about
        // the subscription result (accepted or not)
        auto self = shared_from_this();
        auto itsAsyncSubscriptionHandler = [this, self, _address, _eventgroup](
            client_id_t _client,
            uid_t _uid, gid_t _gid,
            bool _subscribe,
            SubscriptionAcceptedHandler_t _acceptedHandler) {

            // hooks must be called by the mainloop
            proxyPushFunctionToMainLoop([this, _client, _uid, _gid, _subscribe, _acceptedHandler, _address, _eventgroup]() {
                AsyncSubscriptionHandler_t itsHandler;
                {
                    std::lock_guard<std::mutex> itsLock(subscriptionMutex_);
                    auto foundService = subscription_.find(_address.getService());
                    if (foundService != subscription_.end()) {
                        auto foundInstance = foundService->second.find(_address.getInstance());
                        if (foundInstance != foundService->second.end()) {
                            auto foundEventgroup = foundInstance->second.find(_eventgroup);
                            if (foundEventgroup != foundInstance->second.end()) {
                                itsHandler = foundEventgroup->second;
                            }
                        }
                    }
                }
                if(itsHandler) {
                    itsHandler(_client, _uid, _gid, _subscribe, _acceptedHandler);
                } else {
                    _acceptedHandler(true);
                }
            });

        };
        application_->register_async_subscription_handler(_address.getService(), _address.getInstance(), _eventgroup, itsAsyncSubscriptionHandler);
    } else {
        application_->register_async_subscription_handler(_address.getService(), _address.getInstance(), _eventgroup, _handler);
    }
}

void Connection::unregisterSubscriptionHandler(const Address &_address,
        const eventgroup_id_t _eventgroup) {
    std::lock_guard<std::mutex> itsLock(subscriptionMutex_);
    {
        auto foundService = subscription_.find(_address.getService());
        if (foundService != subscription_.end()) {
            auto foundInstance = foundService->second.find(_address.getInstance());
            if (foundInstance != foundService->second.end()) {
                auto foundEventgroup = foundInstance->second.find(_eventgroup);
                if (foundEventgroup != foundInstance->second.end()) {
                    foundInstance->second.erase(_eventgroup);
                }
            }
        }
    }
    application_->unregister_subscription_handler(_address.getService(), _address.getInstance(), _eventgroup);
}

void Connection::incrementConnection() {
    std::lock_guard < std::mutex > lock(activeConnectionsMutex_);
    activeConnections_++;
}

void Connection::decrementConnection() {
    uint32_t activeConnections = 0;
    {
    std::lock_guard < std::mutex > lock(activeConnectionsMutex_);
    activeConnections = --activeConnections_;
    }

    if (!activeConnections) {
        disconnect();
    }
}

void Connection::proxyPushMessageToMainLoop(const Message &_message,
                                  std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler) {
    //add message to the async answers
    {
        std::lock_guard<std::recursive_mutex> lock(sendReceiveMutex_);
        auto timeoutTime = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now()
                           + std::chrono::milliseconds(ASYNC_MESSAGE_REPLY_TIMEOUT_MS);
        session_id_fake_t itsSession = _message.getSessionId();
        if (itsSession == 0) {
            static std::uint16_t fakeSessionId = 0;
            fakeSessionId++;
            if (fakeSessionId == 0) {
                fakeSessionId++; // set to 1 on overflow
            }
            itsSession = static_cast<session_id_fake_t>(fakeSessionId << 16);
            errorResponses_[_message.message_] = itsSession;
        }
        asyncAnswers_[itsSession]
            = std::make_tuple(
                    timeoutTime, _message.message_,
                    std::move(messageReplyAsyncHandler), 0);
    }

    //handle the message by the mainloop or by the current thread
    receive(_message.message_);
}

void Connection::getAvailableInstances(service_id_t _serviceId, std::vector<std::string> *_instances) {
    vsomeip::application::available_t itsAvailableServices;
    if(application_->are_available(itsAvailableServices, _serviceId)) {
        for(auto itsAvailableServicesIt = itsAvailableServices.begin();
                itsAvailableServicesIt != itsAvailableServices.end();
                ++itsAvailableServicesIt) {
            for(auto itsAvailableInstancesIt = itsAvailableServicesIt->second.begin();
                    itsAvailableInstancesIt != itsAvailableServicesIt->second.end();
                    ++itsAvailableInstancesIt) {
                Address service(_serviceId, itsAvailableInstancesIt->first);
                CommonAPI::Address capiAddressService;
                AddressTranslator::get()->translate(service, capiAddressService);
                _instances->push_back(capiAddressService.getInstance());
            }
        }
    }
}

void Connection::insertSubscriptionStatusListener(service_id_t serviceId, instance_id_t instanceId,
        eventgroup_id_t eventGroupId, event_id_t eventId,
        std::weak_ptr<ProxyConnection::EventHandler> eventHandler,
        uint32_t _tag) {
    auto itsTuple = std::make_tuple(serviceId, instanceId, eventGroupId, eventId);
    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    auto its_wrapper = subscriptionStates_.find(itsTuple);
    if (its_wrapper != subscriptionStates_.end()) {
        its_wrapper->second->addHandler(eventHandler, _tag);
    } else {
        auto subscriptionStatus = std::make_shared<SubscriptionStatusWrapper>(serviceId, instanceId, eventGroupId, eventId);
        subscriptionStatus->addHandler(eventHandler, _tag);
        subscriptionStates_[itsTuple] = subscriptionStatus;
    }
}

} // namespace SomeIP
} // namespace CommonAPI
