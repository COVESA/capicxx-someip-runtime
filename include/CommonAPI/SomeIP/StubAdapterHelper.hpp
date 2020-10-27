// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_STUBADAPTERHELPER_HPP_
#define COMMONAPI_SOMEIP_STUBADAPTERHELPER_HPP_

#include <initializer_list>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <iostream>
#include <string>


#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/ClientId.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>
#include <CommonAPI/SomeIP/Helper.hpp>
#include <CommonAPI/SomeIP/InputStream.hpp>
#include <CommonAPI/SomeIP/OutputStream.hpp>
#include <CommonAPI/SomeIP/SerializableArguments.hpp>
#include <CommonAPI/SomeIP/StubAdapter.hpp>

namespace CommonAPI {
namespace SomeIP {

template <typename StubClass_>
class StubDispatcher {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    virtual ~StubDispatcher() {}
    virtual bool dispatchMessage(const Message &_message,
                                 const std::shared_ptr<StubClass_> &_stub,
                                 RemoteEventHandlerType *_remoteEventHandler,
                                 std::shared_ptr<ProxyConnection> _connection) = 0;
};

template <typename StubClass_>
struct AttributeDispatcherStruct {
    StubDispatcher<StubClass_>* getter;
    StubDispatcher<StubClass_>* setter;

    AttributeDispatcherStruct(StubDispatcher<StubClass_>* g, StubDispatcher<StubClass_>* s) {
        getter = g;
        setter = s;
    }
};

template <typename T>
struct identity { typedef T type; };

template <typename... Stubs_>
class StubAdapterHelper {
public:
  StubAdapterHelper(const Address &_address,
                    const std::shared_ptr< ProxyConnection > &_connection,
                    const std::shared_ptr< StubBase> &_stub) :
                        proxyConnection_(_connection) {
      (void) _address;
      (void) _stub;
  }
  template <typename RemoteEventHandlerType>
  void setRemoteEventHandler(RemoteEventHandlerType * _remoteEventHandler) {
    (void) _remoteEventHandler;
  }
protected:
    bool findDispatcherAndHandle(const Message &message, const method_id_t &methodId) {
      (void) message;
      (void) methodId;
      auto error = message.createErrorResponseMessage(return_code_e::E_UNKNOWN_METHOD);
      proxyConnection_->sendMessage(error);
      return false;
    }

    std::shared_ptr< ProxyConnection > proxyConnection_;
};

template <typename StubClass_, typename... Stubs_>
class StubAdapterHelper<StubClass_, Stubs_...>:
 public virtual StubAdapter,
 public StubAdapterHelper<Stubs_...> {
 public:
    typedef typename StubClass_::StubAdapterType StubAdapterType;
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    // interfaceMemberName, interfaceMemberSignature
    typedef std::string InterfaceMemberPath;
    typedef std::unordered_map<method_id_t, StubDispatcher<StubClass_>*> StubDispatcherTable;
    typedef std::unordered_map<std::string, AttributeDispatcherStruct<StubClass_>> StubAttributeTable;

 public:
    StubAdapterHelper(const Address &_address,
                      const std::shared_ptr< ProxyConnection > &_connection,
                      const std::shared_ptr< StubBase > &_stub)
        : StubAdapter(_address, _connection),
          StubAdapterHelper<Stubs_...>(_address, _connection, _stub),
          remoteEventHandler_(nullptr) {
        stub_ = std::dynamic_pointer_cast<StubClass_>(_stub);
    }

    virtual ~StubAdapterHelper() {
        StubAdapter::deinit();
        stub_.reset();
    }

    virtual void init(std::shared_ptr<StubAdapter> instance) {
        StubAdapter::init(instance);
        std::shared_ptr<StubAdapterType> stubAdapter
            = std::dynamic_pointer_cast<StubAdapterType>(instance);
        remoteEventHandler_ = stub_->initStubAdapter(stubAdapter);
        StubAdapterHelper<Stubs_...>::setRemoteEventHandler(remoteEventHandler_);
    }

    void setRemoteEventHandler(RemoteEventHandlerType* _remoteEventHandler) {
      remoteEventHandler_ = _remoteEventHandler;
      StubAdapterHelper<Stubs_...>::setRemoteEventHandler(remoteEventHandler_);
    }

    virtual void deinit() {
        StubAdapter::deinit();
        stub_.reset();
    }

    inline RemoteEventHandlerType* getRemoteEventHandler() {
        return remoteEventHandler_;
    }

 protected:

    virtual bool onInterfaceMessage(const Message &message) {
        const method_id_t methodId = message.getMethodId();
        return findDispatcherAndHandle(message, methodId);
    }

    bool findDispatcherAndHandle(const Message &message, const method_id_t &methodId) {
        auto findIterator = stubDispatcherTable_.find(methodId);
        const bool foundInterfaceMemberHandler = (findIterator != stubDispatcherTable_.end());
        bool isMessageHandled = false;

        // To prevent the destruction of the stub while still handling a message
        if (stub_ && foundInterfaceMemberHandler) {
            StubDispatcher<StubClass_> *stubDispatcher = findIterator->second;
            isMessageHandled = stubDispatcher->dispatchMessage(message, stub_, getRemoteEventHandler(), getConnection());
            if (!isMessageHandled) {
                if (message.isRequestType()) {
                    auto error = message.createErrorResponseMessage(return_code_e::E_MALFORMED_MESSAGE);
                    connection_->sendMessage(error);
                }
            }
            return isMessageHandled;
        }
        return StubAdapterHelper<Stubs_...>::findDispatcherAndHandle(message, methodId);
    }

    template <typename Stub_>
    void addStubDispatcher(method_id_t methodId,
                           StubDispatcher<Stub_>* _stubDispatcher) {
        addStubDispatcher(methodId, _stubDispatcher, identity<Stub_>());
    }

    template <typename Stub_>
    void addAttributeDispatcher(std::string _key,
                                StubDispatcher<Stub_>* _stubDispatcherGetter,
                                StubDispatcher<Stub_>* _stubDispatcherSetter) {
        addAttributeDispatcher(_key, _stubDispatcherGetter, _stubDispatcherSetter, identity<Stub_>());
    }

    std::shared_ptr<StubClass_> stub_;
    RemoteEventHandlerType *remoteEventHandler_;
    StubDispatcherTable stubDispatcherTable_;
    StubAttributeTable stubAttributeTable_;

  private:
    template <typename Stub_>
    void addStubDispatcher(method_id_t methodId,
                           StubDispatcher<Stub_>* _stubDispatcher,
                           identity<Stub_>) {
        StubAdapterHelper<Stubs_...>::addStubDispatcher(methodId, _stubDispatcher);

    }

    void addStubDispatcher(method_id_t methodId,
                           StubDispatcher<StubClass_>* _stubDispatcher,
                           identity<StubClass_>) {
        stubDispatcherTable_.insert({methodId, _stubDispatcher});

    }

    template <typename Stub_>
    void addAttributeDispatcher(std::string _key,
                           StubDispatcher<Stub_>* _stubDispatcherGetter,
                           StubDispatcher<Stub_>* _stubDispatcherSetter,
                           identity<Stub_>) {
        StubAdapterHelper<Stubs_...>::addAttributeDispatcher(_key, _stubDispatcherGetter, _stubDispatcherSetter);

    }

    void addAttributeDispatcher(std::string _key,
                           StubDispatcher<StubClass_>* _stubDispatcherGetter,
                           StubDispatcher<StubClass_>* _stubDispatcherSetter,
                           identity<StubClass_>) {
        stubAttributeTable_.insert({_key, {_stubDispatcherGetter, _stubDispatcherSetter}});
    }
};

template <class>
struct StubEventHelper;

template <template <class ...> class In_, class... InArgs_>
struct StubEventHelper<In_<InArgs_...>> {

    static inline bool sendEvent(const Address &_address,
                                 const event_id_t &_event,
                                 const bool _isLittleEndian,
                                 const std::shared_ptr<ProxyConnection> &_connection,
                                 const InArgs_&... _in) {

        Message message = Message::createNotificationMessage(_address, _event, false);
        if (sizeof...(InArgs_) > 0) {
            OutputStream output(message, _isLittleEndian);
            if (!SerializableArguments<InArgs_...>::serialize(output, _in...)) {
                COMMONAPI_ERROR("StubEventHelper (someip): serialization failed! [",
                        message.getServiceId(), ".",
                        message.getInstanceId(), ".",
                        message.getMethodId());
                return false;
            }
            output.flush();
        }

        return _connection->sendEvent(message);
    }

    template <typename Stub_ = StubAdapter>
    static bool sendEvent(const Stub_ &_stub,
                          const event_id_t &_event,
                          const bool _isLittleEndian,
                          const InArgs_&... _in) {
        return (sendEvent(_stub.getSomeIpAddress(),
                          _event,
                          _isLittleEndian,
                          _stub.getConnection(),
                          _in...));
    }

    template <typename Stub_ = StubAdapter>
    static bool sendEvent(const client_id_t _client,
                          const Stub_ &_stub,
                          const event_id_t &_event,
                          const bool _isLittleEndian,
                          const InArgs_&... _in) {
        Message message = Message::createNotificationMessage(
                _stub.getSomeIpAddress(), _event, false);

        if (sizeof...(InArgs_) > 0) {
            OutputStream output(message, _isLittleEndian);
            if (!SerializableArguments<InArgs_...>::serialize(output, _in...)) {
                COMMONAPI_ERROR("StubEventHelper (someip) 2: serialization failed! [",
                        message.getServiceId(), ".",
                        message.getInstanceId(), ".",
                        message.getMethodId());
                return false;
            }
            output.flush();
        }
        return _stub.getConnection()->sendEvent(message, _client);
    }
};

template<class, class, class>
class MethodStubDispatcher;

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class DeplIn_, class... DeplInArgs_>
class MethodStubDispatcher<StubClass_, In_<InArgs_...>, DeplIn_<DeplInArgs_...>>
    : public StubDispatcher <StubClass_>{
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_...);

    MethodStubDispatcher(
        StubFunctor_ stubFunctor, const bool _isLittleEndian, const bool _isImplemented, std::tuple<DeplInArgs_*...> _in)
        : stubFunctor_(stubFunctor), isLittleEndian_(_isLittleEndian), isImplemented_(_isImplemented) {

        initialize(typename make_sequence_range<sizeof...(DeplInArgs_), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         RemoteEventHandlerType *_remoteEventHandler,
                         std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        return dispatchMessageHelper(
                    _message, _stub, _remoteEventHandler, _connection,
                    typename make_sequence_range<sizeof...(InArgs_), 0>::type());
    }

private:
    template <size_t... DeplInArgIndices_>
    inline void initialize(index_sequence<DeplInArgIndices_...>, std::tuple<DeplInArgs_*...> &_in) {
        in_ = std::make_tuple(std::get<DeplInArgIndices_>(_in)...);
    }

    template <size_t... InArgIndices_>
    inline bool dispatchMessageHelper(const Message &_message,
                                        const std::shared_ptr<StubClass_> &_stub,
                                        RemoteEventHandlerType* _remoteEventHandler,
                                        std::shared_ptr<ProxyConnection> _connection,
                                      index_sequence<InArgIndices_...>) {
        (void)_remoteEventHandler;
        (void)_connection;

        std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in(in_);
        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message, isLittleEndian_);
            if (!SerializableArguments<CommonAPI::Deployable<InArgs_, DeplInArgs_>...>::deserialize(
                    inputStream, std::get<InArgIndices_>(in)...)) {
                COMMONAPI_ERROR("MethodStubDispatcher (someip): deserialization failed! [",
                        _message.getServiceId(), ".",
                        _message.getInstanceId(), ".",
                        _message.getMethodId(), ".",
                        _message.getSessionId(), "]");
                return false;
            }
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId(), _message.getUid(), _message.getGid());

        (_stub.get()->*stubFunctor_)(
            client,
            std::move(std::get<InArgIndices_>(in))...
        );

           return true;
    }

    StubFunctor_ stubFunctor_;
    const bool isLittleEndian_;
    const bool isImplemented_;

    std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in_;
};


template<class, class, class, class, class...>
class MethodWithReplyStubDispatcher;

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_,
    template <class...> class DeplIn_, class... DeplInArgs_,
    template <class...> class DeplOut_, class... DeplOutArgs_>
class MethodWithReplyStubDispatcher<
        StubClass_,
        In_<InArgs_...>,
        Out_<OutArgs_...>,
        DeplIn_<DeplInArgs_...>,
        DeplOut_<DeplOutArgs_...>> :
            public StubDispatcher<StubClass_> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., ReplyType_t);

    MethodWithReplyStubDispatcher(StubFunctor_ _stubFunctor,
                                  const bool _isLittleEndian,
                                  const bool _isImplemented,
                                  const std::tuple<DeplInArgs_*...> _in,
                                  const std::tuple<DeplOutArgs_*...> _out) :
                                      isLittleEndian_(_isLittleEndian),
                                      isImplemented_(_isImplemented),
                                      out_(_out),
                                      currentCall_(0),
                                      stubFunctor_(_stubFunctor) {
        initialize(typename make_sequence_range<sizeof...(DeplInArgs_), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         RemoteEventHandlerType* _remoteEventHandler,
                         std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        return dispatchMessageHelper(
                    _message, _stub, _remoteEventHandler, _connection,
                    typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                    typename make_sequence_range<sizeof...(OutArgs_), 0>::type());
    }

    bool sendReplyMessage(const CommonAPI::CallId_t _call,
                          const std::weak_ptr<ProxyConnection> &_connection,
                          const std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> _args = std::make_tuple()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto message = pending_.find(_call);
            if(message != pending_.end()) {
                Message reply = message->second.createResponseMessage();
                pending_[_call] = reply;
            } else {
                return false;
            }
        }
        return sendReplyMessageHelper(_call, _connection, typename make_sequence_range<sizeof...(OutArgs_), 0>::type(), _args);
    }

protected:
    const bool isLittleEndian_;
    const bool isImplemented_;

    std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in_;
    std::tuple<DeplOutArgs_*...> out_;

    CommonAPI::CallId_t currentCall_;
    std::map<CommonAPI::CallId_t, Message> pending_;
    std::mutex mutex_; // protects pending_

private:
    template <size_t... DeplInArgIndices_>
    inline void initialize(index_sequence<DeplInArgIndices_...>, const std::tuple<DeplInArgs_*...> &_in) {
        in_ = std::make_tuple(std::get<DeplInArgIndices_>(_in)...);
    }

    template <size_t... InArgIndices_, size_t... OutArgIndices_>
    inline bool dispatchMessageHelper(const Message &_message,
                                      const std::shared_ptr<StubClass_> &_stub,
                                      RemoteEventHandlerType *_remoteEventHandler,
                                      std::shared_ptr<ProxyConnection> _connection,
                                      index_sequence<InArgIndices_...>,
                                      index_sequence<OutArgIndices_...>) {
        (void) _remoteEventHandler;

        if (!_message.isRequestType()) {
            auto error = _message.createErrorResponseMessage(return_code_e::E_WRONG_MESSAGE_TYPE);
            _connection->sendMessage(error);
            return true;
        }

        std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in(in_);
        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message, isLittleEndian_);
            if (!SerializableArguments<CommonAPI::Deployable<InArgs_, DeplInArgs_>...>::deserialize(
                    inputStream, std::get<InArgIndices_>(in)...)) {
                COMMONAPI_ERROR("MethodWithReplyStubDispatcher (someip): deserialization failed! [",
                        _message.getServiceId(), ".",
                        _message.getInstanceId(), ".",
                        _message.getMethodId(), ".",
                        _message.getSessionId(), "]");
                return false;
            }
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId(), _message.getUid(), _message.getGid());

        CommonAPI::CallId_t call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            call = currentCall_++;
            pending_[call] = _message;
        }

        // Call the stub function with the list of deserialized in-Parameters
        // and a lambda function which holds the call identifier the deployments
        // for the out-Parameters and extracts them from the deployables before
        // calling the send function.
        std::weak_ptr<ProxyConnection> itsConnection = _connection;
        (_stub.get()->*stubFunctor_)(
            client,
            std::move(std::get<InArgIndices_>(in).getValue())...,
            [call, itsConnection, this](OutArgs_... _args) {
                this->sendReplyMessage(
                    call,
                    itsConnection,
                    std::make_tuple(
                        CommonAPI::Deployable<OutArgs_, DeplOutArgs_>(
                            _args, std::get<OutArgIndices_>(out_)
                        )...
                    )
                );
            }
        );

        return true;
    }

    template<size_t... OutArgIndices_>
    bool sendReplyMessageHelper(const CommonAPI::CallId_t _call,
                                const std::weak_ptr<ProxyConnection> &_connection,
                                index_sequence<OutArgIndices_...>,
                                const std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> &_args) {
        (void)_args;

        std::lock_guard<std::mutex> lock(mutex_);
        auto reply = pending_.find(_call);
        if (reply != pending_.end()) {
            if (sizeof...(DeplOutArgs_) > 0) {
                OutputStream output(reply->second, isLittleEndian_);
                if (!SerializableArguments<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>::serialize(
                        output, std::get<OutArgIndices_>(_args)...)) {
                    COMMONAPI_ERROR("MethodWithReplyStubDispatcher (someip): "
                            "serialization failed! [",
                            reply->second.getServiceId(), ".",
                            reply->second.getInstanceId(), ".",
                            reply->second.getMethodId(), "]");
                    pending_.erase(_call);
                    return false;
                }
                output.flush();
            }
        } else {
            return false;
        }
        bool isSuccessful = false;
        if(auto itsConnection = _connection.lock()) {
            isSuccessful = itsConnection->sendMessage(reply->second);
        }
        pending_.erase(_call);
        return isSuccessful;
    }

    StubFunctor_ stubFunctor_;
};

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_,
    template <class...> class DeplIn_, class... DeplInArgs_,
    template <class...> class DeplOut_, class... DeplOutArgs_,
    class... ErrorReplies_>
class MethodWithReplyStubDispatcher<
        StubClass_,
        In_<InArgs_...>,
        Out_<OutArgs_...>,
        DeplIn_<DeplInArgs_...>,
        DeplOut_<DeplOutArgs_...>,
        ErrorReplies_...>
            : public MethodWithReplyStubDispatcher<
              StubClass_,
              In_<InArgs_...>,
              Out_<OutArgs_...>,
              DeplIn_<DeplInArgs_...>,
              DeplOut_<DeplOutArgs_...>> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, CommonAPI::CallId_t, InArgs_..., ReplyType_t, ErrorReplies_...);

    MethodWithReplyStubDispatcher(StubFunctor_ _stubFunctor,
                                  const bool _isLittleEndian,
                                  const bool _isImplemented,
                                  const std::tuple<DeplInArgs_*...> _in,
                                  const std::tuple<DeplOutArgs_*...> _out,
                                  const ErrorReplies_... _errorReplies) :
                                      MethodWithReplyStubDispatcher<
                                                    StubClass_,
                                                    In_<InArgs_...>,
                                                    Out_<OutArgs_...>,
                                                    DeplIn_<DeplInArgs_...>,
                                                    DeplOut_<DeplOutArgs_...>>(
                                                            NULL,
                                                            _isLittleEndian,
                                                            _isImplemented,
                                                            _in,
                                                            _out),
                                      stubFunctor_(_stubFunctor),
                                      errorReplies_(std::make_tuple(_errorReplies...)) { }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         RemoteEventHandlerType* _remoteEventHandler,
                         std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        return dispatchMessageHelper(
                    _message, _stub, _remoteEventHandler, _connection,
                    typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                    typename make_sequence_range<sizeof...(OutArgs_), 0>::type(),
                    typename make_sequence_range<sizeof...(ErrorReplies_), 0>::type());
    }


    template <class... ErrorReplyOutArgs_, class... ErrorReplyDeplOutArgs_>
    bool sendErrorReplyMessage(const CommonAPI::CallId_t _call,
                               const std::string &_errorName,
                               const std::tuple<CommonAPI::Deployable<ErrorReplyOutArgs_, ErrorReplyDeplOutArgs_>...>& _args) {
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            auto message = this->pending_.find(_call);
            if(message != this->pending_.end()) {
                // TODO create error response message
                Message reply = message->second.createResponseMessage();
                this->pending_[_call] = reply;
            } else {
                return false;
            }
        }
        return sendErrorReplyMessageHandler(_call, typename make_sequence_range<sizeof...(ErrorReplyOutArgs_), 0>::type(), _args);
    }

private:

    template <size_t... InArgIndices_, size_t... OutArgIndices_, size_t... ErrorRepliesIndices_>
    inline bool dispatchMessageHelper(const Message &_message,
                                      const std::shared_ptr<StubClass_> &_stub,
                                      RemoteEventHandlerType* _remoteEventHandler,
                                      std::shared_ptr<ProxyConnection> _connection,
                                      index_sequence<InArgIndices_...>,
                                      index_sequence<OutArgIndices_...>,
                                      index_sequence<ErrorRepliesIndices_...>) {
        (void) _remoteEventHandler;

        if (!_message.isRequestType()) {
            auto error = _message.createErrorResponseMessage(return_code_e::E_WRONG_MESSAGE_TYPE);
            _connection->sendMessage(error);
            return true;
        }

        std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in(this->in_);
        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message, this->isLittleEndian_);
            if (!SerializableArguments<CommonAPI::Deployable<InArgs_, DeplInArgs_>...>::deserialize(
                    inputStream, std::get<InArgIndices_>(in)...)) {
                COMMONAPI_ERROR("MethodWithReplyStubDispatcher w/ error replies"
                        " (someip): deserialization failed! [",
                        _message.getServiceId(), ".",
                        _message.getInstanceId(), ".",
                        _message.getMethodId(), ".",
                        _message.getSessionId(), "]");
                return false;
            }
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId(), _message.getUid(), _message.getGid());

        CommonAPI::CallId_t call;
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            call = this->currentCall_++;
            this->pending_[call] = _message;
        }

        // Call the stub function with the list of deserialized in-Parameters
        // and a lambda function which holds the call identifier the deployments
        // for the out-Parameters and extracts them from the deployables before
        // calling the send function.
        std::weak_ptr<ProxyConnection> itsConnection = _connection;
        (_stub.get()->*stubFunctor_)(
            client,
            call,
            std::move(std::get<InArgIndices_>(in).getValue())...,
            [call, itsConnection, this](OutArgs_... _args) {
                this->sendReplyMessage(
                    call,
                    itsConnection,
                    std::make_tuple(
                        CommonAPI::Deployable<OutArgs_, DeplOutArgs_>(
                            _args, std::get<OutArgIndices_>(this->out_)
                        )...
                    )
                );
            },
            std::get<ErrorRepliesIndices_>(errorReplies_)...
        );

        return true;
    }

    template<size_t... OutArgIndices_>
    bool sendErrorReplyMessageHelper(const CommonAPI::CallId_t _call,
                                     const std::weak_ptr<ProxyConnection> &_connection,
                                     index_sequence<OutArgIndices_...>,
                                     const std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>& _args) {
        (void)_args;

        std::lock_guard<std::mutex> lock(this->mutex_);
        auto reply = this->pending_.find(_call);
        if (reply != this->pending_.end()) {
            if (sizeof...(DeplOutArgs_) > 0) {
                OutputStream output(reply->second, this->isLittleEndian_);
                if (!SerializableArguments<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>::serialize(
                        output, std::get<OutArgIndices_>(_args)...)) {
                    COMMONAPI_ERROR("MethodWithReplyStubDispatcher w/ error replies"
                            "(someip): serialization failed! [",
                            reply.getServiceId(), ".",
                            reply.getInstanceId(), ".",
                            reply.getMethodId(), "]");
                    this->pending_.erase(_call);
                    return false;
                }
                output.flush();
            }
        } else {
            return false;
        }
        bool isSuccessful = false;
        if(auto itsConnection = _connection.lock()) {
            isSuccessful = itsConnection->sendMessage(reply->second);
        }
        this->pending_.erase(_call);
        return isSuccessful;
    }

    StubFunctor_ stubFunctor_;
    std::tuple<ErrorReplies_...> errorReplies_;
};

template<class, class, class, class>
class MethodWithReplyAdapterDispatcher;

template <
    typename StubClass_,
    typename StubAdapterClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_>
class MethodWithReplyAdapterDispatcher<StubClass_, StubAdapterClass_, In_<InArgs_...>, Out_<OutArgs_...>>
    : public StubDispatcher<StubClass_> {

public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef void (StubAdapterClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., OutArgs_&...);
    typedef typename CommonAPI::Stub<typename StubClass_::StubAdapterType, typename StubClass_::RemoteEventType> StubType;

    MethodWithReplyAdapterDispatcher(StubFunctor_ stubFunctor, const bool _isLittleEndian, const bool _isImplemented)
        : stubFunctor_(stubFunctor), isLittleEndian_(_isLittleEndian), isImplemented_(_isImplemented) {
    }

    bool dispatchMessage(const Message &_message,
      const std::shared_ptr<StubClass_> &_stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        std::tuple<InArgs_..., OutArgs_...> argTuple;
        return dispatchMessageHelper(
                        _message,
                        _stub,
                        _remoteEventHandler, _connection,
                        typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                        typename make_sequence_range<sizeof...(OutArgs_), sizeof...(InArgs_)>::type(),
                        argTuple);
    }

 private:
    template <size_t... InArgIndices_, size_t... OutArgIndices_>
    inline bool dispatchMessageHelper(
                        const Message &_message,
                        const std::shared_ptr<StubClass_> &_stub,
                        RemoteEventHandlerType* _remoteEventHandler,
                        std::shared_ptr<ProxyConnection> _connection,
                        index_sequence<InArgIndices_...>,
                        index_sequence<OutArgIndices_...>,
                        std::tuple<InArgs_..., OutArgs_...> _argTuple) const {

        if (!_message.isRequestType()) {
            auto error = _message.createErrorResponseMessage(return_code_e::E_WRONG_MESSAGE_TYPE);
            _connection->sendMessage(error);
            return true;
        }

       if (sizeof...(InArgs_) > 0) {
            InputStream inputStream(_message, isLittleEndian_);
            if (!SerializableArguments<InArgs_...>::deserialize(inputStream, std::get<InArgIndices_>(_argTuple)...)) {
                COMMONAPI_ERROR("MethodWithReplyAdapterDispatcher (someip)"
                        " deserialization failed! [",
                        _message.getServiceId(), ".",
                        _message.getInstanceId(), ".",
                        _message.getMethodId(), ".",
                        _message.getSessionId(), "]");
                return false;
            }
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId(), _message.getUid(), _message.getGid());

        (_stub->StubType::getStubAdapter().get()->*stubFunctor_)(client, std::move(std::get<InArgIndices_>(_argTuple))..., std::get<OutArgIndices_>(_argTuple)...);
        Message reply = _message.createResponseMessage();

        if (sizeof...(OutArgs_) > 0) {
           OutputStream outputStream(reply, isLittleEndian_);
            if (!SerializableArguments<OutArgs_...>::serialize(outputStream, std::get<OutArgIndices_>(_argTuple)...)) {
                COMMONAPI_ERROR("MethodWithReplyAdapterDispatcher (someip) "
                        " serialization failed! [",
                        reply.getServiceId(), ".",
                        reply.getInstanceId(), ".",
                        reply.getMethodId(), "]");
                return false;
            }

            outputStream.flush();
       }

        return _connection->sendMessage(reply);
    }

    StubFunctor_ stubFunctor_;
    const bool isLittleEndian_;
    const bool isImplemented_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class GetAttributeStubDispatcher: public StubDispatcher<StubClass_> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef void (StubClass_::*LockStubFunctor)(bool);
    typedef const AttributeType_& (StubClass_::*GetStubFunctor)(std::shared_ptr<CommonAPI::ClientId>);
    typedef typename StubClass_::StubAdapterType StubAdapterType;
    typedef typename CommonAPI::Stub<StubAdapterType, typename StubClass_::RemoteEventType> StubType;

    GetAttributeStubDispatcher(LockStubFunctor _lockStubFunctor, GetStubFunctor _getStubFunctor,
            const bool _isLittleEndian, const bool _isImplemented,
            AttributeDepl_ *_depl = nullptr)
        : lockStubFunctor_(_lockStubFunctor), getStubFunctor_(_getStubFunctor),
          isLittleEndian_(_isLittleEndian), isImplemented_(_isImplemented),
          depl_(_depl) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        (void) _remoteEventHandler;
        return sendAttributeValueReply(message, stub, _connection);
    }

 protected:
    inline bool sendAttributeValueReply(const Message &message, const std::shared_ptr<StubClass_>& stub,
      std::shared_ptr<ProxyConnection> _connection) {

        Message reply = message.createResponseMessage();
        OutputStream outputStream(reply, isLittleEndian_);

        std::shared_ptr<ClientId> clientId
            = std::make_shared<ClientId>(message.getClientId(), message.getUid(), message.getGid());

        (stub.get()->*lockStubFunctor_)(true);
        auto deployable = CommonAPI::Deployable<AttributeType_, AttributeDepl_>((stub.get()->*getStubFunctor_)(clientId), depl_);
        (stub.get()->*lockStubFunctor_)(false);

        outputStream << deployable;
        outputStream.flush();

        return _connection->sendMessage(reply);
    }

    LockStubFunctor lockStubFunctor_;
    GetStubFunctor getStubFunctor_;
    const bool isLittleEndian_;
    const bool isImplemented_;
    AttributeDepl_ *depl_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class SetAttributeStubDispatcher: public GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    typedef typename GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::LockStubFunctor LockStubFunctor;
    typedef typename GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef bool (RemoteEventHandlerType::*OnRemoteSetFunctor)(std::shared_ptr<CommonAPI::ClientId>, AttributeType_);
    typedef void (RemoteEventHandlerType::*OnRemoteChangedFunctor)();

    SetAttributeStubDispatcher(LockStubFunctor _lockStubFunctor, GetStubFunctor _getStubFunctor,
            OnRemoteSetFunctor _onRemoteSetFunctor, OnRemoteChangedFunctor _onRemoteChangedFunctor,
            bool _isLittleEndian, bool _isImplemented,
            AttributeDepl_ *_depl = nullptr)
        : GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(
          _lockStubFunctor, _getStubFunctor, _isLittleEndian, _isImplemented, _depl),
          onRemoteSetFunctor_(_onRemoteSetFunctor),
          onRemoteChangedFunctor_(_onRemoteChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub,
            RemoteEventHandlerType* _remoteEventHandler, std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        bool attributeValueChanged;
        if (!setAttributeValue(message, stub, _remoteEventHandler, _connection, attributeValueChanged)) {
            return false;
        }

        if (attributeValueChanged) {
            notifyOnRemoteChanged( _remoteEventHandler);
        }

        return true;
    }

 protected:
    inline bool setAttributeValue(const Message &message,
            const std::shared_ptr<StubClass_>& stub,
            RemoteEventHandlerType* _remoteEventHandler,
            std::shared_ptr<ProxyConnection> _connection,
            bool &attributeValueChanged) {
        InputStream inputStream(message, this->isLittleEndian_);
        CommonAPI::Deployable<AttributeType_, AttributeDepl_> attributeValue(this->depl_);
        inputStream >> attributeValue;
        if (inputStream.hasError()) {
            COMMONAPI_ERROR("CommonAPI::SomeIP::SetAttributeStubDispatcher"
                    " deserialization failed! [",
                    message.getServiceId(), ".",
                    message.getInstanceId(), ".",
                    message.getMethodId(), ".",
                    message.getSessionId(), "]");
            return false;
        }

        std::shared_ptr<ClientId> clientId
            = std::make_shared<ClientId>(message.getClientId(), message.getUid(), message.getGid());

        attributeValueChanged = (_remoteEventHandler->*onRemoteSetFunctor_)(clientId, std::move(attributeValue.getValue()));

        return this->sendAttributeValueReply(message, stub, _connection);
    }

    inline void notifyOnRemoteChanged(RemoteEventHandlerType* _remoteEventHandler) {
        (_remoteEventHandler->*onRemoteChangedFunctor_)();
    }

    inline const AttributeType_& getAttributeValue(std::shared_ptr<CommonAPI::ClientId> clientId, const std::shared_ptr<StubClass_> &stub) {
        return (stub.get()->*(this->getStubFunctor_))(clientId);
    }

    const OnRemoteSetFunctor onRemoteSetFunctor_;
    const OnRemoteChangedFunctor onRemoteChangedFunctor_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class SetObservableAttributeStubDispatcher: public SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef typename StubClass_::StubAdapterType StubAdapterType;

    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::LockStubFunctor LockStubFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef typename CommonAPI::Stub<StubAdapterType, typename StubClass_::RemoteEventType> StubType;
    typedef void (StubAdapterType::*FireChangedFunctor)(const AttributeType_ &);

    SetObservableAttributeStubDispatcher(LockStubFunctor _lockStubFunctor, GetStubFunctor _getStubFunctor,
            OnRemoteSetFunctor _onRemoteSetFunctor, OnRemoteChangedFunctor _onRemoteChangedFunctor,
            FireChangedFunctor _fireChangedFunctor,
            bool _isLittleEndian, bool _isImplemented,
            AttributeDepl_ *_depl = nullptr)
        : SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(
                _lockStubFunctor, _getStubFunctor,
                _onRemoteSetFunctor, _onRemoteChangedFunctor,
                _isLittleEndian, _isImplemented,
                _depl),
          fireChangedFunctor_(_fireChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {
        if (!this->isImplemented_)
            return false;

        bool attributeValueChanged;
        if (!this->setAttributeValue(message, stub, _remoteEventHandler, _connection, attributeValueChanged)) {
            return false;
        }

        if (attributeValueChanged) {
            std::shared_ptr<ClientId> clientId
                = std::make_shared<ClientId>(message.getClientId(), message.getUid(), message.getGid());
            fireAttributeValueChanged(clientId,  stub);
            this->notifyOnRemoteChanged(_remoteEventHandler);
        }
        return true;
    }

 private:
    inline void fireAttributeValueChanged(std::shared_ptr<CommonAPI::ClientId> _client, const std::shared_ptr<StubClass_> _stub) {
        auto stubAdapter = _stub->StubType::getStubAdapter();
        (_stub.get()->*SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::lockStubFunctor_)(true);
        (stubAdapter.get()->*fireChangedFunctor_)(this->getAttributeValue(_client, _stub));
        (_stub.get()->*SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::lockStubFunctor_)(false);
    }

    const FireChangedFunctor fireChangedFunctor_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUBADAPTERHELPER_HPP_
