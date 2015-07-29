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

template < typename _StubClass >
class StubAdapterHelper: public virtual StubAdapter {
 public:
    typedef typename _StubClass::StubAdapterType StubAdapterType;
    typedef typename _StubClass::RemoteEventHandlerType RemoteEventHandlerType;

    class StubDispatcher: public StubDispatcherBase {
     public:
		virtual ~StubDispatcher() {}
        virtual bool dispatchMessage(const Message &_message,
        							 const std::shared_ptr<_StubClass> &_stub,
        							 StubAdapterHelper<_StubClass> &_helper) = 0;
    };

    // interfaceMemberName, interfaceMemberSignature
    typedef std::string InterfaceMemberPath;
    typedef std::unordered_map<method_id_t, StubDispatcherBase *> StubDispatcherTable;

 public:
    StubAdapterHelper(const Address &_address,
                      const std::shared_ptr< ProxyConnection > &_connection,
                      const std::shared_ptr< _StubClass > &_stub)
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
        bool isMessageHandled = false;

        //To prevent the destruction of the stub whilst still handling a message
        if (stub_ && foundInterfaceMemberHandler) {
            StubDispatcher *stubDispatcher = static_cast< StubDispatcher * >(findIterator->second);
            isMessageHandled = stubDispatcher->dispatchMessage(message, stub_, *this);
        }

        return isMessageHandled;
    }

    virtual const StubDispatcherTable& getStubDispatcherTable() = 0;

    std::shared_ptr<_StubClass> stub_;
    RemoteEventHandlerType *remoteEventHandler_;
};

template <class>
struct StubEventHelper;

template <template <class ...> class _In, class... _InArgs>
struct StubEventHelper<_In<_InArgs...>> {

    static inline bool sendEvent(const Address &_address,
                                 const event_id_t &_event,
                                 const std::shared_ptr<ProxyConnection> &_connection,
                                 const _InArgs&... _in) {

        Message message = Message::createNotificationMessage(_address, _event, false);

        if (sizeof...(_InArgs) > 0) {
            OutputStream output(message);
            if (!SerializableArguments<_InArgs...>::serialize(output, _in...)) {
                COMMONAPI_ERROR("CommonAPI::SomeIP::StubEventHelper: serialization failed!");
                return false;
            }
            output.flush();
        }

        return _connection->sendEvent(message);
    }

    template <typename _Stub = StubAdapter>
    static bool sendEvent(const _Stub &_stub,
                          const event_id_t &_event,
                          const _InArgs&... _in) {
        return (sendEvent(_stub.getSomeIpAddress(),
                          _event,
                          _stub.getConnection(),
                          _in...));
    }

    template <typename _Stub = StubAdapter>
    static bool sendEvent(const client_id_t _client,
    					  const _Stub &_stub,
                          const event_id_t &_event,
                          const _InArgs&... _in) {

        Message message = Message::createNotificationMessage(
                _stub.getSomeIpAddress(), _event, false);

        if (sizeof...(_InArgs) > 0) {
            OutputStream output(message);
            if (!SerializableArguments<_InArgs...>::serialize(output, _in...)) {
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
    typename _StubClass,
    template <class...> class _In, class... _InArgs,
	template <class...> class _DeplIn, class... _DeplInArgs>
class MethodStubDispatcher<_StubClass, _In<_InArgs...>, _DeplIn<_DeplInArgs...>>
	: public StubAdapterHelper<_StubClass>::StubDispatcher {
public:
    typedef StubAdapterHelper<_StubClass> StubAdapterHelperType;
    typedef void (_StubClass::*_StubFunctor)(std::shared_ptr<CommonAPI::ClientId>, _InArgs...);

    MethodStubDispatcher(
    	_StubFunctor stubFunctor, std::tuple<_DeplInArgs*...> _in)
        : stubFunctor_(stubFunctor) {

    	initialize(typename make_sequence_range<sizeof...(_DeplInArgs), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
    					 const std::shared_ptr<_StubClass> &_stub,
						 StubAdapterHelperType &_adapterHelper) {
        return dispatchMessageHelper(
                    _message, _stub, _adapterHelper,
                    typename make_sequence_range<sizeof...(_InArgs), 0>::type());
    }

private:
	template <int... _DeplInArgIndices>
	inline void initialize(index_sequence<_DeplInArgIndices...>, std::tuple<_DeplInArgs*...> &_in) {
		in_ = std::make_tuple(std::get<_DeplInArgIndices>(_in)...);
	}

    template <int... _InArgIndices>
    inline bool dispatchMessageHelper(const Message &_message,
                              		  const std::shared_ptr<_StubClass> &_stub,
                              		  StubAdapterHelperType &_adapterHelper,
									  index_sequence<_InArgIndices...>) {

        if (sizeof...(_DeplInArgs) > 0) {
            InputStream inputStream(_message);
			if (!SerializableArguments<CommonAPI::Deployable<_InArgs, _DeplInArgs>...>::deserialize(
					inputStream, std::get<_InArgIndices>(in_)...))
                return false;
        }

        std::shared_ptr<ClientId> client
			= std::make_shared<ClientId>(_message.getClientId());

        (_stub.get()->*stubFunctor_)(
        	client,
        	std::move(std::get<_InArgIndices>(in_))...
        );

       	return true;
	}

    _StubFunctor stubFunctor_;

    std::tuple<CommonAPI::Deployable<_InArgs, _DeplInArgs>...> in_;
};


template<class, class, class, class, class>
class MethodWithReplyStubDispatcher;

template <
    typename _StubClass,
    template <class...> class _In, class... _InArgs,
    template <class...> class _Out, class... _OutArgs,
	template <class...> class _DeplIn, class... _DeplInArgs,
	template <class...> class _DeplOut, class... _DeplOutArgs>
class MethodWithReplyStubDispatcher<_StubClass, _In<_InArgs...>, _Out<_OutArgs...>, _DeplIn<_DeplInArgs...>, _DeplOut<_DeplOutArgs...>>
	: public StubAdapterHelper<_StubClass>::StubDispatcher {
public:
    typedef StubAdapterHelper<_StubClass> StubAdapterHelperType;
    typedef std::function<void (_OutArgs...)> ReplyType_t;
    typedef void (_StubClass::*_StubFunctor)(std::shared_ptr<CommonAPI::ClientId>, _InArgs..., ReplyType_t);

    MethodWithReplyStubDispatcher(
    	_StubFunctor stubFunctor, std::tuple<_DeplInArgs*...> _in, std::tuple<_DeplOutArgs*...> _out)
        : stubFunctor_(stubFunctor), out_(_out) {

    	initialize(typename make_sequence_range<sizeof...(_DeplInArgs), 0>::type(), _in);
    }

    bool dispatchMessage(const Message &_message,
    					 const std::shared_ptr<_StubClass> &_stub,
						 StubAdapterHelperType &_adapterHelper) {
		connection_ = _adapterHelper.getConnection();
        return dispatchMessageHelper(
                    _message, _stub, _adapterHelper,
                    typename make_sequence_range<sizeof...(_InArgs), 0>::type(),
                    typename make_sequence_range<sizeof...(_OutArgs), 0>::type());
    }

	bool sendReplyMessage(CommonAPI::CallId_t _call,
						  std::tuple<CommonAPI::Deployable<_OutArgs, _DeplOutArgs>...> _args = std::make_tuple()) {
		return sendReplyMessageHelper(_call, typename make_sequence_range<sizeof...(_OutArgs), 0>::type(), _args);
	}

private:
	template <int... _DeplInArgIndices>
	inline void initialize(index_sequence<_DeplInArgIndices...>, std::tuple<_DeplInArgs*...> &_in) {
		in_ = std::make_tuple(std::get<_DeplInArgIndices>(_in)...);
	}

    template <int... _InArgIndices, int... _OutArgIndices>
    inline bool dispatchMessageHelper(const Message &_message,
                              		  const std::shared_ptr<_StubClass> &_stub,
                              		  StubAdapterHelperType &_adapterHelper,
		                              index_sequence<_InArgIndices...>,
        		                      index_sequence<_OutArgIndices...>) {

        if (sizeof...(_DeplInArgs) > 0) {
            InputStream inputStream(_message);
			if (!SerializableArguments<CommonAPI::Deployable<_InArgs, _DeplInArgs>...>::deserialize(
					inputStream, std::get<_InArgIndices>(in_)...))
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
        	std::move(std::get<_InArgIndices>(in_).getValue())...,
        	[call, this](_OutArgs... _args) {
        		this->sendReplyMessage(
        			call,
        			std::make_tuple(
        				CommonAPI::Deployable<_OutArgs, _DeplOutArgs>(
        					_args, std::get<_OutArgIndices>(out_)
						)...
					)
        		);
        	}
        );

        return true;
	}

    template<int... _OutArgIndices>
    bool sendReplyMessageHelper(CommonAPI::CallId_t _call,
    					   	    index_sequence<_OutArgIndices...>,
								std::tuple<CommonAPI::Deployable<_OutArgs, _DeplOutArgs>...> _args) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto reply = pending_.find(_call);
		if (reply != pending_.end()) {
			if (sizeof...(_DeplOutArgs) > 0) {
				OutputStream output(reply->second);
				if (!SerializableArguments<CommonAPI::Deployable<_OutArgs, _DeplOutArgs>...>::serialize(
						output, std::get<_OutArgIndices>(_args)...)) {
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

    _StubFunctor stubFunctor_;

    std::tuple<CommonAPI::Deployable<_InArgs, _DeplInArgs>...> in_;
    std::tuple<_DeplOutArgs*...> out_;

	CommonAPI::CallId_t currentCall_;
	std::map<CommonAPI::CallId_t, Message> pending_;
	std::mutex mutex_; // protects pending_

	std::shared_ptr<ProxyConnection> connection_;
};

template<class, class, class, class>
class MethodWithReplyAdapterDispatcher;

template <
    typename _StubClass,
    typename _StubAdapterClass,
    template <class...> class _In, class... _InArgs,
    template <class...> class _Out, class... _OutArgs>
class MethodWithReplyAdapterDispatcher<_StubClass, _StubAdapterClass, _In<_InArgs...>, _Out<_OutArgs...>>
    : public StubAdapterHelper<_StubClass>::StubDispatcher {

public:
    typedef StubAdapterHelper<_StubClass> StubAdapterHelperType;
    typedef void (_StubAdapterClass::*_StubFunctor)(std::shared_ptr<CommonAPI::ClientId>, _InArgs..., _OutArgs&...);
    typedef typename CommonAPI::Stub<typename StubAdapterHelperType::StubAdapterType, typename _StubClass::RemoteEventType> StubType;

    MethodWithReplyAdapterDispatcher(_StubFunctor stubFunctor)
        : stubFunctor_(stubFunctor) {
    }

    bool dispatchMessage(const Message &_message, const std::shared_ptr<_StubClass> &_stub, StubAdapterHelperType &_adapterHelper) {
		std::tuple<_InArgs..., _OutArgs...> argTuple;
        return dispatchMessageHelper(
                        _message,
                        _stub,
                        _adapterHelper,
                        typename make_sequence_range<sizeof...(_InArgs), 0>::type(),
                        typename make_sequence_range<sizeof...(_OutArgs), sizeof...(_InArgs)>::type(),
						argTuple);
    }

 private:
    template <int... _InArgIndices, int... _OutArgIndices>
    inline bool dispatchMessageHelper(
						const Message &_message,
                        const std::shared_ptr<_StubClass> &_stub,
                        StubAdapterHelperType &_adapterHelper,
                        index_sequence<_InArgIndices...>,
                        index_sequence<_OutArgIndices...>,
						std::tuple<_InArgs..., _OutArgs...> _argTuple) const {
        if (sizeof...(_InArgs) > 0) {
            InputStream inputStream(_message);
			if (!SerializableArguments<_InArgs...>::deserialize(inputStream, std::get<_InArgIndices>(_argTuple)...))
                return false;
        }

        std::shared_ptr<ClientId> client
			= std::make_shared<ClientId>(_message.getClientId());

        (_stub->StubType::getStubAdapter().get()->*stubFunctor_)(client, std::move(std::get<_InArgIndices>(_argTuple))..., std::get<_OutArgIndices>(_argTuple)...);
        Message reply = _message.createResponseMessage();

        if (sizeof...(_OutArgs) > 0) {
           OutputStream outputStream(reply);
            if (!SerializableArguments<_OutArgs...>::serialize(outputStream, std::get<_OutArgIndices>(_argTuple)...))
                return false;

            outputStream.flush();
       }

        return _adapterHelper.getConnection()->sendMessage(reply);
    }

    _StubFunctor stubFunctor_;
};


template <typename _StubClass, typename _AttributeType, typename _AttributeDepl = EmptyDeployment>
class GetAttributeStubDispatcher: public StubAdapterHelper<_StubClass>::StubDispatcher {
 public:
    typedef StubAdapterHelper<_StubClass> StubAdapterHelperType;
    typedef const _AttributeType& (_StubClass::*GetStubFunctor)(std::shared_ptr<CommonAPI::ClientId>);

    GetAttributeStubDispatcher(GetStubFunctor getStubFunctor, _AttributeDepl *_depl = nullptr)
        : getStubFunctor_(getStubFunctor), depl_(_depl) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<_StubClass> &stub, StubAdapterHelperType &stubAdapterHelper) {
        return sendAttributeValueReply(message, stub, stubAdapterHelper);
    }

 protected:
    inline bool sendAttributeValueReply(const Message &message, const std::shared_ptr<_StubClass>& stub, StubAdapterHelperType& stubAdapterHelper) {
        Message reply = message.createResponseMessage();
        OutputStream outputStream(reply);

        std::shared_ptr<ClientId> clientId = std::make_shared<ClientId>(message.getClientId());

        outputStream << CommonAPI::Deployable<_AttributeType, _AttributeDepl>((stub.get()->*getStubFunctor_)(clientId), depl_);
        outputStream.flush();

        return stubAdapterHelper.getConnection()->sendMessage(reply);
    }

    GetStubFunctor getStubFunctor_;
    _AttributeDepl *depl_;
};


template <typename _StubClass, typename _AttributeType, typename _AttributeDepl = EmptyDeployment>
class SetAttributeStubDispatcher: public GetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl> {
public:
    typedef typename GetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>::StubAdapterHelperType StubAdapterHelperType;
    typedef typename StubAdapterHelperType::RemoteEventHandlerType RemoteEventHandlerType;

    typedef typename GetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>::GetStubFunctor GetStubFunctor;
    typedef bool (RemoteEventHandlerType::*OnRemoteSetFunctor)(std::shared_ptr<CommonAPI::ClientId>, _AttributeType);
    typedef void (RemoteEventHandlerType::*OnRemoteChangedFunctor)();

    SetAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                               OnRemoteSetFunctor onRemoteSetFunctor,
                               OnRemoteChangedFunctor onRemoteChangedFunctor,
                               _AttributeDepl *_depl = nullptr)
        : GetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>(getStubFunctor, _depl),
          onRemoteSetFunctor_(onRemoteSetFunctor),
          onRemoteChangedFunctor_(onRemoteChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<_StubClass> &stub, StubAdapterHelperType &stubAdapterHelper) {
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
                                  const std::shared_ptr<_StubClass>& stub,
                                  StubAdapterHelperType& stubAdapterHelper,
                                  bool& attributeValueChanged) {
        InputStream inputStream(message);
        CommonAPI::Deployable<_AttributeType, _AttributeDepl> attributeValue(this->depl_);
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

    inline const _AttributeType& getAttributeValue(std::shared_ptr<CommonAPI::ClientId> clientId, const std::shared_ptr<_StubClass> &stub) {
        return (stub.get()->*(this->getStubFunctor_))(clientId);
    }

    const OnRemoteSetFunctor onRemoteSetFunctor_;
    const OnRemoteChangedFunctor onRemoteChangedFunctor_;
};


template <typename _StubClass, typename _AttributeType, typename _AttributeDepl = EmptyDeployment>
class SetObservableAttributeStubDispatcher: public SetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl> {
 public:
    typedef typename SetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>::StubAdapterHelperType StubAdapterHelperType;
    typedef typename StubAdapterHelperType::StubAdapterType StubAdapterType;

    typedef typename SetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>::GetStubFunctor GetStubFunctor;
    typedef typename SetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>::OnRemoteSetFunctor OnRemoteSetFunctor;
    typedef typename SetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>::OnRemoteChangedFunctor OnRemoteChangedFunctor;
    typedef typename CommonAPI::Stub<StubAdapterType, typename _StubClass::RemoteEventType> StubType;
    typedef void (StubAdapterType::*FireChangedFunctor)(const _AttributeType&);

    SetObservableAttributeStubDispatcher(GetStubFunctor getStubFunctor,
                                         OnRemoteSetFunctor onRemoteSetFunctor,
                                         OnRemoteChangedFunctor onRemoteChangedFunctor,
                                         FireChangedFunctor fireChangedFunctor,
                                         _AttributeDepl *_depl = nullptr)
        : SetAttributeStubDispatcher<_StubClass, _AttributeType, _AttributeDepl>(getStubFunctor,
                                                                 onRemoteSetFunctor,
                                                                 onRemoteChangedFunctor,
                                                                 _depl),
                    fireChangedFunctor_(fireChangedFunctor) {
    }

    bool dispatchMessage(const Message &message, const std::shared_ptr<_StubClass> &stub, StubAdapterHelperType &stubAdapterHelper) {
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
    inline void fireAttributeValueChanged(std::shared_ptr<CommonAPI::ClientId> clientId, StubAdapterHelperType& stubAdapterHelper, const std::shared_ptr<_StubClass> stub) {
        (stub->StubType::getStubAdapter().get()->*fireChangedFunctor_)(this->getAttributeValue(clientId, stub));
    }

    const FireChangedFunctor fireChangedFunctor_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUB_ADAPTER_HELPER_HPP_
