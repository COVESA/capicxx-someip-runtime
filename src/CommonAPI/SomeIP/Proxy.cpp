// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/Utils.hpp>
#include <CommonAPI/SomeIP/Proxy.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>

namespace CommonAPI {
namespace SomeIP {

ProxyStatusEventHelper::ProxyStatusEventHelper(Proxy* proxy) :
        proxy_(proxy) {
}

void ProxyStatusEventHelper::onListenerAdded(const Listener& listener) {
    if (proxy_->isAvailable()) {
        listener(AvailabilityStatus::AVAILABLE);
    }
}

void Proxy::onServiceInstanceStatus(service_id_t serviceId,
        instance_id_t instanceId, bool isAvailable) {
    availabilityStatus_ =
            isAvailable ?
                    AvailabilityStatus::AVAILABLE :
                    AvailabilityStatus::NOT_AVAILABLE;

    if (isAvailable) {
        getConnection()->sendPendingSubscriptions(serviceId, instanceId);
    }

    proxyStatusEvent_.notifyListeners(availabilityStatus_);
    availabilityCondition_.notify_one();
}

Proxy::Proxy(const Address &_address,
        const std::shared_ptr<ProxyConnection> &connection, bool hasSelective) :
        ProxyBase(connection), address_(_address), proxyStatusEvent_(this), availabilityStatus_(
                AvailabilityStatus::UNKNOWN), interfaceVersionAttribute_(*this,
                0x0, true), hasSelectiveEvents_(hasSelective) {

}

Proxy::~Proxy() {
    getConnection()->unregisterAvailabilityHandler(address_, availabilityHandlerId_);
}

void Proxy::init() {
    std::function<void(service_id_t, instance_id_t, bool)> availabilityHandler =
            std::bind(&Proxy::onServiceInstanceStatus, this,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3);

    std::shared_ptr<ProxyConnection> connection = getConnection();
    connection->requestService(address_, hasSelectiveEvents_);
    availabilityHandlerId_ = connection->registerAvailabilityHandler(
                                    address_, availabilityHandler);
    if (connection->isAvailable(address_)) {
        availabilityStatus_ = AvailabilityStatus::AVAILABLE;
    }
}

const Address &
Proxy::getSomeIpAddress() const {
    return address_;
}

bool Proxy::isAvailable() const {
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

ProxyStatusEvent& Proxy::getProxyStatusEvent() {
    return proxyStatusEvent_;
}

InterfaceVersionAttribute& Proxy::getInterfaceVersionAttribute() {
    return interfaceVersionAttribute_;
}

} // namespace SomeIP
} // namespace CommonAPI
