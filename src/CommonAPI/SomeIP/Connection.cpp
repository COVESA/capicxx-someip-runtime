// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include <CommonAPI/SomeIP/Config.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/Defines.hpp>
#include <CommonAPI/SomeIP/ProxyAsyncEventCallbackHandler.hpp>

namespace CommonAPI {
namespace SomeIP {

void Connection::proxyReceive(const std::shared_ptr<vsomeip::message> &_message) {
    if (auto lockedContext = mainLoopContext_.lock()) {
        std::shared_ptr<Watch::MsgQueueEntry> msg_queue_entry = std::make_shared<Watch::MsgQueueEntry>(
                watch_, _message, Watch::commDirectionType::PROXYRECEIVE);
        watch_->pushQueue(msg_queue_entry);
    }
    else {
        handleProxyReceive(_message);
    }
}

void Connection::handleProxyReceive(const std::shared_ptr<vsomeip::message> &_message) {
    sendReceiveMutex_.lock();

    session_id_t sessionId = _message->get_session();

    // handle events
    if(_message->get_message_type() == message_type_e::MT_NOTIFICATION) {
        service_id_t serviceId = _message->get_service();
        instance_id_t instanceId = _message->get_instance();
        event_id_t eventId = _message->get_method();

        std::set<ProxyConnection::EventHandler *> handlers;
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
        for (auto handler : handlers)
            handler->onEventMessage(Message(_message));

        return;
    }

    // handle sync method calls
    auto foundSession = sendAndBlockAnswers_.find(sessionId);
    if (foundSession != sendAndBlockAnswers_.end()) {
        foundSession->second = Message(_message);
        sendReceiveMutex_.unlock();
        std::lock_guard< std::mutex > its_lock(sendAndBlockMutex_);
        sendAndBlockCondition_.notify_all();
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

        itsHandler->onMessageReply(callStatus, Message(_message));
    } else {
        sendReceiveMutex_.unlock();
    }
}

void Connection::stubReceive(const std::shared_ptr<vsomeip::message> &_message) {
    if (auto lockedContext = mainLoopContext_.lock()) {
        std::shared_ptr<Watch::MsgQueueEntry> msg_queue_entry = std::make_shared<Watch::MsgQueueEntry>(
                        watch_, _message, Watch::commDirectionType::STUBRECEIVE);
        watch_->pushQueue(msg_queue_entry);
    }
    else {
        handleStubReceive(_message);
    }
}

void Connection::handleStubReceive(const std::shared_ptr<vsomeip::message> &_message) {
    if(stubMessageHandler_) {
        if (!stubMessageHandler_(Message(_message))) {
            if (_message->get_message_type() == message_type_e::MT_REQUEST) {
                auto error = vsomeip::runtime::get()->create_response(_message);
                error->set_message_type(message_type_e::MT_ERROR);
                error->set_return_code(return_code_e::E_MALFORMED_MESSAGE);
                application_->send(error, true);
            }
        }
    }
}

void Connection::onConnectionEvent(state_type_e state) {
    std::lock_guard<std::mutex> itsLock(connectionMutex_);
    connectionStatus_ = state;
    connectionCondition_.notify_one();
}

void Connection::onAvailabilityChange(service_id_t _service, instance_id_t _instance,
           bool _is_available) {
    if (auto lockedContext = mainLoopContext_.lock()) {
        std::shared_ptr<Watch::AvblQueueEntry> avbl_queue_entry = std::make_shared<Watch::AvblQueueEntry>(
                        watch_, _service, _instance, _is_available);
        watch_->pushQueue(avbl_queue_entry);
    }
    else {
        handleAvailabilityChange(_service, _instance, _is_available);
    }
}

void Connection::handleAvailabilityChange(const service_id_t _service, instance_id_t _instance,
                                          bool _is_available) {
    std::list<AvailabilityHandler_t> itsHandlers;

    {
        std::unique_lock<std::mutex> itsLock(availabilityMutex_);
        auto foundService = availabilityHandlers_.find(_service);
        if (foundService != availabilityHandlers_.end()) {
            auto foundInstance = foundService->second.find(_instance);
            if (foundInstance != foundService->second.end()) {
                for (auto &h : foundInstance->second)
                    itsHandlers.push_back(h.second);
            }
            auto foundWildcardInstance = foundService->second.find(vsomeip::ANY_INSTANCE);
            if (foundWildcardInstance != foundService->second.end()) {
                for (auto &h : foundWildcardInstance->second)
                    itsHandlers.push_back(h.second);
            }
        }
    }

    for (auto h : itsHandlers)
        h(_service, _instance, _is_available);

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
            std::lock_guard<std::mutex> lock(sendReceiveMutex_);
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            auto it = asyncAnswers_.begin();
            while (it != asyncAnswers_.end()) {
                if(now > std::get<0>(it->second)) {
                    std::shared_ptr<vsomeip::message> response
                        = vsomeip::runtime::get()->create_response(std::get<1>(it->second));
                    response->set_message_type(vsomeip::message_type_e::MT_ERROR);
                    response->set_return_code(vsomeip::return_code_e::E_TIMEOUT);
                    if (auto lockedContext = mainLoopContext_.lock()) {
                        std::shared_ptr<Watch::MsgQueueEntry> msg_queue_entry = std::make_shared<Watch::MsgQueueEntry>(
                                        watch_, response, Watch::commDirectionType::PROXYRECEIVE);
                        watch_->pushQueue(msg_queue_entry);
                    } else {
                        std::get<2>(it->second)->onMessageReply(CallStatus::REMOTE_ERROR, Message(response));
                    }
                    it = asyncAnswers_.erase(it);
                } else {
                    it++;
                }
            }
        }

        {
            timeout = std::numeric_limits<int>::max();
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::lock_guard<std::mutex> lock(sendReceiveMutex_);
            for (auto it = asyncAnswers_.begin(); it != asyncAnswers_.end(); it++) {
                int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::get<0>(it->second) - now).count();
                if (timeout > remaining)
                    timeout = remaining;
            }
        }
    }
}

Connection::Connection(const std::string &_name)
      : dispatchThread_(NULL),
        connectionStatus_(state_type_e::ST_DEREGISTERED),
        application_(vsomeip::runtime::get()->create_application(_name)),
        asyncAnswersCleanupThread_(NULL),
        cleanupCancelled_(false),
        activeConnections_(0) {
    std::string appId = Runtime::getProperty("LogApplication");
    std::string contextId = Runtime::getProperty("LogContext");

    if (appId != "")
        vsomeip::runtime::set_property("LogApplication", appId);

    if (contextId != "")
        vsomeip::runtime::set_property("LogContext", contextId);
}

Connection::~Connection() {
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

#ifndef WIN32
    asyncAnswersCleanupThread_ = std::make_shared<std::thread>(&Connection::cleanup, this);
#endif
    dispatchThread_ = new std::thread(&Connection::dispatch, this);
    return true;
}

void Connection::disconnect() {
    if (auto lockedContext = mainLoopContext_.lock()) {
        lockedContext->deregisterWatch(watch_);
        lockedContext->deregisterDispatchSource(dispatchSource_);
    }
    if (asyncAnswersCleanupThread_) {
        cleanupCancelled_ = true;
        cleanupCondition_.notify_one();
        if (asyncAnswersCleanupThread_->joinable())
            asyncAnswersCleanupThread_->join();
    }

    application_->stop();
    if(dispatchThread_) {
        if (dispatchThread_->joinable())
            dispatchThread_->join();
        delete dispatchThread_;
    }
    application_->clear_all_handler();
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

    application_->send(message.message_);
    return true;
}

bool Connection::sendEvent(const Message &message, uint32_t *) const {
    application_->notify(message.getServiceId(), message.getInstanceId(),
            message.getMethodId(), message.message_->get_payload());

    return true;
}

bool Connection::sendEvent(const Message &message, client_id_t _client,
        uint32_t *) const {
    application_->notify_one(message.getServiceId(), message.getInstanceId(), message.getMethodId(),
            message.message_->get_payload(), _client);

    return true;
}

std::future<CallStatus> Connection::sendMessageWithReplyAsync(
        const Message& message,
        std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler,
        const CommonAPI::CallInfo *_info) const {

    if (!isConnected())
        return std::future<CallStatus>();

    std::lock_guard<std::mutex> lock(sendReceiveMutex_);
    application_->send(message.message_, true);

    if (_info->sender_ != 0) {
        COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_,
                " - ClientID: ", message.getClientId(),
                ", SessionID: ", message.getSessionId());
    }

    MessageReplyAsyncHandler* replyAsyncHandler = messageReplyAsyncHandler.get();

    std::future<CallStatus> callStatusFuture;
    try {
        callStatusFuture = replyAsyncHandler->getFuture();
    } catch (std::exception& e) {
        (void)e;
    }

    auto timeoutTime = std::chrono::high_resolution_clock::now()
    				   + std::chrono::milliseconds(_info->timeout_);
    asyncAnswers_[message.getSessionId()]
		= std::make_tuple(
				timeoutTime, message.message_,
				std::move(messageReplyAsyncHandler));
    cleanupCondition_.notify_one();

    return callStatusFuture;
}

Message Connection::sendMessageWithReplyAndBlock(
        const Message& message,
        const CommonAPI::CallInfo *_info) const {

	if (!isConnected())
        return Message();

	std::pair<std::map<session_id_t, Message>::iterator, bool> itsAnswer;
    {
        std::lock_guard<std::mutex> lock(sendReceiveMutex_);
        application_->send(message.message_, true);
        if (_info->sender_ != 0) {
            COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_,
                        " - ClientID: ", message.getClientId(),
                        ", SessionID: ", message.getSessionId());
        }

        itsAnswer = sendAndBlockAnswers_.emplace(message.getSessionId(), Message());
    }

    std::unique_lock<std::mutex> lock(sendAndBlockMutex_);
    std::cv_status waitStatus = std::cv_status::no_timeout;

    Message itsResult;

    std::chrono::system_clock::time_point elapsed(
    		std::chrono::system_clock::now()
    		+ std::chrono::milliseconds(_info->timeout_));

    // The while condition implicitly checks whether we received the answer
    // before acquiring sendAndBlockMutex_
    while (!itsAnswer.first->second) {
		waitStatus = sendAndBlockCondition_.wait_until(lock, elapsed);
		if (waitStatus == std::cv_status::timeout || itsAnswer.first->second)
			break;
    }

    // If there was an answer (thus, we did not run into the timeout),
    // move it to itsResult
    if (waitStatus != std::cv_status::timeout) {
		std::unique_lock<std::mutex> lock(sendReceiveMutex_);
		itsResult = std::move(itsAnswer.first->second);
		sendAndBlockAnswers_.erase(itsAnswer.first);
    }

    return itsResult;
}

void Connection::addEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler,
        major_version_t major,
        bool isField,
        bool isSelective) {

    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    eventHandlers_[serviceId][instanceId][eventId].insert(eventHandler);
    const bool inserted(std::get<1>(subscriptions_[serviceId][instanceId][eventId].insert(eventGroupId)));

    if(inserted) {
        if(!isField) {
            application_->subscribe(serviceId, instanceId, eventGroupId, major,
                    vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE, eventId);
        }
        if(isSelective) {
            addSelectiveErrorListener(serviceId, instanceId, eventGroupId);
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

    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    auto foundService = eventHandlers_.find(serviceId);
    if (foundService != eventHandlers_.end()) {
        auto foundInstance = foundService->second.find(instanceId);
        if (foundInstance != foundService->second.end()) {
            auto foundEventId = foundInstance->second.find(eventId);
            if (foundEventId != foundInstance->second.end()) {
                foundEventId->second.erase(eventHandler);
                if (foundEventId->second.size() == 0) {
                    foundInstance->second.erase(foundEventId);
                    if (application_->is_available(serviceId, instanceId, major, minor)) {
                        application_->unsubscribe(serviceId, instanceId, eventGroupId);
                    }
                }
            }
        }
    }

    auto foundPendingService = subscriptions_.find(serviceId);
    if (foundPendingService != subscriptions_.end()) {
        auto foundPendingInstance = foundPendingService->second.find(instanceId);
        if (foundPendingInstance != foundPendingService->second.end()) {
            foundPendingInstance->second.erase(eventId);
            if (foundPendingInstance->second.size() == 0) {
                foundPendingService->second.erase(foundPendingInstance);
                if (foundPendingService->second.size() == 0)
                    subscriptions_.erase(foundPendingService);
            }
        }
    }

    auto foundService2 = inital_event_requests.find(serviceId);
    if (foundService2 != inital_event_requests.end()) {
        auto foundInstance = foundService2->second.find(instanceId);
        if (foundInstance != foundService2->second.end()) {
            foundInstance->second.clear();
        }
    }

    auto it_service = pendingSelectiveErrorHandlers_.find(serviceId);
    if (it_service != pendingSelectiveErrorHandlers_.end()) {
        auto it_instance = it_service->second.find(instanceId);
        if (it_instance != it_service->second.end()) {
            it_instance->second.erase(eventGroupId);
            if (it_instance->second.size() == 0) {
                it_service->second.erase(instanceId);
                if (it_service->second.size() == 0) {
                    pendingSelectiveErrorHandlers_.erase(serviceId);
                }
            }
        }
    }

    application_->unregister_subscription_error_handler(serviceId, instanceId,
            eventGroupId);
}

void Connection::subscribeForSelective(service_id_t serviceId, instance_id_t instanceId,
                     eventgroup_id_t eventGroupId, ProxyConnection::EventHandler* eventHandler,
					 uint32_t _tag, major_version_t major) {
    std::set<uint32_t> tags;
    tags.insert(_tag);

    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    selectiveErrorHandlers_[serviceId][instanceId][eventGroupId].push(std::make_pair(eventHandler, tags));
    pendingSelectiveErrorHandlers_[serviceId][instanceId][eventGroupId][eventHandler].insert(_tag);

    application_->subscribe(serviceId, instanceId, eventGroupId, major);
}

void Connection::addSelectiveErrorListener(service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId) {

    auto errorHandler = [serviceId, instanceId, eventGroupId, this] (
                    const uint16_t errorCode) {
        std::unique_lock<std::mutex> lock(eventHandlerMutex_);
        {
            auto it_service = selectiveErrorHandlers_.find(serviceId);
            if (it_service != selectiveErrorHandlers_.end()) {
                auto it_instance = it_service->second.find(instanceId);
                if (it_instance != it_service->second.end()) {
                    auto it_eventgroup = it_instance->second.find(eventGroupId);
                    if (it_eventgroup != it_instance->second.end()) {
                        if (!it_eventgroup->second.empty()) {
                            auto entry = it_eventgroup->second.front();
                            it_eventgroup->second.pop();
                            for (uint32_t tag : entry.second) {
                                ProxyConnection::EventHandler* handler = entry.first;
                                if (auto lockedContext = mainLoopContext_.lock()) {
                                    std::shared_ptr<Watch::ErrQueueEntry> err_queue_entry =
                                            std::make_shared<Watch::ErrQueueEntry>(
                                                    handler, errorCode, tag);
                                    watch_->pushQueue(err_queue_entry);
                                } else {
                                    handler->onError(errorCode, tag);
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    application_->register_subscription_error_handler(serviceId, instanceId,
                eventGroupId, errorHandler);
}

bool
Connection::isAvailable(const Address &_address) {
    return application_->is_available(_address.getService(), _address.getInstance(),
            _address.getMajorVersion(), _address.getMinorVersion());
}

AvailabilityHandlerId_t
Connection::registerAvailabilityHandler(
        const Address &_address, AvailabilityHandler_t _handler) {
    static AvailabilityHandlerId_t itsHandlerId = 0;
    AvailabilityHandlerId_t itsCurrentHandlerId;
    bool isRegistered(false);

    service_id_t itsService = _address.getService();
    instance_id_t itsInstance = _address.getInstance();
    major_version_t itsMajor = _address.getMajorVersion();
    minor_version_t itsMinor = _address.getMinorVersion();

    {
        std::unique_lock<std::mutex> itsLock(availabilityMutex_);
        itsHandlerId++;
        itsCurrentHandlerId = itsHandlerId;

        auto foundService = availabilityHandlers_.find(itsService);
        if (foundService != availabilityHandlers_.end()) {
            auto foundInstance = foundService->second.find(itsInstance);
            if (foundInstance != foundService->second.end()) {
                foundInstance->second[itsCurrentHandlerId] = _handler;
                isRegistered = true;
            } else {
                foundService->second[itsInstance][itsCurrentHandlerId] = _handler;
            }
        } else {
            availabilityHandlers_[itsService][itsInstance][itsCurrentHandlerId] = _handler;
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
    minor_version_t itsMinor = _address.getMinorVersion();

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
        = std::bind(&Connection::stubReceive, shared_from_this(), std::placeholders::_1);
    application_->register_message_handler(service, instance, SOMEIP_ANY_METHOD, handler);
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

    application_->stop_offer_service(service, instance, major, minor);
    application_->unregister_message_handler(service, instance, SOMEIP_ANY_METHOD);
}

void
Connection::requestService(const Address &_address, bool _hasSelective) {
    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();
    major_version_t majorVersion = _address.getMajorVersion();
    minor_version_t minorVersion = SOMEIP_ANY_MINOR_VERSION;

    application_->request_service(service, instance,
                                  majorVersion, minorVersion,
                                  _hasSelective);

    vsomeip::message_handler_t handler
        = std::bind(&Connection::proxyReceive, shared_from_this(), std::placeholders::_1);
    application_->register_message_handler(service, instance, SOMEIP_ANY_METHOD, handler);
}

void
Connection::releaseService(const Address &_address) {
    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();
    application_->release_service(service, instance);
}

void
Connection::registerEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event, const std::set<eventgroup_id_t> &_eventGroups, bool _isField) {
    application_->offer_event(_service, _instance,
            _event, _eventGroups, _isField);
}

void
Connection::unregisterEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event) {
    application_->stop_offer_event(_service, _instance, _event);
}

void
Connection::requestEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event, eventgroup_id_t _eventGroup, bool _isField) {
    std::set<eventgroup_id_t> itsEventGroups;
    itsEventGroups.insert(_eventGroup);

    application_->request_event(_service, _instance,
            _event, itsEventGroups, _isField);
}

void
Connection::releaseEvent(service_id_t _service, instance_id_t _instance,
        event_id_t _event) {
    application_->release_event(_service, _instance, _event);
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

void Connection::processMsgQueueEntry(Watch::MsgQueueEntry &_msgQueueEntry) {
    Watch::commDirectionType commDirType = _msgQueueEntry.directionType_;

    switch(commDirType) {
    case Watch::commDirectionType::PROXYRECEIVE:
        handleProxyReceive(_msgQueueEntry.message_);
        break;
    case Watch::commDirectionType::STUBRECEIVE:
        handleStubReceive(_msgQueueEntry.message_);
        break;
    default:
        COMMONAPI_ERROR("Mainloop: Unknown communication direction!");
        break;
    }
}

void Connection::processAvblQueueEntry(Watch::AvblQueueEntry &_avblQueueEntry) {
    handleAvailabilityChange(_avblQueueEntry.service_, _avblQueueEntry.instance_,
            _avblQueueEntry.isAvailable_);
}

void Connection::processFunctionQueueEntry(Watch::FunctionQueueEntry &_functionQueueEntry) {
    _functionQueueEntry.function_(_functionQueueEntry.value_);
}

const ConnectionId_t& Connection::getConnectionId() {
    return static_cast<const ConnectionId_t&>(application_->get_name());
}

void Connection::queueSelectiveErrorHandler(service_id_t serviceId,
                                              instance_id_t instanceId) {
    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    auto findService = subscriptions_.find(serviceId);
    if (findService != subscriptions_.end()) {
        auto findInstance = findService->second.find(instanceId);
        if (findInstance != findService->second.end()) {
            for (auto &e : findInstance->second) {
                auto it_service = pendingSelectiveErrorHandlers_.find(serviceId);
                if (it_service != pendingSelectiveErrorHandlers_.end()) {
                    auto it_instance = it_service->second.find(instanceId);
                    if (it_instance != it_service->second.end()) {
                        for (auto group : e.second) {
                            auto it_eventgroup = it_instance->second.find(group);
                            if (it_eventgroup != it_instance->second.end()) {
                                for (auto its_handler : it_eventgroup->second) {
                                    selectiveErrorHandlers_[serviceId][instanceId][group].push(
                                        std::make_pair(its_handler.first, its_handler.second));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void Connection::registerSubsciptionHandler(const Address &_address,
        const eventgroup_id_t _eventgroup, SubsciptionHandler_t _handler) {

    application_->register_subscription_handler(_address.getService(), _address.getInstance(), _eventgroup, _handler);
}

void Connection::unregisterSubsciptionHandler(const Address &_address,
        const eventgroup_id_t _eventgroup) {
    application_->unregister_subscription_handler(_address.getService(), _address.getInstance(), _eventgroup);
}

void Connection::getInitialEvent(service_id_t serviceId,
                                 instance_id_t instanceId,
                                 eventgroup_id_t eventGroupId,
                                 event_id_t eventId,
                                 major_version_t major) {
    application_->subscribe(serviceId, instanceId, eventGroupId, major,
            vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE, eventId);
}

void Connection::eventInitialValueCallback(const CallStatus callStatus,
            const Message& message, EventHandler *_eventHandler,
            const uint32_t tag) {

    if (_eventHandler && callStatus == CommonAPI::CallStatus::SUCCESS) {
        _eventHandler->onInitialValueEventMessage(message, tag);
    } else {
        COMMONAPI_ERROR("Subscribe: Get initial attribute value failed!");
    }
}

void Connection::incrementConnection() {
    std::lock_guard < std::mutex > lock(activeConnectionsMutex_);
    activeConnections_++;
}

void Connection::decrementConnection() {
    std::lock_guard < std::mutex > lock(activeConnectionsMutex_);
    activeConnections_--;

    if (!activeConnections_) {
        disconnect();
    }
}

void Connection::proxyPushMessage(const Message &_message,
                                  std::unique_ptr<MessageReplyAsyncHandler> messageReplyAsyncHandler) {
    //add message to the async answers
    {
        std::lock_guard<std::mutex> lock(sendReceiveMutex_);
        auto timeoutTime = std::chrono::high_resolution_clock::now()
                           + std::chrono::milliseconds(ASYNC_MESSAGE_REPLY_TIMEOUT_MS);
        asyncAnswers_[_message.getSessionId()]
            = std::make_tuple(
                    timeoutTime, _message.message_,
                    std::move(messageReplyAsyncHandler));
    }

    //handle the message by the mainloop or by the current thread
    proxyReceive(_message.message_);
}

void Connection::proxyPushFunction(std::function<void(const uint32_t)> _function, uint32_t _value) {
    if (auto lockedContext = mainLoopContext_.lock()) {
        std::shared_ptr<Watch::FunctionQueueEntry> functionQueueEntry = std::make_shared<Watch::FunctionQueueEntry>(
                watch_, _function, _value);
        watch_->pushQueue(functionQueueEntry);
    }
    else {
        _function(_value);
    }
}

} // namespace SomeIP
} // namespace CommonAPI
