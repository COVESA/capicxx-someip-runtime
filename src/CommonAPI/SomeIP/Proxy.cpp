// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/Utils.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/Factory.hpp>

namespace CommonAPI {
namespace SomeIP {

ProxyStatusEventHelper::ProxyStatusEventHelper(Proxy* proxy) :
        proxy_(proxy) {
}

void ProxyStatusEventHelper::onListenerAdded(const Listener& _listener,
                                             const Subscription _subscription) {
    std::lock_guard<std::recursive_mutex> listenersLock(listenersMutex_);

    //notify listener about availability status -> push function to mainloop
    std::weak_ptr<Proxy> itsProxy = proxy_->shared_from_this();
    proxy_->getConnection()->proxyPushFunctionToMainLoop<Connection>(
            Proxy::notifySpecificListener,
            itsProxy,
            _listener,
            _subscription);
}

void ProxyStatusEventHelper::onListenerRemoved(const Listener& _listener,
                                               const Subscription _subscription) {
    std::lock_guard<std::recursive_mutex> listenersLock(listenersMutex_);
    (void)_listener;
    auto listenerIt = listeners_.begin();
    while(listenerIt != listeners_.end()) {
        if(listenerIt->first == _subscription)
            listenerIt = listeners_.erase(listenerIt);
        else
            ++listenerIt;
    }
}

void Proxy::availabilityTimeoutThreadHandler() const {
    std::unique_lock<std::mutex> threadLock(availabilityTimeoutThreadMutex_);

    bool cancel = false;
    bool firstIteration = true;

    // the callbacks that have to be done are stored with
    // their required data in a list of tuples.
    typedef std::tuple<
            isAvailableAsyncCallback,
            std::promise<AvailabilityStatus>,
            AvailabilityStatus,
            std::chrono::steady_clock::time_point
            > CallbackData_t;
    std::list<CallbackData_t> callbacks;

    while(!cancel) {

        //get min timeout
        timeoutsMutex_.lock();

        int timeout = std::numeric_limits<int>::max();
        std::chrono::steady_clock::time_point minTimeout;
        if (timeouts_.size() > 0) {
            auto minTimeoutElement = std::min_element(timeouts_.begin(), timeouts_.end(),
                    [] (const AvailabilityTimeout_t& lhs, const AvailabilityTimeout_t& rhs) {
                        return std::get<0>(lhs) < std::get<0>(rhs);
            });
            minTimeout = std::get<0>(*minTimeoutElement);
            std::chrono::steady_clock::time_point now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            timeout = (int)std::chrono::duration_cast<std::chrono::milliseconds>(minTimeout - now).count();
        }
        timeoutsMutex_.unlock();

        //wait for timeout or notification
        if (!firstIteration && std::cv_status::timeout ==
                    availabilityTimeoutCondition_.wait_for(threadLock, std::chrono::milliseconds(timeout))) {

            timeoutsMutex_.lock();

            //iterate through timeouts
            auto it = timeouts_.begin();
            while (it != timeouts_.end()) {
                std::chrono::steady_clock::time_point now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();

                isAvailableAsyncCallback callback = std::get<1>(*it);

                if (now > std::get<0>(*it)) {
                    //timeout
                    std::chrono::steady_clock::time_point timepoint_;
                    if(isAvailable()) {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::AVAILABLE,
                                                            timepoint_));
                    } else {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::NOT_AVAILABLE,
                                                            timepoint_));
                    }
                    it = timeouts_.erase(it);
                    availabilityMutex_.unlock();
                } else {
                    //timeout not expired
                    if(isAvailable()) {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::AVAILABLE,
                                                            minTimeout));
                        it = timeouts_.erase(it);
                        availabilityMutex_.unlock();
                    } else {
                        ++it;
                    }
                }
            }

            timeoutsMutex_.unlock();
        } else {

            if(firstIteration) {
                firstIteration = false;
                continue;
            }

            //timeout not expired
            timeoutsMutex_.lock();
            auto it = timeouts_.begin();
            while (it != timeouts_.end()) {
                isAvailableAsyncCallback callback = std::get<1>(*it);

                if(isAvailable()) {
                    availabilityMutex_.lock();
                    callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                        AvailabilityStatus::AVAILABLE,
                                                        minTimeout));
                    it = timeouts_.erase(it);
                    availabilityMutex_.unlock();
                } else {
                    ++it;
                }
            }

            timeoutsMutex_.unlock();
        }

        //do callbacks
        isAvailableAsyncCallback callback;
        AvailabilityStatus avStatus;
        int remainingTimeout;
        std::chrono::steady_clock::time_point now;

        auto it = callbacks.begin();
        while(it != callbacks.end()) {
            callback = std::get<0>(*it);
            avStatus = std::get<2>(*it);

            // compute remaining timeout
            now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            remainingTimeout = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::get<3>(*it) - now).count();
            if(remainingTimeout < 0)
                remainingTimeout = 0;

            threadLock.unlock();

            std::get<1>(*it).set_value(avStatus);
            callback(avStatus, remainingTimeout);

            threadLock.lock();

            it = callbacks.erase(it);
        }

        //cancel thread
        timeoutsMutex_.lock();
        if(timeouts_.size() == 0 && callbacks.size() == 0)
            cancel = true;
        timeoutsMutex_.unlock();
    }
}

void Proxy::notifySpecificListener(std::weak_ptr<Proxy> _proxy,
                                   const ProxyStatusEvent::Listener &_listener,
                                   const ProxyStatusEvent::Subscription _subscription) {
    if(auto itsProxy = _proxy.lock()) {
        std::lock_guard<std::recursive_mutex> listenersLock(itsProxy->proxyStatusEvent_.listenersMutex_);
        AvailabilityStatus itsStatus = AvailabilityStatus::UNKNOWN;
        {
            std::lock_guard<std::mutex> itsLock(itsProxy->availabilityMutex_);
            itsStatus = itsProxy->availabilityStatus_;
        }
        if (itsStatus != AvailabilityStatus::UNKNOWN)
            itsProxy->proxyStatusEvent_.notifySpecificListener(_subscription, itsStatus);

        //add listener to list so that it can be notified about a change of availability
        itsProxy->proxyStatusEvent_.listeners_.push_back(std::make_pair(_subscription, _listener));
    }
}

void Proxy::onServiceInstanceStatus(std::shared_ptr<Proxy> _proxy,
                                    service_id_t serviceId,
                                    instance_id_t instanceId,
                                    bool isAvailable,
                                    void* _data) {
    (void)_proxy;
    (void)_data;
    bool queueSelective(false);
    {
        std::lock_guard<std::recursive_mutex> listenersLock(proxyStatusEvent_.listenersMutex_);
        {
            std::lock_guard<std::mutex> itsLock(availabilityMutex_);
            if (availabilityStatus_ == AvailabilityStatus::AVAILABLE && !isAvailable) {
                // Only queue selective error handlers for implicitly re-subscribing!
                queueSelective = true;
            }
            const AvailabilityStatus itsStatus(
                    isAvailable ? AvailabilityStatus::AVAILABLE :
                            AvailabilityStatus::NOT_AVAILABLE);

            if (availabilityStatus_ == itsStatus) {
                return;
            }
            availabilityStatus_ = itsStatus;
        }
        availabilityTimeoutThreadMutex_.lock();
        //notify availability thread that proxy status has changed
        availabilityTimeoutCondition_.notify_all();
        availabilityTimeoutThreadMutex_.unlock();

        if (queueSelective)
            getConnection()->queueSelectiveErrorHandler(serviceId, instanceId);

        for(auto listenerIt : proxyStatusEvent_.listeners_)
            proxyStatusEvent_.notifySpecificListener(listenerIt.first, availabilityStatus_);
    }
    _proxy->availabilityCondition_.notify_one();
}

Proxy::Proxy(const Address &_address,
             const std::shared_ptr<ProxyConnection> &connection,
             bool hasSelective) :
        ProxyBase(connection),
        address_(_address),
        proxyStatusEvent_(this),
        availabilityStatus_(AvailabilityStatus::UNKNOWN),
        availabilityHandlerId_(0),
        interfaceVersionAttribute_(*this, 0x0, true, false),
        hasSelectiveEvents_(hasSelective) {
}

Proxy::~Proxy() {
    if(availabilityTimeoutThread_) {
        if(availabilityTimeoutThread_->joinable())
            availabilityTimeoutThread_->join();
    }
    getConnection()->releaseService(address_);
    getConnection()->unregisterAvailabilityHandler(address_, availabilityHandlerId_);
    Factory::get()->decrementConnection(getConnection());
}

bool Proxy::init() {
    std::shared_ptr<ProxyConnection> connection = getConnection();
    if (!connection)
        return false;

    connection->requestService(address_, hasSelectiveEvents_);

    std::weak_ptr<Proxy> itsProxy = shared_from_this();
    availabilityHandlerId_ = connection->registerAvailabilityHandler(
                                    address_,
                                    std::bind(&Proxy::onServiceInstanceStatus,
                                              this,
                                              std::placeholders::_1,
                                              std::placeholders::_2,
                                              std::placeholders::_3,
                                              std::placeholders::_4,
                                              std::placeholders::_5),
                                    itsProxy,
                                    NULL);
    if (connection->isAvailable(address_)) {
        std::lock_guard<std::mutex> itsLock(availabilityMutex_);
        availabilityStatus_ = AvailabilityStatus::AVAILABLE;
    }

    return true;
}

const Address &
Proxy::getSomeIpAddress() const {
    return address_;
}

bool Proxy::isAvailable() const {
    std::lock_guard<std::mutex> itsLock(availabilityMutex_);
    return (getConnection()->isConnected()
            && availabilityStatus_ == AvailabilityStatus::AVAILABLE);
}

bool Proxy::isAvailableBlocking() const {
    std::shared_ptr<ProxyConnection> connection = getConnection();
    if (connection)
        connection->waitUntilConnected();

    std::unique_lock < std::mutex > itsLock(availabilityMutex_);
    while (availabilityStatus_ != AvailabilityStatus::AVAILABLE) {
        availabilityCondition_.wait(itsLock);
    }

    return true;
}

std::future<AvailabilityStatus> Proxy::isAvailableAsync(
            isAvailableAsyncCallback _callback,
            const CommonAPI::CallInfo *_info) const {

    std::promise<AvailabilityStatus> promise;
    std::future<AvailabilityStatus> future = promise.get_future();

    //set timeout point
    auto timeoutPoint = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now() + std::chrono::milliseconds(_info->timeout_);

    timeoutsMutex_.lock();
    if(timeouts_.size() == 0) {
        //no timeouts

        bool isAvailabilityTimeoutThread = false;

        //join running availability thread
        if(availabilityTimeoutThread_) {

            //check if current thread is availability timeout thread
            isAvailabilityTimeoutThread = (std::this_thread::get_id() ==
                            availabilityTimeoutThread_.get()->get_id());

            if(availabilityTimeoutThread_->joinable() && !isAvailabilityTimeoutThread) {
                timeoutsMutex_.unlock();
                availabilityTimeoutThread_->join();
                timeoutsMutex_.lock();
            }
        }
        //add new timeout
        timeouts_.push_back(std::make_tuple(timeoutPoint, _callback, std::move(promise)));

        //start availability thread
        if(!isAvailabilityTimeoutThread)
            availabilityTimeoutThread_ = std::make_shared<std::thread>(
                    std::bind(&Proxy::availabilityTimeoutThreadHandler, this));
    } else {
        //add timeout
        timeouts_.push_back(std::make_tuple(timeoutPoint, _callback, std::move(promise)));
    }
    timeoutsMutex_.unlock();

    availabilityTimeoutThreadMutex_.lock();
    //notify availability thread that new timeout was added
    availabilityTimeoutCondition_.notify_all();
    availabilityTimeoutThreadMutex_.unlock();

    return future;
}

AvailabilityStatus Proxy::getAvailabilityStatus() const {
    std::lock_guard<std::mutex> itsLock(availabilityMutex_);
    return availabilityStatus_;
}

ProxyStatusEvent& Proxy::getProxyStatusEvent() {
    return proxyStatusEvent_;
}

InterfaceVersionAttribute& Proxy::getInterfaceVersionAttribute() {
    return interfaceVersionAttribute_;
}

void Proxy::getInitialEvent(service_id_t serviceId, instance_id_t instanceId,
                            eventgroup_id_t eventGroupId, event_id_t eventId,
                            major_version_t major) {
    getConnection()->subscribeForField(serviceId, instanceId, eventGroupId,
            eventId, major);
}

} // namespace SomeIP
} // namespace CommonAPI
