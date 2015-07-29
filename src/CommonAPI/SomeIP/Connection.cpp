// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <map>

#include <vsomeip/vsomeip.hpp>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/Config.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/Defines.hpp>

namespace CommonAPI {
namespace SomeIP {

void Connection::proxyReceive(const std::shared_ptr<vsomeip::message> &_message) {

	if (auto lockedContext = mainLoopContext_.lock()) {
		Watch::msgQueueEntry msg_queue_entry(_message, Watch::commDirectionType::PROXYRECEIVE);
		watch_->pushQueue(msg_queue_entry);
	}
	else {
		handleProxyReceive(_message);
	}
}

void Connection::handleProxyReceive(const std::shared_ptr<vsomeip::message> &_message) {
    session_id_t sessionId = _message->get_session();
    std::unique_lock<std::mutex> lock(sendReceiveMutex_);

    // handle events
    if(_message->get_message_type() == message_type_e::MT_NOTIFICATION) {
        service_id_t serviceId = _message->get_service();
        instance_id_t instanceId = _message->get_instance();
        event_id_t eventId = _message->get_method();

        std::set<ProxyConnection::EventHandler *> handlers;
        {
            std::unique_lock<std::mutex> eventLock(eventHandlerMutex_);
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

        for (auto handler : handlers)
            handler->onEventMessage(Message(_message));

        return;
    }

    // handle sync method calls
    if(sendAndBlockAnswer_.first == sessionId) {
        std::lock_guard< std::mutex > its_lock(sendAndBlockMutex_);
        sendAndBlockAnswer_.second = Message(_message);
        sendAndBlockWait_ = false;
        sendAndBlockCondition_.notify_one();

        return;
    }

    // handle async method calls
    async_answers_map_t::iterator foundAsyncHandler = asyncAnswers_.find(sessionId);

    if(foundAsyncHandler != asyncAnswers_.end()) {
    	CallStatus callStatus = (_message->get_return_code() == vsomeip::return_code_e::E_OK ?
    								CallStatus::SUCCESS : CallStatus::REMOTE_ERROR);
        std::get<2>(foundAsyncHandler->second)->onMessageReply(callStatus, Message(_message));
        asyncAnswers_.erase(sessionId);
    }
}

void Connection::stubReceive(const std::shared_ptr<vsomeip::message> &_message) {
	if (auto lockedContext = mainLoopContext_.lock()) {
		Watch::msgQueueEntry msg_queue_entry(_message, Watch::commDirectionType::STUBRECEIVE);
		watch_->pushQueue(msg_queue_entry);
	}
	else {
		handleStubReceive(_message);
	}
}

void Connection::handleStubReceive(const std::shared_ptr<vsomeip::message> &_message) {
    if(stubMessageHandler_) {
        stubMessageHandler_(Message(_message));
    }
}

void Connection::onConnectionEvent(event_type_e event) {
    connectionStatus_ = event;
    connectionCondition_.notify_one();
}

void Connection::onAvailabilityChange(service_id_t _service, instance_id_t _instance,
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
						Watch::msgQueueEntry msg_queue_entry(response, Watch::commDirectionType::PROXYRECEIVE);
						watch_->pushQueue(msg_queue_entry);
						it++;
					} else {
						std::get<2>(it->second)->onMessageReply(CallStatus::REMOTE_ERROR, Message(response));
						it = asyncAnswers_.erase(it);
					}
				} else {
					it++;
				}
			}
		}

    	timeout = std::numeric_limits<int>::max();
		std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
		for (auto it = asyncAnswers_.begin(); it != asyncAnswers_.end(); it++) {
			int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::get<0>(it->second) - now).count();
			if (timeout > remaining)
				timeout = remaining;
		}
    }
}

Connection::Connection(const std::string &_name)
      : dispatchThread_(NULL),
        asyncAnswersCleanupThread_(NULL),
        connectionStatus_(event_type_e::ET_DEREGISTERED),
        application_(vsomeip::runtime::get()->create_application(_name)),
        sendAndBlockWait_(true),
        executeEndlessPoll(false),
		cleanupCancelled_(false) {

    application_->init(); //TODO error handling

    std::function<void(event_type_e)> connectionHandler = std::bind(&Connection::onConnectionEvent,
                                                                    this,
                                                                    std::placeholders::_1);
    application_->register_event_handler(connectionHandler);
}

Connection::~Connection() {
    application_->stop();

    if(NULL != dispatchThread_) {
        dispatchThread_->join();
        delete dispatchThread_;
    }

    if (asyncAnswersCleanupThread_) {
    	cleanupCancelled_ = true;
    	cleanupCondition_.notify_one();
        asyncAnswersCleanupThread_->join();
    }

    if (auto lockedContext = mainLoopContext_.lock()) {
    	lockedContext->deregisterWatch(watch_.get());
    	lockedContext->deregisterDispatchSource(dispatchSource_.get());
    }
}

bool Connection::attachMainLoopContext(std::weak_ptr<MainLoopContext> mainLoopContext) {
	if (mainLoopContext_.lock() == mainLoopContext.lock())
	    return true;

    bool result = false;

	mainLoopContext_ = mainLoopContext;

	if (auto lockedContext = mainLoopContext_.lock()) {
	    if (!watch_)
	        watch_ = std::make_shared<Watch>(shared_from_this());
	    if (!dispatchSource_)
	        dispatchSource_ = std::make_shared<DispatchSource>(watch_);
		lockedContext->registerDispatchSource(dispatchSource_.get());
		lockedContext->registerWatch(watch_.get());

		lockedContext->wakeup();

		result = true;
	}

    return result;
}

bool Connection::connect(bool startDispatchThread) {
    std::unique_lock<std::mutex> lock(connectionMutex_);

#ifndef WIN32
    asyncAnswersCleanupThread_ = std::make_shared<std::thread>(&Connection::cleanup, this);
#endif
    dispatchThread_ = new std::thread(&Connection::dispatch, this);
    return isConnected();
}

void Connection::disconnect() {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    application_->stop();

    while(connectionStatus_ != event_type_e::ET_DEREGISTERED) {
        connectionCondition_.wait(lock);
    }
}

bool Connection::isConnected() const {
    return (connectionStatus_ == event_type_e::ET_REGISTERED);
}

void Connection::waitUntilConnected() {
	std::unique_lock<std::mutex> itsLock(connectionMutex_);
	while (!isConnected())
		connectionCondition_.wait(itsLock);
}

ProxyConnection::ConnectionStatusEvent& Connection::getConnectionStatusEvent() {
    return connectionStatusEvent_;
}

bool Connection::sendMessage(const Message& message, uint32_t* allocatedSerial) const {
    if (!isConnected())
    	return false;

    application_->send(message.message_);
    return true;
}

bool Connection::sendEvent(const Message &message, uint32_t *allocatedSerial) const {
    application_->notify(message.getServiceId(), message.getInstanceId(),
    		message.getMethodId(), message.message_->get_payload());

    return true;
}

bool Connection::sendEvent(const Message &message, client_id_t _client,
        uint32_t *allocatedSerial) const {
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

    auto timeoutTime = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(_info->timeout_);
    asyncAnswers_[message.getSessionId()] = std::make_tuple(timeoutTime, message.message_, std::move(messageReplyAsyncHandler));
    cleanupCondition_.notify_one();

    return replyAsyncHandler->getFuture();
}

Message Connection::sendMessageWithReplyAndBlock(
        const Message& message,
		const CommonAPI::CallInfo *_info) const {

	if (!isConnected())
		return Message();

    {
        std::unique_lock<std::mutex> lock(sendReceiveMutex_);
        application_->send(message.message_, true);

        if (_info->sender_ != 0) {
			COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_,
						" - ClientID: ", message.getClientId(),
						", SessionID: ", message.getSessionId());
        }

        sendAndBlockAnswer_.first = message.getSessionId();
    }

    std::unique_lock<std::mutex> lock(sendAndBlockMutex_);
    std::cv_status waitStatus = std::cv_status::no_timeout;

    if(sendAndBlockWait_) {
        waitStatus = sendAndBlockCondition_.wait_for(lock, std::chrono::milliseconds(_info->timeout_));
    }
    sendAndBlockWait_ = true;

    return (waitStatus == std::cv_status::no_timeout) ? sendAndBlockAnswer_.second : Message();
}

void Connection::addEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler) {

    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    eventHandlers_[serviceId][instanceId][eventId].insert(eventHandler);

    subscriptions_[serviceId][instanceId].insert(eventGroupId);

    if (application_->is_available(serviceId, instanceId))
        application_->subscribe(serviceId, instanceId, eventGroupId);
}

void Connection::removeEventHandler(
        service_id_t serviceId,
        instance_id_t instanceId,
        eventgroup_id_t eventGroupId,
        event_id_t eventId,
        ProxyConnection::EventHandler* eventHandler) {

    std::unique_lock<std::mutex> lock(eventHandlerMutex_);
    auto foundService = eventHandlers_.find(serviceId);
    if (foundService != eventHandlers_.end()) {
        auto foundInstance = foundService->second.find(instanceId);
        if (foundInstance != foundService->second.end()) {
            auto foundEventId = foundInstance->second.find(eventId);
            if (foundEventId != foundInstance->second.end()) {
                foundEventId->second.erase(eventHandler);
                if (foundEventId->second.size() == 0)
                    foundInstance->second.erase(foundEventId);
                application_->unsubscribe(serviceId, instanceId, eventGroupId);
            }
        }
    }

    auto foundPendingService = subscriptions_.find(serviceId);
    if (foundPendingService != subscriptions_.end()) {
        auto foundPendingInstance = foundPendingService->second.find(instanceId);
        if (foundPendingInstance != foundPendingService->second.end()) {
            foundPendingInstance->second.erase(eventGroupId);
            if (foundPendingInstance->second.size() == 0) {
                foundPendingService->second.erase(foundPendingInstance);
                if (foundPendingService->second.size() == 0)
                    subscriptions_.erase(foundPendingService);
            }
        }
    }
}

bool
Connection::isAvailable(const Address &_address) {
    return application_->is_available(_address.getService(), _address.getInstance());
}

AvailabilityHandlerId_t
Connection::registerAvailabilityHandler(
		const Address &_address, AvailabilityHandler_t _handler) {
    static AvailabilityHandlerId_t itsHandlerId = 0;
    AvailabilityHandlerId_t itsCurrentHandlerId;
    bool isRegistered(false);

    service_id_t itsService = _address.getService();
    instance_id_t itsInstance = _address.getInstance();

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
                itsService, itsInstance, itsHandler);
    }

    return itsCurrentHandlerId;
}

void
Connection::unregisterAvailabilityHandler(
		const Address &_address, AvailabilityHandlerId_t _handlerId) {
    bool mustUnregister(false);

    service_id_t itsService = _address.getService();
    instance_id_t itsInstance = _address.getInstance();

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
                itsService, itsInstance);
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

    application_->offer_service(service, instance, majorVersion, minorVersion);

    vsomeip::message_handler_t handler
    	= std::bind(&Connection::stubReceive, this, std::placeholders::_1);
    application_->register_message_handler(service, instance, SOMEIP_ANY_METHOD, handler);
}

void
Connection::unregisterService(const Address &_address) {
    if (!stubMessageHandler_)
        return;

    service_id_t service = _address.getService();
    instance_id_t instance = _address.getInstance();

    application_->stop_offer_service(service, instance);
    application_->unregister_message_handler(service, instance, SOMEIP_ANY_METHOD);
}

void
Connection::requestService(const Address &_address, bool _hasSelective) {
	service_id_t service = _address.getService();
	instance_id_t instance = _address.getInstance();
	major_version_t majorVersion = _address.getMajorVersion();
	minor_version_t minorVersion = _address.getMinorVersion();

    application_->request_service(service, instance, _hasSelective, majorVersion, minorVersion);

    vsomeip::message_handler_t handler
    	= std::bind(&Connection::proxyReceive, this, std::placeholders::_1);
    application_->register_message_handler(service, instance, SOMEIP_ANY_METHOD, handler);
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

void Connection::processMsgQueueEntry(Watch::msgQueueEntry &_msgQueueEntry) {
	Watch::commDirectionType commDirType = _msgQueueEntry.second;

	switch(commDirType) {
	case Watch::commDirectionType::PROXYRECEIVE:
		handleProxyReceive(_msgQueueEntry.first);
		break;
	case Watch::commDirectionType::STUBRECEIVE:
		handleStubReceive(_msgQueueEntry.first);
		break;
	default:
		COMMONAPI_ERROR("Mainloop: Unknown communication direction!");
		break;
	}
}

const ConnectionId_t& Connection::getConnectionId() {
    return static_cast<const ConnectionId_t&>(application_->get_name());
}

void Connection::sendPendingSubscriptions(service_id_t serviceId, instance_id_t instanceId) const {
    std::unique_lock<std::mutex> lock(eventHandlerMutex_);

    auto findService = subscriptions_.find(serviceId);
    if (findService != subscriptions_.end()) {
        auto findInstance = findService->second.find(instanceId);
        if (findInstance != findService->second.end()) {
            for (auto &e : findInstance->second)
                application_->subscribe(serviceId, instanceId, e);
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

} // namespace SomeIP
} // namespace CommonAPI
