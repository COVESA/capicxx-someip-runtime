// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include <CommonAPI/SomeIP/AddressTranslator.hpp>

namespace CommonAPI {
namespace SomeIP {

static std::weak_ptr<Factory> factory__(Factory::get());

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

    bool finish = false;
    bool firstIteration = true;

    // the callbacks that have to be done are stored with
    // their required data in a list of tuples.
    typedef std::tuple<
            isAvailableAsyncCallback,
            AvailabilityStatus,
            std::chrono::steady_clock::time_point,
            std::list<AvailabilityTimeout_t>::iterator
            > CallbackData_t;
    std::list<CallbackData_t> callbacks;

    while(!finish) {

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
                        callbacks.push_back(std::make_tuple(callback,
                                                            AvailabilityStatus::AVAILABLE,
                                                            timepoint_, it));
                    } else {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback,
                                                            AvailabilityStatus::NOT_AVAILABLE,
                                                            timepoint_, it));
                    }
                    ++it;
                    availabilityMutex_.unlock();
                } else {
                    //timeout not expired
                    if(isAvailable()) {
                        availabilityMutex_.lock();
                        callbacks.push_back(std::make_tuple(callback,
                                                            AvailabilityStatus::AVAILABLE,
                                                            minTimeout, it));
                        ++it;
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
                if (!isAvailable()) {
                    continue;
                }
            }

            //timeout not expired
            timeoutsMutex_.lock();
            auto it = timeouts_.begin();
            while (it != timeouts_.end()) {
                isAvailableAsyncCallback callback = std::get<1>(*it);

                if(isAvailable()) {
                    availabilityMutex_.lock();
                    callbacks.push_back(std::make_tuple(callback,
                                                        AvailabilityStatus::AVAILABLE,
                                                        minTimeout, it));
                    ++it;
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
            avStatus = std::get<1>(*it);

            // compute remaining timeout
            now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            remainingTimeout = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::get<2>(*it) - now).count();
            if(remainingTimeout < 0)
                remainingTimeout = 0;

            threadLock.unlock();

            callback(avStatus, remainingTimeout);

            threadLock.lock();

            ++it;
        }

        //cancel thread
        timeoutsMutex_.lock();
        for (const auto& cb : callbacks) {
            timeouts_.erase(std::get<3>(cb));
        }
        callbacks.clear();
        if(timeouts_.size() == 0 && callbacks.size() == 0)
            finish = true;
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
    (void) serviceId;
    (void) instanceId;
    {
        std::lock_guard<std::recursive_mutex> listenersLock(proxyStatusEvent_.listenersMutex_);
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

        for(auto listenerIt : proxyStatusEvent_.listeners_)
            proxyStatusEvent_.notifySpecificListener(listenerIt.first, availabilityStatus_);
    }
    {
        std::lock_guard<std::mutex> itsLock(_proxy->availabilityMutex_);
        _proxy->availabilityCondition_.notify_one();
    }
}

Proxy::Proxy(const Address &_address,
             const std::shared_ptr<ProxyConnection> &connection) :
        ProxyBase(connection),
        address_(_address),
        alias_(AddressTranslator::get()->getAddressAlias(_address)),
        proxyStatusEvent_(this),
        availabilityStatus_(AvailabilityStatus::UNKNOWN),
        availabilityHandlerId_(0),
        interfaceVersionAttribute_(*this, 0x0, true, false) {
}

Proxy::~Proxy() {
    {
        std::lock_guard<std::mutex> itsLock(timeoutsMutex_);
        timeouts_.clear();
    }
    {
        std::lock_guard<std::mutex> itsTimeoutThreadLock(availabilityTimeoutThreadMutex_);
        availabilityTimeoutCondition_.notify_all();
    }
    if(availabilityTimeoutThread_) {
        if (availabilityTimeoutThread_->get_id() == std::this_thread::get_id()) {
            availabilityTimeoutThread_->detach();
        } else if(availabilityTimeoutThread_->joinable()) {
            availabilityTimeoutThread_->join();
        }
    }
    getConnection()->releaseService(alias_);
    getConnection()->unregisterAvailabilityHandler(alias_, availabilityHandlerId_);
    if (auto ptr = factory__.lock()) {
        ptr->decrementConnection(getConnection());
    }
}

bool Proxy::init() {
    std::shared_ptr<ProxyConnection> connection = getConnection();
    if (!connection)
        return false;

    connection->requestService(alias_);

    std::weak_ptr<Proxy> itsProxy = shared_from_this();
    availabilityHandlerId_ = connection->registerAvailabilityHandler(
                                    alias_,
                                    std::bind(&Proxy::onServiceInstanceStatus,
                                              this,
                                              std::placeholders::_1,
                                              std::placeholders::_2,
                                              std::placeholders::_3,
                                              std::placeholders::_4,
                                              std::placeholders::_5),
                                    itsProxy,
                                    NULL);
    if (connection->isAvailable(alias_)) {
        std::lock_guard<std::mutex> itsLock(availabilityMutex_);
        availabilityStatus_ = AvailabilityStatus::AVAILABLE;
    }

    return true;
}

const Address &
Proxy::getSomeIpAddress() const {
    return address_;
}

const Address &
Proxy::getSomeIpAlias() const {
    return alias_;
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
        timeouts_.push_back(std::make_tuple(timeoutPoint, _callback, std::promise<AvailabilityStatus>()));

        //start availability thread
        if(!isAvailabilityTimeoutThread)
            availabilityTimeoutThread_ = std::make_shared<std::thread>(
                    std::bind(&Proxy::availabilityTimeoutThreadHandler,
                              shared_from_this()));
    } else {
        //add timeout
        timeouts_.push_back(std::make_tuple(timeoutPoint, _callback, std::promise<AvailabilityStatus>()));
    }
    timeoutsMutex_.unlock();

    availabilityTimeoutThreadMutex_.lock();
    //notify availability thread that new timeout was added
    availabilityTimeoutCondition_.notify_all();
    availabilityTimeoutThreadMutex_.unlock();

    return std::future<AvailabilityStatus>();
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

} // namespace SomeIP
} // namespace CommonAPI
