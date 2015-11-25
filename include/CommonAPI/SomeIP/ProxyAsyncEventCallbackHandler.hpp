// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_ASYNC_EVENT_CALLBACK_HANDLER_HPP_
#define COMMONAPI_SOMEIP_PROXY_ASYNC_EVENT_CALLBACK_HANDLER_HPP_

#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>

namespace CommonAPI {
namespace SomeIP {

class ProxyAsyncEventCallbackHandler: public ProxyConnection::MessageReplyAsyncHandler {
 public:
    typedef std::function< void(CallStatus, Message, ProxyConnection::EventHandler*, uint32_t) > FunctionType;

    static std::unique_ptr<ProxyConnection::MessageReplyAsyncHandler> create(
            FunctionType& callback, ProxyConnection::EventHandler *_eventHandler,
            const uint32_t tag) {
        return std::unique_ptr<ProxyConnection::MessageReplyAsyncHandler>(
                new ProxyAsyncEventCallbackHandler(std::move(callback), _eventHandler, tag));
    }

    ProxyAsyncEventCallbackHandler() = delete;
    ProxyAsyncEventCallbackHandler(FunctionType&& callback,
            ProxyConnection::EventHandler *_eventHandler,
            const uint32_t tag):
        callback_(std::move(callback)), eventHandler_(_eventHandler), tag_(tag) {
    }

    virtual std::future< CallStatus > getFuture() {
        return promise_.get_future();
    }

    virtual void onMessageReply(const CallStatus &callStatus, const Message &message) {
        promise_.set_value(handleMessageReply(callStatus, message));
    }

 private:
    inline CallStatus handleMessageReply(const CallStatus callStatus, const Message& message) const {
        CallStatus status = callStatus;

        callback_(status, message, eventHandler_, tag_);
        return status;
    }

    std::promise< CallStatus > promise_;
    const FunctionType callback_;
    ProxyConnection::EventHandler *eventHandler_;
    const uint32_t tag_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXY_ASYNC_EVENT_CALLBACK_HANDLER_HPP_
