// Copyright (C) 2013-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_CONSTANTS_HPP_
#define COMMONAPI_SOMEIP_CONSTANTS_HPP_

#include <cstdint>
#include <string>

#include <vsomeip/vsomeip.hpp>

#include <CommonAPI/CallInfo.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

const method_id_t ANY_METHOD = vsomeip::ANY_METHOD;
const major_version_t ANY_MAJOR_VERSION  = vsomeip::ANY_MAJOR;
const minor_version_t ANY_MINOR_VERSION  = vsomeip::ANY_MINOR;

const major_version_t DEFAULT_MAJOR_VERSION = vsomeip::DEFAULT_MAJOR;
const minor_version_t DEFAULT_MINOR_VERSION = vsomeip::DEFAULT_MINOR;

const service_id_t MIN_SERVICE_ID = 0x0001;
const service_id_t MAX_SERVICE_ID = 0xFFFD;

const instance_id_t MIN_INSTANCE_ID = 0x0001;
const instance_id_t MAX_INSTANCE_ID = 0xFFFE;

const ms_t ASYNC_MESSAGE_REPLY_TIMEOUT_MS = 5000;
const ms_t ASYNC_MESSAGE_CLEANUP_INTERVAL_MS = 1000;

const method_id_t  MIN_METHOD_ID = 0x0001;
const method_id_t  MAX_METHOD_ID = 0xFFFE;

const eventgroup_id_t MIN_EVENTGROUP_ID = 0x0001;
const eventgroup_id_t MAX_EVENTGROUP_ID = 0xFFFE;

static const CommonAPI::CallInfo defaultCallInfo(CommonAPI::DEFAULT_SEND_TIMEOUT_MS);

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CONSTANTS_HPP_
