// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_ASYNC_CALLBACK_HANDLER_HPP_
#define COMMONAPI_SOMEIP_PROXY_ASYNC_CALLBACK_HANDLER_HPP_

#include <functional>
#include <future>
#include <memory>

#include <CommonAPI/SomeIP/Helper.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/SerializableArguments.hpp>

namespace CommonAPI {
namespace SomeIP {

template < typename ... _ArgTypes >
class ProxyAsyncCallbackHandler: public ProxyConnection::MessageReplyAsyncHandler {
 public:
    typedef std::function< void(CallStatus, _ArgTypes...) > FunctionType;

    static std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler > create(FunctionType &&callback) {
        return std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler >(
                new ProxyAsyncCallbackHandler(std::move(callback)));
    }

    ProxyAsyncCallbackHandler() = delete;
    ProxyAsyncCallbackHandler(FunctionType&& callback):
        callback_(std::move(callback)) {
    }

    virtual std::future< CallStatus > getFuture() {
        return promise_.get_future();
    }

    virtual void onMessageReply(const CallStatus &callStatus, const Message &message) {
        promise_.set_value(handleMessageReply(callStatus, message, typename make_sequence< sizeof...(_ArgTypes) >::type()));
    }

 private:
    template < int... _ArgIndices >
    inline CallStatus handleMessageReply(const CallStatus _callStatus, const Message &message, index_sequence< _ArgIndices... >) const {
        CallStatus callStatus = _callStatus;
        std::tuple< _ArgTypes... > argTuple;

        if (callStatus == CallStatus::SUCCESS) {
            if (!message.isErrorType()) {
                InputStream inputStream(message);
                const bool success = SerializableArguments< _ArgTypes... >::deserialize(inputStream, std::get< _ArgIndices >(argTuple)...);
                if (!success) {
                    callStatus = CallStatus::REMOTE_ERROR;
                }
            } else {
                callStatus = CallStatus::REMOTE_ERROR;
            }
        }

        callback_(callStatus, std::move(std::get< _ArgIndices >(argTuple))...);
        return callStatus;
    }

    std::promise< CallStatus > promise_;
    const FunctionType callback_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXY_ASYNC_CALLBACK_HANDLER_HPP_
