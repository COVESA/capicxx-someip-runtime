// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_SOMEIP_CONSTANTS_HPP_
#define COMMONAPI_SOMEIP_CONSTANTS_HPP_

#include <cstdint>
#include <string>

#include <vsomeip/vsomeip.hpp>

#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

const major_version_t DEFAULT_MAJOR_VERSION = vsomeip::DEFAULT_MAJOR;
const minor_version_t DEFAULT_MINOR_VERSION = vsomeip::DEFAULT_MINOR;

const CommonAPI::Timeout_t DEFAULT_SEND_TIMEOUT_MS = 5000;
const ms_t ASYNC_MESSAGE_REPLY_TIMEOUT_MS = 5000;
const ms_t ASYNC_MESSAGE_CLEANUP_INTERVAL_MS = 1000;

static const CommonAPI::CallInfo defaultCallInfo(DEFAULT_SEND_TIMEOUT_MS);

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CONSTANTS_HPP_
