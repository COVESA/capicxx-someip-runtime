// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/DispatchSource.hpp>

#include <CommonAPI/SomeIP/Watch.hpp>
#include <iostream>
#include <thread>

namespace CommonAPI {
namespace SomeIP {

DispatchSource::DispatchSource(const std::shared_ptr<Watch>& watch) :
    watch_(watch) {
    watch_->addDependentDispatchSource(this);
}

DispatchSource::~DispatchSource() {
    std::unique_lock<std::mutex> itsLock(watchMutex_);
    watch_->removeDependentDispatchSource(this);
}

bool DispatchSource::prepare(int64_t& timeout) {
    std::unique_lock<std::mutex> itsLock(watchMutex_);
    timeout = -1;
    return !watch_->emptyQueue();
}

bool DispatchSource::check() {
    std::unique_lock<std::mutex> itsLock(watchMutex_);
    return !watch_->emptyQueue();
}

bool DispatchSource::dispatch() {
    std::unique_lock<std::mutex> itsLock(watchMutex_);
    if (!watch_->emptyQueue()) {
        auto msgQueueEntry = watch_->frontQueue();
        watch_->popQueue();
        watch_->processMsgQueueEntry(msgQueueEntry);
    }

    return !watch_->emptyQueue();
}

} // namespace SomeIP
} // namespace CommonAPI
