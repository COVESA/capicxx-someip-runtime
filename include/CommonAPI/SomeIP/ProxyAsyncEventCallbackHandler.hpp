// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXYASYNCEVENTCALLBACKHANDLER_HPP_
#define COMMONAPI_SOMEIP_PROXYASYNCEVENTCALLBACKHANDLER_HPP_

#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>

namespace CommonAPI {
namespace SomeIP {

template <typename DelegateObjectType_>
class ProxyAsyncEventCallbackHandler: public ProxyConnection::MessageReplyAsyncHandler {
 public:

    struct Delegate {
        typedef std::function< void(CallStatus, Message, ProxyConnection::EventHandler*, uint32_t) > FunctionType;

         Delegate(std::shared_ptr<DelegateObjectType_> object, FunctionType function) :
             function_(std::move(function)) {
             object_ = object;
         }
         std::weak_ptr<DelegateObjectType_> object_;
         FunctionType function_;
     };

    static std::unique_ptr<ProxyConnection::MessageReplyAsyncHandler> create(
            Delegate& delegate, ProxyConnection::EventHandler *_eventHandler,
            const uint32_t tag) {
        return std::unique_ptr<ProxyConnection::MessageReplyAsyncHandler>(
                new ProxyAsyncEventCallbackHandler<DelegateObjectType_>(std::move(delegate), _eventHandler, tag));
    }

    ProxyAsyncEventCallbackHandler() = delete;
    ProxyAsyncEventCallbackHandler(Delegate&& delegate,
            ProxyConnection::EventHandler *_eventHandler,
            const uint32_t tag):
        delegate_(std::move(delegate)), eventHandler_(_eventHandler), tag_(tag) {
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

        //ensure that delegate object (i.e Proxy) is not destroyed while callback function is invoked
        if(auto itsDelegateObject = this->delegate_.object_.lock())
            this->delegate_.function_(status, message, eventHandler_, tag_);

        return status;
    }

    std::promise< CallStatus > promise_;
    const Delegate delegate_;
    ProxyConnection::EventHandler *eventHandler_;
    const uint32_t tag_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXYASYNCEVENTCALLBACKHANDLER_HPP_
