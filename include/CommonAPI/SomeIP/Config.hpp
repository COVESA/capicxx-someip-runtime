// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_CONFIG_HPP_
#define COMMONAPI_SOMEIP_CONFIG_HPP_

#include <vsomeip/constants.hpp>

#define SOMEIP_ANY_METHOD       vsomeip::ANY_METHOD
#define SOMEIP_ANY_MINOR_VERSION vsomeip::ANY_MINOR

#define SOMEIP_MIN_SERVICE_ID   0x0001
#define SOMEIP_MAX_SERVICE_ID   0xFFFD

#define SOMEIP_MIN_INSTANCE_ID  0x0001
#define SOMEIP_MAX_INSTANCE_ID  0xFFFE

#endif // COMMONAPI_SOMEIP_CONFIG_HPP_
