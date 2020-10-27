// Copyright (C) 2013-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_TYPES_HPP_
#define COMMONAPI_SOMEIP_TYPES_HPP_

#include <cstdint>
#include <string>

#include <vsomeip/vsomeip.hpp>

namespace CommonAPI {
namespace SomeIP {

class Proxy;

typedef vsomeip::service_t service_id_t;
typedef vsomeip::method_t method_id_t;
typedef vsomeip::event_t event_id_t;
typedef vsomeip::instance_t instance_id_t;
typedef vsomeip::eventgroup_t eventgroup_id_t;
typedef vsomeip::session_t session_id_t;
typedef vsomeip::client_t client_id_t;

typedef vsomeip::byte_t byte_t;
typedef vsomeip::length_t message_length_t;

typedef vsomeip::return_code_e return_code_e;
typedef vsomeip::message_type_e message_type_e;
typedef vsomeip::state_type_e state_type_e;
typedef vsomeip::event_type_e event_type_e;
typedef vsomeip::reliability_type_e reliability_type_e;

typedef vsomeip::major_version_t major_version_t;
typedef vsomeip::minor_version_t minor_version_t;

typedef int64_t ms_t;

typedef vsomeip::uid_t uid_t;
typedef vsomeip::gid_t gid_t;

typedef uint32_t AvailabilityHandlerId_t;
typedef std::function<void (std::shared_ptr<Proxy>, service_id_t, instance_id_t, bool, void*)> AvailabilityHandler_t;
typedef std::function<bool (client_id_t, uid_t, gid_t, bool) > SubscriptionHandler_t;
typedef std::function<void (const bool)> SubscriptionAcceptedHandler_t;
typedef std::function<void (client_id_t, uid_t, gid_t, bool, const SubscriptionAcceptedHandler_t&) > AsyncSubscriptionHandler_t;

typedef std::uint32_t session_id_fake_t;

class Message;
typedef std::function<bool (const Message &) > MessageHandler_t;

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_TYPES_HPP_
