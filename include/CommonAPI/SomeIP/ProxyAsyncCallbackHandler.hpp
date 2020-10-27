// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXYASYNCCALLBACKHANDLER_HPP_
#define COMMONAPI_SOMEIP_PROXYASYNCCALLBACKHANDLER_HPP_

#include <functional>
#include <future>
#include <memory>

#include <CommonAPI/SomeIP/Helper.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/SerializableArguments.hpp>

namespace CommonAPI {
namespace SomeIP {

template <typename DelegateObjectType_, typename ... ArgTypes_ >
class ProxyAsyncCallbackHandler: public ProxyConnection::MessageReplyAsyncHandler {
 public:

    struct Delegate {
        typedef std::function< void(CallStatus, ArgTypes_...) > FunctionType;

        Delegate(std::shared_ptr<DelegateObjectType_> object, FunctionType function) :
            function_(std::move(function)) {
            object_ = object;
        }
        std::weak_ptr<DelegateObjectType_> object_;
        FunctionType function_;
    };

    static std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler > create(
            Delegate &delegate, bool _isLittleEndian, std::tuple< ArgTypes_... >&& _argTuple) {
        return std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler >(
                new ProxyAsyncCallbackHandler<DelegateObjectType_, ArgTypes_...>(
                        std::move(delegate), _isLittleEndian, std::move(_argTuple)));
    }

    ProxyAsyncCallbackHandler() = delete;
    ProxyAsyncCallbackHandler(Delegate&& delegate, bool _isLittleEndian, std::tuple< ArgTypes_... >&& _argTuple):
        delegate_(std::move(delegate)),
        isLittleEndian_(_isLittleEndian),
        argTuple_(std::move(_argTuple)) {
    }

    virtual std::future< CallStatus > getFuture() {
        return promise_.get_future();
    }

    virtual void onMessageReply(const CallStatus &callStatus, const Message &message) {
        promise_.set_value(handleMessageReply(callStatus, message, typename make_sequence< sizeof...(ArgTypes_) >::type()));
    }

 private:
    template<size_t... ArgIndices_>
    inline CallStatus handleMessageReply(const CallStatus _callStatus, const Message &message, index_sequence< ArgIndices_... >) {
        CallStatus callStatus = _callStatus;
        if (callStatus == CallStatus::SUCCESS) {
            if (!message.isErrorType()) {
                if(!message.isValidCRC()) {
                    callStatus = CallStatus::INVALID_VALUE;
                } else {
                    InputStream inputStream(message, isLittleEndian_);
                    const bool success = SerializableArguments< ArgTypes_... >::deserialize(inputStream, std::get< ArgIndices_ >(argTuple_)...);
                    if (!success) {
                        COMMONAPI_ERROR("ProxyAsyncCallbackHandler (someip): deserialization failed!");
                        callStatus = CallStatus::SERIALIZATION_ERROR;
                    }
                }
            } else {
                callStatus = CallStatus::REMOTE_ERROR;
            }
        }

        //ensure that delegate object (i.e Proxy) is not destroyed while callback function is invoked
        if(auto itsDelegateObject = this->delegate_.object_.lock())
            this->delegate_.function_(callStatus, std::move(std::get< ArgIndices_ >(argTuple_))...);

        return callStatus;
    }

    std::promise< CallStatus > promise_;
    const Delegate delegate_;
    bool isLittleEndian_;
    std::tuple< ArgTypes_... > argTuple_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXYASYNCCALLBACKHANDLER_HPP_
