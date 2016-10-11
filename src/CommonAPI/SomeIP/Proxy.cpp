// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

void ProxyStatusEventHelper::onListenerAdded(const Listener& listener, const Subscription subscription) {
    (void)listener;
    std::function<void(uint32_t)> notifySpecificListenerHandler =
                std::bind(&ProxyStatusEventHelper::onNotifySpecificListener, this,
                        std::placeholders::_1);
    proxy_->getConnection()->proxyPushFunction(notifySpecificListenerHandler, subscription);
}

void ProxyStatusEventHelper::onNotifySpecificListener(uint32_t subscription) {
    AvailabilityStatus itsStatus = proxy_->getAvailabilityStatus();
    if (itsStatus != AvailabilityStatus::UNKNOWN)
        notifySpecificListener(subscription, itsStatus);
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
            std::chrono::time_point<std::chrono::high_resolution_clock>
            > CallbackData_t;
    std::list<CallbackData_t> callbacks;

    while(!cancel) {

        //get min timeout
        timeoutsMutex_.lock();

        int timeout = std::numeric_limits<int>::max();
        std::chrono::time_point<std::chrono::high_resolution_clock> minTimeout;
        if (timeouts_.size() > 0) {
            auto minTimeoutElement = std::min_element(timeouts_.begin(), timeouts_.end(),
                    [] (const AvailabilityTimeout_t& lhs, const AvailabilityTimeout_t& rhs) {
                        return std::get<0>(lhs) < std::get<0>(rhs);
            });
            minTimeout = std::get<0>(*minTimeoutElement);
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
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
                std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

                isAvailableAsyncCallback callback = std::get<1>(*it);

                if (now > std::get<0>(*it)) {
                    //timeout
                    if(isAvailable()) {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::AVAILABLE,
                                                            std::chrono::time_point<std::chrono::high_resolution_clock>()));
                    } else {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::NOT_AVAILABLE,
                                                            std::chrono::time_point<std::chrono::high_resolution_clock>()));
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
        std::chrono::high_resolution_clock::time_point now;

        auto it = callbacks.begin();
        while(it != callbacks.end()) {
            callback = std::get<0>(*it);
            avStatus = std::get<2>(*it);

            // compute remaining timeout
            now = std::chrono::high_resolution_clock::now();
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

void Proxy::onServiceInstanceStatus(service_id_t serviceId,
        instance_id_t instanceId, bool isAvailable) {
    {
        std::lock_guard<std::mutex> itsLock(availabilityMutex_);
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

    if (!isAvailable) {
        getConnection()->queueSelectiveErrorHandler(serviceId, instanceId);
    }

    proxyStatusEvent_.notifyListeners(availabilityStatus_);
    availabilityCondition_.notify_one();
}

Proxy::Proxy(const Address &_address,
        const std::shared_ptr<ProxyConnection> &connection, bool hasSelective) :
        ProxyBase(connection), address_(_address), proxyStatusEvent_(this), availabilityStatus_(
                AvailabilityStatus::UNKNOWN), interfaceVersionAttribute_(*this,
                0x0, true, false), hasSelectiveEvents_(hasSelective) {
    Factory::get()->incrementConnection(getConnection());
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
    std::function<void(service_id_t, instance_id_t, bool)> availabilityHandler =
            std::bind(&Proxy::onServiceInstanceStatus, this,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3);

    std::shared_ptr<ProxyConnection> connection = getConnection();
    if (!connection)
        return false;

    connection->requestService(address_, hasSelectiveEvents_);
    availabilityHandlerId_ = connection->registerAvailabilityHandler(
                                    address_, availabilityHandler);
    if (connection->isAvailable(address_)) {
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
    auto timeoutPoint = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(_info->timeout_);

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
    getConnection()->getInitialEvent(serviceId, instanceId, eventGroupId,
            eventId, major);
}

} // namespace SomeIP
} // namespace CommonAPI
