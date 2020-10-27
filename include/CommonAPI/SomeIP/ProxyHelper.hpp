// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXYHELPER_HPP_
#define COMMONAPI_SOMEIP_PROXYHELPER_HPP_

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <mutex>

#include <CommonAPI/Logger.hpp>

#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/ProxyAsyncCallbackHandler.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/SerializableArguments.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

class Proxy;

template <class, class>
struct ProxyHelper;

template <
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_>
struct ProxyHelper<In_<InArgs_...>, Out_<OutArgs_...>> {

    template <typename Proxy_ = Proxy>
    static void callMethod(
        Proxy_ &_proxy,
        const method_id_t _methodId,
        const bool _isReliable,
        const bool _isLittleEndian,
        const InArgs_ &... _inArgs,
        CommonAPI::CallStatus &_callStatus) {

        Message methodCall = _proxy.createMethodCall(_methodId, _isReliable);
        callMethod(_proxy, methodCall, _isLittleEndian, _inArgs..., _callStatus);
    }

    template <typename Proxy_ = Proxy>
    static void callMethod(
        Proxy_ &_proxy,
        Message &_methodCall,
        const bool _isLittleEndian,
        const InArgs_ &... _inArgs,
        CommonAPI::CallStatus &_callStatus) {
        if (_proxy.isAvailable()) {
            if (sizeof...(InArgs_) > 0) {
                OutputStream outputStream(_methodCall, _isLittleEndian);
                const bool success = SerializableArguments<InArgs_...>::serialize(outputStream, _inArgs...);
                if (!success) {
                    COMMONAPI_ERROR("MethodSync(someip): serialization failed: [",
                            _methodCall.getServiceId(), ".",
                            _methodCall.getInstanceId(), ".",
                            _methodCall.getMethodId(), "]");

                    _callStatus = CallStatus::SERIALIZATION_ERROR;
                    return;
                }
                outputStream.flush();
            }

            bool success = _proxy.getConnection()->sendMessage(_methodCall);

            if (!success) {
                _callStatus = CallStatus::REMOTE_ERROR;
                return;
            }

            _callStatus = CallStatus::SUCCESS;
        }
        else {
            _callStatus = CallStatus::NOT_AVAILABLE;
        }
    }

    template <typename Proxy_ = Proxy>
    static void callMethodWithReply(
                    Proxy_ &_proxy,
                    Message &_methodCall,
                    const bool _isLittleEndian,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_ &... _inArgs,
                    CommonAPI::CallStatus &_callStatus,
                    OutArgs_ &... _outArgs) {
        if (_proxy.isAvailable()) {
            if (sizeof...(InArgs_) > 0) {
                OutputStream outputStream(_methodCall, _isLittleEndian);
                const bool success = SerializableArguments<InArgs_...>::serialize(outputStream, _inArgs...);
                if (!success) {
                    COMMONAPI_ERROR("MethodSync w/ reply (someip): serialization failed: [",
                            _methodCall.getServiceId(), ".",
                            _methodCall.getInstanceId(), ".",
                            _methodCall.getMethodId(), "]");

                    _callStatus = CallStatus::SERIALIZATION_ERROR;
                    return;
                }
                outputStream.flush();
            }

            Message reply = _proxy.getConnection()->sendMessageWithReplyAndBlock(_methodCall, _info);

            if (!reply.isResponseType()) {
                _callStatus = CallStatus::REMOTE_ERROR;
                return;
            }

            if(!reply.isValidCRC()) {
                _callStatus = CallStatus::INVALID_VALUE;
                return;
            }

            if (sizeof...(OutArgs_) > 0) {
                InputStream inputStream(reply, _isLittleEndian);
                const bool success = SerializableArguments<OutArgs_...>::deserialize(inputStream, _outArgs...);
                if (!success) {
                    COMMONAPI_ERROR("MethodSync w/ reply (someip): reply deserialization failed: [",
                            reply.getServiceId(), ".",
                            reply.getInstanceId(), ".",
                            reply.getMethodId(), ".",
                            reply.getSessionId(), ".");

                    _callStatus = CallStatus::SERIALIZATION_ERROR;
                    return;
                }
            }
            _callStatus = CallStatus::SUCCESS;
        } else {
            _callStatus = CallStatus::NOT_AVAILABLE;
        }
    }

    template <typename Proxy_ = Proxy>
    static void callMethodWithReply(
                    Proxy_ &_proxy,
                    const method_id_t _methodId,
                    const bool _isReliable,
                    const bool _isLittleEndian,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _inArgs,
                    CommonAPI::CallStatus &_callStatus,
                    OutArgs_&... _outArgs) {
        Message methodCall = _proxy.createMethodCall(_methodId, _isReliable);
        callMethodWithReply(_proxy, methodCall, _isLittleEndian, _info, _inArgs..., _callStatus, _outArgs...);
    }

    template <typename Proxy_ = Proxy, typename DelegateFunction_>
    static std::future<CallStatus> callMethodAsync(
                    Proxy_ &_proxy,
                    const method_id_t _methodId,
                    const bool _isReliable,
                    const bool _isLittleEndian,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _inArgs,
                    DelegateFunction_ _function,
                    std::tuple<OutArgs_...> _outArgs) {
        Message methodCall = _proxy.createMethodCall(_methodId, _isReliable);
        return callMethodAsync(_proxy, methodCall, _isLittleEndian, _info, _inArgs..., _function, _outArgs);
    }

    template <typename Proxy_ = Proxy, typename DelegateFunction_>
    static std::future<CallStatus> callMethodAsync(
                    Proxy_ &_proxy,
                    Message &_message,
                    const bool _isLittleEndian,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _inArgs,
                    DelegateFunction_ _function,
                    std::tuple<OutArgs_...> _outArgs) {
        if (sizeof...(InArgs_) > 0) {
            OutputStream outputStream(_message, _isLittleEndian);
            const bool success = SerializableArguments< InArgs_... >::serialize(outputStream, _inArgs...);
            if (!success) {
                COMMONAPI_ERROR("MethodAsync(someip): serialization failed: [",
                        _message.getServiceId(), ".",
                        _message.getInstanceId(), ".",
                        _message.getMethodId(), "]");

                std::promise<CallStatus> promise;
                promise.set_value(CallStatus::SERIALIZATION_ERROR);
                return promise.get_future();
            }
            outputStream.flush();
        }

        typename ProxyAsyncCallbackHandler<
                                Proxy, OutArgs_...
                                >::Delegate delegate(_proxy.shared_from_this(), _function);
        auto messageReplyAsyncHandler = ProxyAsyncCallbackHandler<
                                            Proxy, OutArgs_...
                                        >::create(delegate, _isLittleEndian, std::move(_outArgs));

        std::future< CallStatus > callStatusFuture;
        try {
            callStatusFuture = messageReplyAsyncHandler->getFuture();
        } catch (std::exception& e) {
            COMMONAPI_ERROR("MethodAsync(someip): messageReplyAsyncHandler future failed(", e.what(), ")");
        }

        if (_proxy.isAvailable()) {
            _proxy.getConnection()->sendMessageWithReplyAsync(
                       _message,
                       std::move(messageReplyAsyncHandler),
                       _info);
            COMMONAPI_VERBOSE("MethodAsync(someip): Proxy available -> sendMessageWithReplyAsync");
            return callStatusFuture;
        } else {
            std::shared_ptr< std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler > > sharedMessageReplyAsyncHandler(
                    new std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler >(std::move(messageReplyAsyncHandler)));
            //async isAvailable call with timeout
            COMMONAPI_VERBOSE("MethodAsync(someip): Proxy not available -> register calback");
            _proxy.isAvailableAsync([&_proxy, _message, sharedMessageReplyAsyncHandler](
                                             const AvailabilityStatus _status,
                                             const Timeout_t remaining) {
                if(_status == AvailabilityStatus::AVAILABLE) {
                    //create new call info with remaining timeout. Minimal timeout is 100 ms.
                    Timeout_t newTimeout = remaining;
                    if(remaining < 100)
                        newTimeout = 100;
                    CallInfo newInfo(newTimeout);
                    if(*sharedMessageReplyAsyncHandler) {
                    _proxy.getConnection()->sendMessageWithReplyAsync(
                        _message,
                        std::move(*sharedMessageReplyAsyncHandler),
                        &newInfo);
                    COMMONAPI_VERBOSE("MethodAsync(someip): Proxy callback available -> sendMessageWithReplyAsync");
                    } else {
                        COMMONAPI_ERROR("MethodAsync(someip): Proxy callback available but callback taken");
                    }
                } else {
                    //create error message and push it directly to the connection
                    if (*sharedMessageReplyAsyncHandler) {
                        Message message = _message.createErrorResponseMessage(vsomeip::return_code_e::E_NOT_REACHABLE);
                        _proxy.getConnection()->proxyPushMessageToMainLoop(message,
                                std::move(*sharedMessageReplyAsyncHandler));
                        COMMONAPI_VERBOSE("MethodAsync(someip): Proxy callback not reachable -> sendMessageWithReplyAsync");
                    } else {
                        COMMONAPI_ERROR("MethodAsync(someip): Proxy callback not reachable but callback taken");
                    }
                }
            }, _info);
            return callStatusFuture;
        }
    }

    template <size_t... ArgIndices_>
    static void callCallbackForCallStatus(CallStatus callStatus,
            std::function<void(CallStatus, OutArgs_...)> _callback,
            index_sequence<ArgIndices_...>,
            std::tuple<OutArgs_...> _argTuple) {
        (void)_argTuple;
        _callback(callStatus, std::move(std::get<ArgIndices_>(_argTuple))...);
    }

    template <typename AsyncCallback_>
    static void callCallbackForCallStatus(CallStatus callStatus,
            AsyncCallback_ _asyncCallback,
            std::tuple<OutArgs_...> _outArgs) {
        callCallbackForCallStatus(callStatus, _asyncCallback, typename make_sequence<sizeof...(OutArgs_)>::type(), _outArgs);
    }
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXYHELPER_HPP_
