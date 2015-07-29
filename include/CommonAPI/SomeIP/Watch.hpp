// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef WATCH_HPP_
#define WATCH_HPP_

#include <memory>
#include <queue>
#include <mutex>

#include <vsomeip/application.hpp>

#include <CommonAPI/MainLoopContext.hpp>

namespace CommonAPI {
namespace SomeIP {

class Connection;

class Watch : public CommonAPI::Watch {
 public:
    enum class commDirectionType : uint8_t {
    	PROXYRECEIVE = 0x00,
        STUBRECEIVE = 0x01,
    };
    typedef std::pair<std::shared_ptr<vsomeip::message>, commDirectionType> msgQueueEntry;

	Watch(const std::shared_ptr<Connection>& _connection);

	virtual ~Watch();

	void dispatch(unsigned int eventFlags);

	const pollfd& getAssociatedFileDescriptor();

#ifdef WIN32
	const HANDLE& getAssociatedEvent();
#endif

	const std::vector<CommonAPI::DispatchSource*>& getDependentDispatchSources();

	void addDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource);

	void removeDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource);

	void pushQueue(msgQueueEntry _msgQueueEntry);

	void popQueue();

	msgQueueEntry& frontQueue();

	bool emptyQueue();

	void processMsgQueueEntry(msgQueueEntry &_msgQueueEntry);

private:
	const int pipeValue_;
	int pipeFileDescriptors_[2];

	pollfd pollFileDescriptor_;
	std::vector<CommonAPI::DispatchSource*> dependentDispatchSources_;
    std::queue<msgQueueEntry> msgQueue_;

    std::mutex msgQueueMutex_;

    std::shared_ptr<Connection> connection_;

#ifdef WIN32
	HANDLE wsaEvent_;
	OVERLAPPED ov;
#endif
};

} // namespace IntraP
} // namespace CommonAPI

#endif /* WATCH_HPP_ */
