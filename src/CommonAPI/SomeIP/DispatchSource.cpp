// Copyright (C) 2015-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/DispatchSource.hpp>

#include <CommonAPI/SomeIP/Watch.hpp>
#include <iostream>
#include <thread>

namespace CommonAPI {
namespace SomeIP {

DispatchSource::DispatchSource(Watch* watch) :
    watch_(watch) {
    watch_->addDependentDispatchSource(this);
}

DispatchSource::~DispatchSource() {
    watch_->removeDependentDispatchSource(this);
}

bool DispatchSource::prepare(int64_t& timeout) {
    timeout = -1;
    return !watch_->emptyQueue();
}

bool DispatchSource::check() {
    return !watch_->emptyQueue();
}

bool DispatchSource::dispatch() {
    if (!watch_->emptyQueue()) {
        auto queueEntry = watch_->frontQueue();
        watch_->popQueue();
        watch_->processQueueEntry(queueEntry);
    }

    return !watch_->emptyQueue();
}

} // namespace SomeIP
} // namespace CommonAPI
