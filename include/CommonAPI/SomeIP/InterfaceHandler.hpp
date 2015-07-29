// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_SOMEIP_INTERFACE_HANDLER_HPP_
#define COMMONAPI_SOMEIP_INTERFACE_HANDLER_HPP_

#include <memory>

#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/Message.hpp>

namespace CommonAPI {
namespace SomeIP {

class InterfaceHandler {
 public:
    virtual ~InterfaceHandler() { }

    virtual bool onInterfaceMessage(const Message& message) = 0;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_INTERFACE_HANDLER_HPP_
