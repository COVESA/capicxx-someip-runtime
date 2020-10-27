// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/Utils.hpp>
#include <CommonAPI/SomeIP/StubAdapter.hpp>
#include <CommonAPI/SomeIP/Factory.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>

namespace CommonAPI {
namespace SomeIP {

StubAdapter::StubAdapter(const Address &_someipAddress,
                         const std::shared_ptr<ProxyConnection> &_connection)
    : someipAddress_(_someipAddress), connection_(_connection) {
}

StubAdapter::~StubAdapter() {
    Factory::get()->unregisterStub(address_.getDomain(), address_.getInterface(), address_.getInstance());
}

void
StubAdapter::init(std::shared_ptr<StubAdapter> instance) {
    (void) instance;
    AddressTranslator::get()->translate(someipAddress_, address_);
}

void
StubAdapter::deinit() {
}

const Address &
StubAdapter::getSomeIpAddress() const {
    return someipAddress_;
}

bool
StubAdapter::isManagingInterface() {
    return false;
}

const std::shared_ptr<ProxyConnection> &
StubAdapter::getConnection() const {
    return connection_;
}

bool
StubAdapter::onInterfaceMessage(const Message &) {
    return true;
}

void
StubAdapter::registerEvent(event_id_t _event, const std::set<eventgroup_id_t> &_eventGroups,
        event_type_e _type, reliability_type_e _reliability) {
    connection_->registerEvent(
            someipAddress_.getService(), someipAddress_.getInstance(),
            _event, _eventGroups, _type, _reliability);
}

void
StubAdapter::unregisterEvent(event_id_t _event) {
    connection_->unregisterEvent(
            someipAddress_.getService(), someipAddress_.getInstance(),
            _event);
}


} // namespace SomeIP
} // namespace CommonAPI
