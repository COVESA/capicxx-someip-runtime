// Copyright (C) 2013-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_DEFINES_HPP_
#define COMMONAPI_SOMEIP_DEFINES_HPP_

namespace CommonAPI {
namespace SomeIP {

#ifndef COMMONAPI_SOMEIP_APPLICATION_NAME
#define COMMONAPI_SOMEIP_APPLICATION_NAME ""
#endif

#ifndef DEFAULT_MAX_PROCESSING_TIME
#define DEFAULT_MAX_PROCESSING_TIME 100
#endif

#ifndef DEFAULT_MAX_QUEUE_SIZE
#define DEFAULT_MAX_QUEUE_SIZE 25
#endif

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_DEFINES_HPP_
