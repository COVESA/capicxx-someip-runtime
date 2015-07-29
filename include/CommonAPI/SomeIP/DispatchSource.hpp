// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef DISPATCHSOURCE_HPP_
#define DISPATCHSOURCE_HPP_

#include <memory>
#include "CommonAPI/MainLoopContext.hpp"

namespace CommonAPI {
namespace SomeIP {

class Watch;

class DispatchSource: public CommonAPI::DispatchSource {
 public:
    DispatchSource(const std::shared_ptr<Watch>& watch);
    virtual ~DispatchSource();

    bool prepare(int64_t& timeout);
    bool check();
    bool dispatch();

 private:
    std::shared_ptr<Watch> watch_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif /* DISPATCHSOURCE_HPP_ */
