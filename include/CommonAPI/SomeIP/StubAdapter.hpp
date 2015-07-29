// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_STUB_ADAPTER_HPP_
#define COMMONAPI_SOMEIP_STUB_ADAPTER_HPP_

#include <memory>
#include <string>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Stub.hpp>
#include <CommonAPI/SomeIP/Address.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/InterfaceHandler.hpp>
#include <CommonAPI/SomeIP/Message.hpp>

namespace CommonAPI {
namespace SomeIP {

class ObjectManagerStub;
class Factory;

class StubAdapter: virtual public CommonAPI::StubAdapter, public InterfaceHandler {
 public:
	 COMMONAPI_EXPORT StubAdapter(const Address &_address,
                const std::shared_ptr<ProxyConnection> &_connection);

	 COMMONAPI_EXPORT virtual ~StubAdapter();

	 COMMONAPI_EXPORT virtual void init(std::shared_ptr<StubAdapter> instance);
	 COMMONAPI_EXPORT virtual void deinit();

	 COMMONAPI_EXPORT const Address &getSomeIpAddress() const;
	 COMMONAPI_EXPORT const std::shared_ptr< ProxyConnection > & getConnection() const;

	 COMMONAPI_EXPORT const bool isManagingInterface();

	 COMMONAPI_EXPORT virtual bool onInterfaceMessage(const Message &message) = 0;

protected:
    Address someipAddress_;
    const std::shared_ptr<ProxyConnection> connection_;
};


} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUB_ADAPTER_HPP_
