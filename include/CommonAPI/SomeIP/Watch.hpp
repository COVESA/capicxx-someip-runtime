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
#include <functional>

#include <vsomeip/application.hpp>

#include <CommonAPI/MainLoopContext.hpp>
#include <CommonAPI/SomeIP/Types.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>

namespace CommonAPI {
namespace SomeIP {

class Connection;

class Watch : public CommonAPI::Watch {
 public:

    enum class commDirectionType : uint8_t {
        PROXYRECEIVE = 0x00,
        STUBRECEIVE = 0x01,
    };

    struct QueueEntry {

        QueueEntry() { }
        QueueEntry(Watch* _watch) :
            watch_(_watch) { }

        virtual void process() = 0;

        Watch* watch_;
    };

    struct MsgQueueEntry : QueueEntry {

        MsgQueueEntry(Watch* _watch,
                      std::shared_ptr<vsomeip::message> _message,
                      commDirectionType _directionType) :
                          QueueEntry(_watch),
                          message_(_message),
                          directionType_(_directionType) { }

        std::shared_ptr<vsomeip::message> message_;
        commDirectionType directionType_;

        void process();
    };

    struct AvblQueueEntry : QueueEntry {

        AvblQueueEntry(Watch* _watch,
                       service_id_t _service,
                       instance_id_t _instance,
                       bool _isAvailable) :
                           QueueEntry(_watch),
                           service_(_service),
                           instance_(_instance),
                           isAvailable_(_isAvailable) { }

        service_id_t service_;
        instance_id_t instance_;

        bool isAvailable_;

        void process();
    };

    struct ErrQueueEntry : QueueEntry {

        ErrQueueEntry(ProxyConnection::EventHandler* _eventHandler,
                      uint16_t _errorCode, uint32_t _tag) :
                      eventHandler_(_eventHandler),
                      errorCode_(_errorCode),
					  tag_(_tag) { }

        ProxyConnection::EventHandler* eventHandler_;
        uint16_t errorCode_;
        uint32_t tag_;

        void process();
    };

    struct FunctionQueueEntry : QueueEntry {

        typedef std::function<void(const uint32_t)> Function;

        FunctionQueueEntry(Watch* _watch,
                               Function _function,
                               uint32_t _value) :
                               QueueEntry(_watch),
                               function_(std::move(_function)),
                               value_(_value){ }

        Function function_;
        uint32_t value_;

        void process();
    };

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

    void pushQueue(std::shared_ptr<QueueEntry> _queueEntry);

    void popQueue();

    std::shared_ptr<QueueEntry> frontQueue();

    bool emptyQueue();

    void processQueueEntry(std::shared_ptr<QueueEntry> _queueEntry);

private:
    int pipeFileDescriptors_[2];

    pollfd pollFileDescriptor_;
    std::vector<CommonAPI::DispatchSource*> dependentDispatchSources_;
    std::queue<std::shared_ptr<QueueEntry>> queue_;

    std::mutex queueMutex_;

    std::weak_ptr<Connection> connection_;

    const int pipeValue_;
#ifdef WIN32
    HANDLE wsaEvent_;
    OVERLAPPED ov;
#endif
};

} // namespace IntraP
} // namespace CommonAPI

#endif /* WATCH_HPP_ */
