// Copyright (C) 2015-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_WATCH_HPP_
#define COMMONAPI_SOMEIP_WATCH_HPP_

#include <memory>
#include <queue>
#include <mutex>
#include <functional>

#include <vsomeip/application.hpp>

#include <CommonAPI/MainLoopContext.hpp>
#include <CommonAPI/SomeIP/Types.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>

namespace CommonAPI {
namespace SomeIP {

class Connection;
struct QueueEntry;

class Watch : public CommonAPI::Watch {
 public:

    Watch(const std::shared_ptr<Connection>& _connection);

    virtual ~Watch();

    void dispatch(unsigned int eventFlags);

    const pollfd& getAssociatedFileDescriptor();

#ifdef _WIN32
    const HANDLE& getAssociatedEvent();
#endif

    const std::vector<CommonAPI::DispatchSource*>& getDependentDispatchSources();

    void addDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource);

    void removeDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource);

    void pushQueue(std::shared_ptr<QueueEntry> _queueEntry);

    void popQueue();

    std::shared_ptr<QueueEntry> frontQueue();

    bool emptyQueue();

    void processQueueEntry(std::shared_ptr<QueueEntry> _queueEntry);

private:
#ifdef _WIN32
    int pipeFileDescriptors_[2];
#else
    int eventFd_;
#endif

    pollfd pollFileDescriptor_;
    std::vector<CommonAPI::DispatchSource*> dependentDispatchSources_;
    std::queue<std::shared_ptr<QueueEntry>> queue_;

    std::mutex queueMutex_;
    std::mutex dependentDispatchSourcesMutex_;

    std::weak_ptr<Connection> connection_;

#ifdef _WIN32
    HANDLE wsaEvent_;
    const int pipeValue_;
#else
    const std::uint64_t eventFdValue_;

#endif
};

} // namespace IntraP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_WATCH_HPP_
