// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/Utils.hpp>
#include <CommonAPI/SomeIP/StubAdapter.hpp>

namespace CommonAPI {
namespace SomeIP {

StubAdapter::StubAdapter(const Address &_someipAddress,
                		 const std::shared_ptr<ProxyConnection> &_connection)
    : someipAddress_(_someipAddress), connection_(_connection) {
}

StubAdapter::~StubAdapter() {
    deinit();
}

void
StubAdapter::init(std::shared_ptr< StubAdapter > instance) {
}

void
StubAdapter::deinit() {
}

const Address &
StubAdapter::getSomeIpAddress() const {
	return someipAddress_;
}

const bool
StubAdapter::isManagingInterface() {
    return false;
}

const std::shared_ptr<ProxyConnection> &
StubAdapter::getConnection() const {
    return connection_;
}

bool
StubAdapter::onInterfaceMessage(const Message &message) {
    return true;
}

} // namespace SomeIP
} // namespace CommonAPI
