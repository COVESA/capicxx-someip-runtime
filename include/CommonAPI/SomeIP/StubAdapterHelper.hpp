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

class StubDispatcherBase {
public:
   virtual ~StubDispatcherBase() { }
};

struct AttributeDispatcherStruct {
    StubDispatcherBase* getter;
    StubDispatcherBase* setter;

    AttributeDispatcherStruct(StubDispatcherBase* g, StubDispatcherBase* s) {
        getter = g;
        setter = s;
    }
};

typedef std::unordered_map<std::string, AttributeDispatcherStruct> StubAttributeTable;

template < typename StubClass_ >
class StubAdapterHelper: public virtual StubAdapter {
 public:
    typedef typename StubClass_::StubAdapterType StubAdapterType;
    typedef typename StubClass_::RemoteEventHandlerType RemoteEventHandlerType;

    class StubDispatcher: public StubDispatcherBase {
     public:
        virtual ~StubDispatcher() {}
        virtual bool dispatchMessage(const Message &_message,
                                     const std::shared_ptr<StubClass_> &_stub,
                                     StubAdapterHelper<StubClass_> &_helper) = 0;
    };

    // interfaceMemberName, interfaceMemberSignature
    typedef std::string InterfaceMemberPath;
    typedef std::unordered_map<method_id_t, StubDispatcherBase *> StubDispatcherTable;

 public:
    StubAdapterHelper(const Address &_address,
                      const std::shared_ptr< ProxyConnection > &_connection,
                      const std::shared_ptr< StubClass_ > &_stub)
        : StubAdapter(_address, _connection),
          stub_(_stub),
          remoteEventHandler_(nullptr) {
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

        auto findIterator = getStubDispatcherTable().find(methodId);
        const bool foundInterfaceMemberHandler = (findIterator != getStubDispatcherTable().end());
        if (!foundInterfaceMemberHandler) {
            auto error = message.createErrorResponseMessage(return_code_e::E_UNKNOWN_METHOD);
            connection_->sendMessage(error);
            return true;
        }

        bool isMessageHandled = false;
        //To prevent the destruction of the stub whilst still handling a message
        if (stub_ && foundInterfaceMemberHandler) {
            StubDispatcher *stubDispatcher = static_cast< StubDispatcher * >(findIterator->second);
            isMessageHandled = stubDispatcher->dispatchMessage(message, stub_, *this);
        }

        return isMessageHandled;
    }

    virtual const StubDispatcherTable& getStubDispatcherTable() = 0;

    std::shared_ptr<StubClass_> stub_;
    RemoteEventHandlerType *remoteEventHandler_;
};

template <class>
struct StubEventHelper;

template <template <class ...> class In_, class... InArgs_>
struct StubEventHelper<In_<InArgs_...>> {

    static inline bool sendEvent(const Address &_address,
                                 const event_id_t &_event,
                                 const std::shared_ptr<ProxyConnection> &_connection,
                                 const InArgs_&... _in) {

        Message message = Message::createNotificationMessage(_address, _event, false);
        if (sizeof...(InArgs_) > 0) {
            OutputStream output(message);
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
                          const InArgs_&... _in) {
        return (sendEvent(_stub.getSomeIpAddress(),
                          _event,
                          _stub.getConnection(),
                          _in...));
    }

    template <typename Stub_ = StubAdapter>
    static bool sendEvent(const client_id_t _client,
                          const Stub_ &_stub,
                          const event_id_t &_event,
                          const InArgs_&... _in) {
        Message message = Message::createNotificationMessage(
                _stub.getSomeIpAddress(), _event, false);

        if (sizeof...(InArgs_) > 0) {
            OutputStream output(message);
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
    : public StubAdapterHelper<StubClass_>::StubDispatcher {
public:
    typedef StubAdapterHelper<StubClass_> StubAdapterHelperType;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_...);

    MethodStubDispatcher(
        StubFunctor_ stubFunctor, std::tuple<DeplInArgs_*...> _in)
        : stubFunctor_(stubFunctor) {

        initialize(typename make_sequence_range<sizeof...(DeplInArgs_), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         StubAdapterHelperType &_adapterHelper) {
        return dispatchMessageHelper(
                    _message, _stub, _adapterHelper,
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
                                        StubAdapterHelperType &_adapterHelper,
                                      index_sequence<InArgIndices_...>) {
        (void)_adapterHelper;

        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message);
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
    : public StubAdapterHelper<StubClass_>::StubDispatcher {
public:
    typedef StubAdapterHelper<StubClass_> StubAdapterHelperType;
    typedef std::function<void (OutArgs_...)> ReplyType_t;
    typedef void (StubClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., ReplyType_t);

    MethodWithReplyStubDispatcher(
        StubFunctor_ stubFunctor, std::tuple<DeplInArgs_*...> _in, std::tuple<DeplOutArgs_*...> _out)
        : stubFunctor_(stubFunctor), out_(_out) {

        initialize(typename make_sequence_range<sizeof...(DeplInArgs_), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
                         const std::shared_ptr<StubClass_> &_stub,
                         StubAdapterHelperType &_adapterHelper) {
        connection_ = _adapterHelper.getConnection();
        return dispatchMessageHelper(
                    _message, _stub,
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
                                      index_sequence<InArgIndices_...>,
                                      index_sequence<OutArgIndices_...>) {
        if (!_message.isRequestType()) {
            auto error = _message.createErrorResponseMessage(return_code_e::E_WRONG_MESSAGE_TYPE);
            connection_->sendMessage(error);
            return true;
        }

        if (sizeof...(DeplInArgs_) > 0) {
            InputStream inputStream(_message);
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
                OutputStream output(reply->second);
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
    : public StubAdapterHelper<StubClass_>::StubDispatcher {

public:
    typedef StubAdapterHelper<StubClass_> StubAdapterHelperType;
    typedef void (StubAdapterClass_::*StubFunctor_)(std::shared_ptr<CommonAPI::ClientId>, InArgs_..., OutArgs_&...);
    typedef typename CommonAPI::Stub<typename StubAdapterHelperType::StubAdapterType, typename StubClass_::RemoteEventType> StubType;

    MethodWithReplyAdapterDispatcher(StubFunctor_ stubFunctor)
        : stubFunctor_(stubFunctor) {
    }

    bool dispatchMessage(const Message &_message, const std::shared_ptr<StubClass_> &_stub, StubAdapterHelperType &_adapterHelper) {
        std::tuple<InArgs_..., OutArgs_...> argTuple;
        return dispatchMessageHelper(
                        _message,
                        _stub,
                        _adapterHelper,
                        typename make_sequence_range<sizeof...(InArgs_), 0>::type(),
                        typename make_sequence_range<sizeof...(OutArgs_), sizeof...(InArgs_)>::type(),
                        argTuple);
    }

 private:
    template <int... InArgIndices_, int... OutArgIndices_>
    inline bool dispatchMessageHelper(
                        const Message &_message,
                        const std::shared_ptr<StubClass_> &_stub,
                        StubAdapterHelperType &_adapterHelper,
                        index_sequence<InArgIndices_...>,
                        index_sequence<OutArgIndices_...>,
                        std::tuple<InArgs_..., OutArgs_...> _argTuple) const {

        if (!_message.isRequestType()) {
            auto error = _message.createErrorResponseMessage(return_code_e::E_WRONG_MESSAGE_TYPE);
            _adapterHelper.getConnection()->sendMessage(error);
            return true;
        } 

       if (sizeof...(InArgs_) > 0) {
            InputStream inputStream(_message);
            if (!SerializableArguments<InArgs_...>::deserialize(inputStream, std::get<InArgIndices_>(_argTuple)...))
                return false;
        }

        std::shared_ptr<ClientId> client
            = std::make_shared<ClientId>(_message.getClientId());

        (_stub->StubType::getStubAdapter().get()->*stubFunctor_)(client, std::move(std::get<InArgIndices_>(_argTuple))..., std::get<OutArgIndices_>(_argTuple)...);
        Message reply = _message.createResponseMessage();

        if (sizeof...(OutArgs_) > 0) {
           OutputStream outputStream(reply);
            if (!SerializableArguments<OutArgs_...>::serialize(outputStream, std::get<OutArgIndices_>(_argTuple)...))
                return false;

            outputStream.flush();
       }

        return _adapterHelper.getConnection()->sendMessage(reply);
    }

    StubFunctor_ stubFunctor_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class GetAttributeStubDispatcher: public StubAdapterHelper<StubClass_>::StubDispatcher {
 public:
    typedef StubAdapterHelper<StubClass_> StubAdapterHelperType;
    typedef const AttributeType_& (StubClass_::*GetStubFunctor)(std::shared_ptr<CommonAPI::ClientId>);

    GetAttributeStubDispatcher(GetStubFunctor getStubFunctor, AttributeDepl_ *_depl = nullptr)
        : getStubFunctor_(getStubFunctor), depl_(_depl) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub, StubAdapterHelperType &stubAdapterHelper) {
        return sendAttributeValueReply(message, stub, stubAdapterHelper);
    }

 protected:
    inline bool sendAttributeValueReply(const Message &message, const std::shared_ptr<StubClass_>& stub, StubAdapterHelperType& stubAdapterHelper) {
        Message reply = message.createResponseMessage();
        OutputStream outputStream(reply);

        std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());

        outputStream << CommonAPI::Deployable<AttributeType_, AttributeDepl_>((stub.get()->*getStubFunctor_)(clientId), depl_);
        outputStream.flush();

        return stubAdapterHelper.getConnection()->sendMessage(reply);
    }

    GetStubFunctor getStubFunctor_;
    AttributeDepl_ *depl_;
};


template <typename StubClass_, typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class SetAttributeStubDispatcher: public GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_> {
public:
    typedef typename GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::StubAdapterHelperType StubAdapterHelperType;
    typedef typename StubAdapterHelperType::RemoteEventHandlerType RemoteEventHandlerType;

    typedef typename GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef bool (RemoteEventHandlerType::*OnRemoteSetFunctor)(std::shared_ptr<CommonAPI::ClientId>, AttributeType_);
    typedef void (RemoteEventHandlerType::*OnRemoteChangedFunctor)();

    SetAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                               OnRemoteSetFunctor onRemoteSetFunctor,
                               OnRemoteChangedFunctor onRemoteChangedFunctor,
                               AttributeDepl_ *_depl = nullptr)
        : GetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(getStubFunctor, _depl),
          onRemoteSetFunctor_(onRemoteSetFunctor),
          onRemoteChangedFunctor_(onRemoteChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub, StubAdapterHelperType &stubAdapterHelper) {
        bool attributeValueChanged;

        if (!setAttributeValue(message, stub, stubAdapterHelper, attributeValueChanged)) {
            return false;
        }

        if (attributeValueChanged) {
            notifyOnRemoteChanged(stubAdapterHelper);
        }

        return true;
    }

 protected:
    inline bool setAttributeValue(const Message &message,
                                  const std::shared_ptr<StubClass_>& stub,
                                  StubAdapterHelperType& stubAdapterHelper,
                                  bool& attributeValueChanged) {
        InputStream inputStream(message);
        CommonAPI::Deployable<AttributeType_, AttributeDepl_> attributeValue(this->depl_);
        inputStream >> attributeValue;
        if (inputStream.hasError()) {
            return false;
        }

        std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());

        attributeValueChanged = (stubAdapterHelper.getRemoteEventHandler()->*onRemoteSetFunctor_)(clientId, std::move(attributeValue.getValue()));

        return this->sendAttributeValueReply(message, stub, stubAdapterHelper);
    }

    inline void notifyOnRemoteChanged(StubAdapterHelperType& stubAdapterHelper) {
        (stubAdapterHelper.getRemoteEventHandler()->*onRemoteChangedFunctor_)();
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
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::StubAdapterHelperType StubAdapterHelperType;
    typedef typename StubAdapterHelperType::StubAdapterType StubAdapterType;

    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::GetStubFunctor GetStubFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef typename CommonAPI::Stub<StubAdapterType, typename StubClass_::RemoteEventType> StubType;
    typedef void (StubAdapterType::*FireChangedFunctor)(const AttributeType_&);

    SetObservableAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                                         OnRemoteSetFunctor onRemoteSetFunctor,
                                         OnRemoteChangedFunctor onRemoteChangedFunctor,
                                         FireChangedFunctor fireChangedFunctor,
                                         AttributeDepl_ *_depl = nullptr)
        : SetAttributeStubDispatcher<StubClass_, AttributeType_, AttributeDepl_>(getStubFunctor,
                                                                 onRemoteSetFunctor,
                                                                 onRemoteChangedFunctor,
                                                                 _depl),
                    fireChangedFunctor_(fireChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<StubClass_> &stub, StubAdapterHelperType &stubAdapterHelper) {
        bool attributeValueChanged;
        if (!this->setAttributeValue(message, stub, stubAdapterHelper, attributeValueChanged)) {
            return false;
        }

        if (attributeValueChanged) {
            std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());
            fireAttributeValueChanged(clientId, stubAdapterHelper, stub);
            this->notifyOnRemoteChanged(stubAdapterHelper);
        }
        return true;
    }

 private:
    inline void fireAttributeValueChanged(std::shared_ptr<CommonAPI::ClientId> _client, StubAdapterHelperType &_helper, const std::shared_ptr<StubClass_> _stub) {
        (void)_helper;
        (_stub->StubType::getStubAdapter().get()->*fireChangedFunctor_)(this->getAttributeValue(_client, _stub));
    }

    const FireChangedFunctor fireChangedFunctor_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUB_ADAPTER_HELPER_HPP_
