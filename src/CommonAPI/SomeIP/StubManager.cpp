// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/Address.hpp>
#include <CommonAPI/SomeIP/StubManager.hpp>

namespace CommonAPI {
namespace SomeIP {

StubManager::StubManager(const std::shared_ptr<ProxyConnection> &_connection)
      : connection_(_connection){
    if (!_connection->isStubMessageHandlerSet()) {
        _connection->setStubMessageHandler(
        	std::bind(&StubManager::handleMessage, this, std::placeholders::_1));
    }
}

StubManager::~StubManager() {
    std::shared_ptr<ProxyConnection> connection = connection_.lock();
}

void StubManager::registerStubAdapter(std::shared_ptr<StubAdapter> _adapter) {
    std::shared_ptr<ProxyConnection> connection = connection_.lock();
    Address itsAddress = _adapter->getSomeIpAddress();
    service_id_t service = itsAddress.getService();
    instance_id_t instance = itsAddress.getInstance();

    connection->registerService(itsAddress);
    registeredStubAdapters_[service][instance] = _adapter;
}

void StubManager::unregisterStubAdapter(std::shared_ptr<StubAdapter> _adapter) {
    std::shared_ptr<ProxyConnection> connection = connection_.lock();
    Address itsAddress = _adapter->getSomeIpAddress();
    service_id_t service = itsAddress.getService();
    instance_id_t instance = itsAddress.getInstance();

    auto foundService = registeredStubAdapters_.find(service);
    if(foundService != registeredStubAdapters_.end()) {
        auto foundInstance = foundService->second.find(instance);
        if (foundInstance != foundService->second.end()) {
            foundService->second.erase(instance);
            connection->unregisterService(itsAddress);
        }
    }
}

bool StubManager::handleMessage(const Message &_message) {
    auto foundService = registeredStubAdapters_.find(_message.getServiceId());
    if(foundService != registeredStubAdapters_.end()) {
        auto foundInstance = foundService->second.find(_message.getInstanceId());
        if (foundInstance != foundService->second.end()) {
            std::shared_ptr<StubAdapter> foundStubAdapter = foundInstance->second;
            return foundStubAdapter->onInterfaceMessage(_message);
        }
    }
    return false;
}

} // namespace SomeIP
} // namespace CommonAPI
