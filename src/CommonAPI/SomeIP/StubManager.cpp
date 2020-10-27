// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
    if (!connection) {
        COMMONAPI_ERROR("StubManager::registerStubAdapter couldn't lock connection");
        return;
    }
    Address itsAddress = _adapter->getSomeIpAddress();
    service_id_t service = itsAddress.getService();
    instance_id_t instance = itsAddress.getInstance();

    {
        std::lock_guard<std::mutex> lock(registeredStubAdaptersMutex_);
        registeredStubAdapters_[service][instance] = _adapter;
    }
    connection->registerService(itsAddress);
}

void StubManager::unregisterStubAdapter(std::shared_ptr<StubAdapter> _adapter) {
    std::shared_ptr<ProxyConnection> connection = connection_.lock();
    if (!connection) {
        COMMONAPI_ERROR("StubManager::unregisterStubAdapter couldn't lock connection");
        return;
    }
    Address itsAddress = _adapter->getSomeIpAddress();
    service_id_t service = itsAddress.getService();
    instance_id_t instance = itsAddress.getInstance();

    bool erased(false);
    {
        std::lock_guard<std::mutex> lock(registeredStubAdaptersMutex_);
        auto foundService = registeredStubAdapters_.find(service);
        if(foundService != registeredStubAdapters_.end()) {
            auto foundInstance = foundService->second.find(instance);
            if (foundInstance != foundService->second.end()) {
                foundService->second.erase(instance);
                erased = true;
            }
        }
    }
    if (erased) {
        connection->unregisterService(itsAddress);
    }
}

bool StubManager::handleMessage(const Message &_message) {
    std::shared_ptr<StubAdapter> foundStubAdapter;
    {
        std::lock_guard<std::mutex> lock(registeredStubAdaptersMutex_);
        auto foundService = registeredStubAdapters_.find(_message.getServiceId());
        if(foundService != registeredStubAdapters_.end()) {
            auto foundInstance = foundService->second.find(_message.getInstanceId());
            if (foundInstance != foundService->second.end()) {
                foundStubAdapter = foundInstance->second;
            }
        }
    }

    if (foundStubAdapter)
        return foundStubAdapter->onInterfaceMessage(_message);

    return false;
}

} // namespace SomeIP
} // namespace CommonAPI
