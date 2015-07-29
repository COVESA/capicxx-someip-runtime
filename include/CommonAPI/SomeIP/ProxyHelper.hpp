// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_HELPER_HPP_
#define COMMONAPI_SOMEIP_PROXY_HELPER_HPP_

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <mutex>

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
    template <class...> class _In, class... _InArgs,
    template <class...> class _Out, class... _OutArgs>
    struct ProxyHelper<_In<_InArgs...>, _Out<_OutArgs...>> {

		template <typename _Proxy = Proxy>
		static void callMethod(
			const _Proxy &_proxy,
			const method_id_t _methodId,
			const bool _reliable,
			const _InArgs &... _inArgs,
			CommonAPI::CallStatus &_callStatus) {
			
			static std::mutex callMethod_mutex_;
			std::lock_guard<std::mutex> lock(callMethod_mutex_);
			Message methodCall = _proxy.createMethodCall(_methodId, _reliable);
			callMethod(_proxy, methodCall, _inArgs..., _callStatus);
		}

        template <typename _Proxy = Proxy>
        static void callMethod(const _Proxy &_proxy,
                        const char *_methodName,
                        const char *_methodSignature,
                        const _InArgs&... _inArgs,
                        CommonAPI::CallStatus &_callStatus) {

        if (_proxy.isAvailable()) {

            Message message = _proxy.createMethodCall(_methodName, _methodSignature);

            if (sizeof...(_InArgs) > 0) {
                OutputStream outputStream(message);
                const bool success = SerializableArguments<_InArgs...>::serialize(outputStream, _inArgs...);
                if (!success) {
                    _callStatus = CallStatus::OUT_OF_MEMORY;
                    return;
                }
                outputStream.flush();
            }

            const bool isSent = _proxy.getSomeIpConnection()->sendSomeIpMessage(message);
            _callStatus = isSent ? CallStatus::SUCCESS : CallStatus::OUT_OF_MEMORY;
        } else {
            _callStatus = CallStatus::NOT_AVAILABLE;
        }
    }

	template <typename _Proxy = Proxy>
	static void callMethod(
		const _Proxy &_proxy,
		Message &_methodCall,
		const _InArgs &... _inArgs,
		CommonAPI::CallStatus &_callStatus) {
		if (_proxy.isAvailable()) {
			if (sizeof...(_InArgs) > 0) {
				OutputStream outputStream(_methodCall);
				const bool success = SerializableArguments<_InArgs...>::serialize(outputStream, _inArgs...);
				if (!success) {
					_callStatus = CallStatus::OUT_OF_MEMORY;
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

    template <typename _Proxy = Proxy>
    static void callMethodWithReply(
                    const _Proxy &_proxy,
                    Message &_methodCall,
                    const CommonAPI::CallInfo *_info,
                    const _InArgs &... _inArgs,
                    CommonAPI::CallStatus &_callStatus,
                    _OutArgs &... _outArgs) {
        if (_proxy.isAvailable()) {
            if (sizeof...(_InArgs) > 0) {
                OutputStream outputStream(_methodCall);
                const bool success = SerializableArguments<_InArgs...>::serialize(outputStream, _inArgs...);
                if (!success) {
                    _callStatus = CallStatus::OUT_OF_MEMORY;
                    return;
                }
                outputStream.flush();
            }

            Message reply = _proxy.getConnection()->sendMessageWithReplyAndBlock(_methodCall, _info);

            if (!reply.isResponseType()) {
                _callStatus = CallStatus::REMOTE_ERROR;
                return;
            }

            if (sizeof...(_OutArgs) > 0) {
                InputStream inputStream(reply);
                const bool success = SerializableArguments<_OutArgs...>::deserialize(inputStream, _outArgs...);
                if (!success) {
                    _callStatus = CallStatus::REMOTE_ERROR;
                    return;
                }
            }
            _callStatus = CallStatus::SUCCESS;
        } else {
            _callStatus = CallStatus::NOT_AVAILABLE;
        }
    }

    template <typename _Proxy = Proxy>
    static void callMethodWithReply(
                    const _Proxy &_proxy,
                    const method_id_t _methodId,
                    const bool _reliable,
                    const CommonAPI::CallInfo *_info,
                    const _InArgs&... _inArgs,
                    CommonAPI::CallStatus &_callStatus,
                    _OutArgs&... _outArgs) {
        static std::mutex callMethodWithReply_mutex_;
        std::lock_guard<std::mutex> lock(callMethodWithReply_mutex_);
        Message methodCall = _proxy.createMethodCall(_methodId, _reliable);
        callMethodWithReply(_proxy, methodCall, _info, _inArgs..., _callStatus, _outArgs...);
    }

    template <typename _Proxy = Proxy, typename _AsyncCallback>
    static std::future<CallStatus> callMethodAsync(
                    const _Proxy &_proxy,
                    const method_id_t _methodId,
                    const bool _reliable,
                    const CommonAPI::CallInfo *_info,
                    const _InArgs&... _inArgs,
                    _AsyncCallback _asyncCallback,
                    std::tuple<_OutArgs...> _outArgs) {
        static std::mutex callMethodAsync_mutex_;
        std::lock_guard<std::mutex> lock(callMethodAsync_mutex_);
        Message methodCall = _proxy.createMethodCall(_methodId, _reliable);
        return callMethodAsync(_proxy, methodCall, _info, _inArgs..., _asyncCallback, _outArgs);
    }

    template <typename _Proxy = Proxy, typename _AsyncCallback>
    static std::future<CallStatus> callMethodAsync(
                    const _Proxy &_proxy,
                    Message &_message,
                    const CommonAPI::CallInfo *_info,
                    const _InArgs&... _inArgs,
                    _AsyncCallback _asyncCallback,
                    std::tuple<_OutArgs...> _outArgs) {
        if (_proxy.isAvailable()) {
            if (sizeof...(_InArgs) > 0) {
                OutputStream outputStream(_message);
                const bool success = SerializableArguments< _InArgs... >::serialize(outputStream, _inArgs...);
                if (!success) {
                    std::promise<CallStatus> promise;
                    promise.set_value(CallStatus::OUT_OF_MEMORY);
                    return promise.get_future();
                }
                outputStream.flush();
            }

            return _proxy.getConnection()->sendMessageWithReplyAsync(
                                               _message,
                                               ProxyAsyncCallbackHandler<
                                                   _OutArgs...
                                               >::create(std::move(_asyncCallback)),
                                               _info);
        } else {
            CallStatus callStatus = CallStatus::NOT_AVAILABLE;
            callCallbackForCallStatus(callStatus,
            		_asyncCallback,
					typename make_sequence<sizeof...(_OutArgs)>::type(),
					_outArgs);
            std::promise< CallStatus > promise;
            promise.set_value(callStatus);
            return promise.get_future();
        }
    }

    template <int... _ArgIndices>
    static void callCallbackForCallStatus(CallStatus callStatus,
    		std::function<void(CallStatus, _OutArgs...)> _callback,
            index_sequence<_ArgIndices...>,
            std::tuple<_OutArgs...> _argTuple) {

        _callback(callStatus, std::move(std::get<_ArgIndices>(_argTuple))...);
    }

    template <typename _AsyncCallback>
	static void callCallbackForCallStatus(CallStatus callStatus,
			_AsyncCallback _asyncCallback,
			std::tuple<_OutArgs...> _outArgs) {
		callCallbackForCallStatus(callStatus, _asyncCallback, typename make_sequence<sizeof...(_OutArgs)>::type(), _outArgs);
	}
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXY_HELPER_HPP_
