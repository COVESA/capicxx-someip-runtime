// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_STUB_ADAPTER_HELPER_HPP_
#define COMMONAPI_SOMEIP_STUB_ADAPTER_HELPER_HPP_

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
                                 RemoteEventHandlerType* _remoteEventHandler,
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
        //To prevent the destruction of the stub whilst still handling a message
        if (stub_ && foundInterfaceMemberHandler) {
            StubDispatcher<StubClass_> *stubDispatcher = findIterator->second;
            isMessageHandled = stubDispatcher->dispatchMessage(message, stub_, getRemoteEventHandler(), getConnection());
            if( !isMessageHandled) {
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
                COMMONAPI_ERROR("CommonAPI::SomeIP::StubEventHelper: serialization failed!");

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
                COMMONAPI_ERROR("CommonAPI::SomeIP::StubEventHelper 2: serialization failed!");
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
        StubFunctor_ stubFunctor, bool _isLittleEndian, std::tuple<DeplInArgs_*...> _in)
        : stubFunctor_(stubFunctor), isLittleEndian_(_isLittleEndian) {

        initialize(typename make_sequence_range<sizeof...(DeplInArgs_), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         RemoteEventHandlerType* _remoteEventHandler,
                         std::shared_ptr<ProxyConnection> _connection) {
        return dispatchMessageHelper(
                    _message, _stub, _remoteEventHandler, _connection,
                    typename make_sequence_range<sizeof...(InArgs_), 0>::type());
    }

private:
    template <int... DeplInArgIndices_>
    inline void initialize(index_sequence<DeplInArgIndices_...>, std::tuple<DeplInArgs_*...> &_in) {
        in_ = std::make_tuple(std::get<DeplInArgIndices_>(_in)...);
    }

    template <int... InArgIndices_>
    inline bool dispatchMessageHelper(const Message &_message,
                                        const std::shared_ptr<StubClass_> &_stub,
                                        RemoteEventHandlerType* _remoteEventHandler,
                                        std::shared_ptr<ProxyConnection> _connection,
                                      index_sequence<InArgIndices_...>) {
        (void)_remoteEventHandler;
        (void)_connection;

        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message, isLittleEndian_);
            if (!SerializableArguments<CommonAPI::Deployable<InArgs_, DeplInArgs_>...>::deserialize(
                    inputStream, std::get<InArgIndices_>(in_)...))
                return false;
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId());

        (_stub.get()->*stubFunctor_)(
            client,
            std::move(std::get<InArgIndices_>(in_))...
        );

           return true;
    }

    StubFunctor_ stubFunctor_;
    bool isLittleEndian_;

    std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in_;
};


template<class, class, class, class, class>
class MethodWithReplyStubDispatcher;

template <
    typename StubClass_,
    template <class...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_,
    template <class...> class DeplIn_, class... DeplInArgs_,
    template <class...> class DeplOut_, class... DeplOutArgs_>
class MethodWithReplyStubDispatcher<StubClass_, In_<InArgs_...>, Out_<OutArgs_...>, DeplIn_<DeplInArgs_...>, DeplOut_<DeplOutArgs_...>>
    : public StubDispatcher<StubClass_> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., ReplyType_t);

    MethodWithReplyStubDispatcher(
        StubFunctor_ stubFunctor, bool _isLittleEndian, std::tuple<DeplInArgs_*...> _in, std::tuple<DeplOutArgs_*...> _out)
        : stubFunctor_(stubFunctor), isLittleEndian_(_isLittleEndian), out_(_out) {

        initialize(typename make_sequence_range<sizeof...(DeplInArgs_), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         RemoteEventHandlerType* _remoteEventHandler,
                         std::shared_ptr<ProxyConnection> _connection) {
        connection_ = _connection;
        return dispatchMessageHelper(
                    _message, _stub, _remoteEventHandler, _connection,
                    typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                    typename make_sequence_range<sizeof...(OutArgs_), 0>::type());
    }

    bool sendReplyMessage(CommonAPI::CallId_t _call,
                          std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> _args = std::make_tuple()) {
        return sendReplyMessageHelper(_call, typename make_sequence_range<sizeof...(OutArgs_), 0>::type(), _args);
    }

private:
    template <int... DeplInArgIndices_>
    inline void initialize(index_sequence<DeplInArgIndices_...>, std::tuple<DeplInArgs_*...> &_in) {
        in_ = std::make_tuple(std::get<DeplInArgIndices_>(_in)...);
    }

    template <int... InArgIndices_, int... OutArgIndices_>
    inline bool dispatchMessageHelper(const Message &_message,
                                        const std::shared_ptr<StubClass_> &_stub,
                                        RemoteEventHandlerType* _remoteEventHandler,
                                        std::shared_ptr<ProxyConnection> _connection,
                                      index_sequence<InArgIndices_...>,
                                      index_sequence<OutArgIndices_...>) {
        (void) _remoteEventHandler;
        (void) _connection;

        if (!_message.isRequestType()) {
            auto error = _message.createErrorResponseMessage(return_code_e::E_WRONG_MESSAGE_TYPE);
            connection_->sendMessage(error);
            return true;
        }

        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message, isLittleEndian_);
            if (!SerializableArguments<CommonAPI::Deployable<InArgs_, DeplInArgs_>...>::deserialize(
                    inputStream, std::get<InArgIndices_>(in_)...))
                return false;
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId());
        Message reply = _message.createResponseMessage();

        CommonAPI::CallId_t call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            call = currentCall_++;
            pending_[call] = reply;
        }

        // Call the stub function with the list of deserialized in-Parameters
        // and a lambda function which holds the call identifier the deployments
        // for the out-Parameters and extracts them from the deployables before
        // calling the send function.
        (_stub.get()->*stubFunctor_)(
            client,
            std::move(std::get<InArgIndices_>(in_).getValue())...,
            [call, this](OutArgs_... _args) {
                this->sendReplyMessage(
                    call,
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

    template<int... OutArgIndices_>
    bool sendReplyMessageHelper(CommonAPI::CallId_t _call,
                                   index_sequence<OutArgIndices_...>,
                                std::tuple<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...> _args) {
        (void)_args;

        std::lock_guard<std::mutex> lock(mutex_);
        auto reply = pending_.find(_call);
        if (reply != pending_.end()) {
            if (sizeof...(DeplOutArgs_) > 0) {
                OutputStream output(reply->second, isLittleEndian_);
                if (!SerializableArguments<CommonAPI::Deployable<OutArgs_, DeplOutArgs_>...>::serialize(
                        output, std::get<OutArgIndices_>(_args)...)) {
                    pending_.erase(_call);
                    return false;
                }
                output.flush();
            }
        } else {
            return false;
        }
        bool isSuccessful = connection_->sendMessage(reply->second);
        pending_.erase(_call);
        return isSuccessful;
    }

    StubFunctor_ stubFunctor_;
    bool isLittleEndian_;

    std::tuple<CommonAPI::Deployable<InArgs_, DeplInArgs_>...> in_;
    std::tuple<DeplOutArgs_*...> out_;

    CommonAPI::CallId_t currentCall_;
    std::map<CommonAPI::CallId_t, Message> pending_;
    std::mutex mutex_; // protects pending_

    std::shared_ptr<ProxyConnection> connection_;
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

    MethodWithReplyAdapterDispatcher(StubFunctor_ stubFunctor, bool _isLittleEndian)
        : stubFunctor_(stubFunctor), isLittleEndian_(_isLittleEndian) {
    }

    bool dispatchMessage(const Message &_message,
      const std::shared_ptr<StubClass_> &_stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {

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
    template <int... InArgIndices_, int... OutArgIndices_>
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
            if (!SerializableArguments<InArgs_...>::deserialize(inputStream, std::get<InArgIndices_>(_argTuple)...))
                return false;
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId());

        (_stub->StubType::getStubAdapter().get()->*stubFunctor_)(client, std::move(std::get<InArgIndices_>(_argTuple))..., std::get<OutArgIndices_>(_argTuple)...);
        Message reply = _message.createResponseMessage();

        if (sizeof...(OutArgs_) > 0) {
           OutputStream outputStream(reply, isLittleEndian_);
            if (!SerializableArguments<OutArgs_...>::serialize(outputStream, std::get<OutArgIndices_>(_argTuple)...))
                return false;

            outputStream.flush();
       }

        return _connection->sendMessage(reply);
    }

    StubFunctor_ stubFunctor_;
    bool isLittleEndian_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class GetAttributeStubDispatcher: public StubDispatcher<StubClass_> {
 public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;
    typedef const AttributeType_& (StubClass_::*GetStubFunctor)(std::shared_ptr<CommonAPI::ClientId>);

    GetAttributeStubDispatcher(GetStubFunctor getStubFunctor, bool _isLittleEndian, AttributeDepl_ *_depl = nullptr)
        : getStubFunctor_(getStubFunctor), isLittleEndian_(_isLittleEndian), depl_(_depl) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {
        (void) _remoteEventHandler;
        return sendAttributeValueReply(message, stub, _connection);
    }

 protected:
    inline bool sendAttributeValueReply(const Message &message, const std::shared_ptr<StubClass_>& stub,
      std::shared_ptr<ProxyConnection> _connection) {

        Message reply = message.createResponseMessage();
        OutputStream outputStream(reply, isLittleEndian_);

        std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());

        outputStream << CommonAPI::Deployable<AttributeType_, AttributeDepl_>((stub.get()->*getStubFunctor_)(clientId), depl_);
        outputStream.flush();

        return _connection->sendMessage(reply);
    }

    GetStubFunctor getStubFunctor_;
    bool isLittleEndian_;
    AttributeDepl_ *depl_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class SetAttributeStubDispatcher: public GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    typedef typename GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef bool (RemoteEventHandlerType::*OnRemoteSetFunctor)(std::shared_ptr<CommonAPI::ClientId>, AttributeType_);
    typedef void (RemoteEventHandlerType::*OnRemoteChangedFunctor)();

    SetAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                               OnRemoteSetFunctor onRemoteSetFunctor,
                               OnRemoteChangedFunctor onRemoteChangedFunctor,
                               bool _isLittleEndian,
                               AttributeDepl_ *_depl = nullptr)
        : GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(getStubFunctor, _isLittleEndian, _depl),
          onRemoteSetFunctor_(onRemoteSetFunctor),
          onRemoteChangedFunctor_(onRemoteChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {
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
                                  bool& attributeValueChanged) {
        InputStream inputStream(message, this->isLittleEndian_);
        CommonAPI::Deployable<AttributeType_, AttributeDepl_> attributeValue(this->depl_);
        inputStream >> attributeValue;
        if (inputStream.hasError()) {
            return false;
        }

        std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());

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

    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef typename CommonAPI::Stub<StubAdapterType, typename StubClass_::RemoteEventType> StubType;
    typedef void (StubAdapterType::*FireChangedFunctor)(const AttributeType_&);

    SetObservableAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                                         OnRemoteSetFunctor onRemoteSetFunctor,
                                         OnRemoteChangedFunctor onRemoteChangedFunctor,
                                         FireChangedFunctor fireChangedFunctor,
                                         bool _isLittleEndian,
                                         AttributeDepl_ *_depl = nullptr)
        : SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(getStubFunctor,
                                                                 onRemoteSetFunctor,
                                                                 onRemoteChangedFunctor,
                                                                 _isLittleEndian,
                                                                 _depl),
                    fireChangedFunctor_(fireChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub,
      RemoteEventHandlerType* _remoteEventHandler,
      std::shared_ptr<ProxyConnection> _connection) {
        bool attributeValueChanged;
        if (!this->setAttributeValue(message, stub, _remoteEventHandler, _connection, attributeValueChanged)) {
            return false;
        }

        if (attributeValueChanged) {
            std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());
            fireAttributeValueChanged(clientId,  stub);
            this->notifyOnRemoteChanged(_remoteEventHandler);
        }
        return true;
    }

 private:
    inline void fireAttributeValueChanged(std::shared_ptr<CommonAPI::ClientId> _client, const std::shared_ptr<StubClass_> _stub) {
        (_stub->StubType::getStubAdapter().get()->*fireChangedFunctor_)(this->getAttributeValue(_client, _stub));
    }

    const FireChangedFunctor fireChangedFunctor_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUB_ADAPTER_HELPER_HPP_
