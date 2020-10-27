// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_STUBMANAGER_HPP_
#define COMMONAPI_SOMEIP_STUBMANAGER_HPP_

#include <map>

#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/StubAdapter.hpp>

namespace CommonAPI {
namespace SomeIP {

class StubManager {
 public:
    StubManager(const std::shared_ptr<ProxyConnection>& connection);
    virtual ~StubManager();

    void registerStubAdapter(std::shared_ptr<StubAdapter> stubAdapter);
    void unregisterStubAdapter(std::shared_ptr<StubAdapter> stubAdapter);

    bool handleMessage(const Message&);

 private:
    std::weak_ptr<ProxyConnection> connection_;
    std::map<service_id_t, std::map<instance_id_t, std::shared_ptr<StubAdapter>>> registeredStubAdapters_;
    std::mutex registeredStubAdaptersMutex_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUBMANAGER_HPP_
