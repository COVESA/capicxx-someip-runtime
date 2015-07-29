// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_ATTRIBUTE_HPP_
#define COMMONAPI_SOMEIP_ATTRIBUTE_HPP_

#include <cassert>
#include <cstdint>
#include <tuple>

#include <CommonAPI/SomeIP/Constants.hpp>
#include <CommonAPI/SomeIP/Event.hpp>
#include <CommonAPI/SomeIP/ProxyHelper.hpp>

namespace CommonAPI {
namespace SomeIP {

template <typename _AttributeType, typename _AttributeDepl = EmptyDeployment>
class ReadonlyAttribute: public _AttributeType {
public:
    typedef typename _AttributeType::ValueType ValueType;
    typedef _AttributeDepl ValueTypeDepl;
    typedef typename _AttributeType::AttributeAsyncCallback AttributeAsyncCallback;

    ReadonlyAttribute(Proxy &_proxy,
                      const method_id_t _getMethodId,
                      const bool _getReliable,
                      _AttributeDepl *_depl = nullptr)
        : proxy_(_proxy),
          getMethodId_(_getMethodId),
          getReliable_(_getReliable),
          depl_(_depl) {
    }

    void getValue(CallStatus &_status, ValueType &_value, const CommonAPI::CallInfo *_info) const {

        if (getMethodId_ != 0) {
			CommonAPI::Deployable<ValueType, _AttributeDepl> deployedValue(depl_);
			ProxyHelper<
				SerializableArguments<>,
				SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>
			>::callMethodWithReply(
					proxy_,
					getMethodId_,
					getReliable_,
					(_info ? _info : &defaultCallInfo),
					_status,
					deployedValue);
			_value = deployedValue.getValue();
        } else {
        	_status = CommonAPI::CallStatus::NOT_AVAILABLE;
        }
    }

    std::future<CallStatus> getValueAsync(AttributeAsyncCallback _callback, const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, _AttributeDepl> deployedValue(depl_);

        if (getMethodId_ != 0) {
			return ProxyHelper<
						SerializableArguments<>,
						SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>
				>::callMethodAsync(
					proxy_,
					getMethodId_,
					getReliable_,
					(_info ? _info : &defaultCallInfo),
					[_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, _AttributeDepl> _response) {
						_callback(_status, _response.getValue());
					},
					std::make_tuple(deployedValue));
        } else {
        	CommonAPI::CallStatus callStatus = CommonAPI::CallStatus::NOT_AVAILABLE;

        	ProxyHelper<
				SerializableArguments<>,
				SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>
				>::callCallbackForCallStatus(
					callStatus,
					[_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, _AttributeDepl> _response) {
						_callback(_status, _response.getValue());
					},
					std::make_tuple(deployedValue));

        	std::promise<CommonAPI::CallStatus> promise;
			promise.set_value(callStatus);
			return promise.get_future();
        }
    }

protected:
    Proxy &proxy_;
    const method_id_t getMethodId_;
    const bool getReliable_;
    _AttributeDepl *depl_;
};

template <typename _AttributeType, typename _AttributeDepl = EmptyDeployment>
class Attribute: public ReadonlyAttribute<_AttributeType, _AttributeDepl> {
public:
    typedef typename _AttributeType::ValueType ValueType;
    typedef typename _AttributeType::AttributeAsyncCallback AttributeAsyncCallback;

    Attribute(Proxy &_proxy,
              const method_id_t _getMethodId,
              const bool _getReliable,
              const method_id_t _setMethodId,
              const bool _setReliable,
              _AttributeDepl *_depl = nullptr)
        : ReadonlyAttribute< _AttributeType, _AttributeDepl>(_proxy, _getMethodId, _getReliable, _depl),
          setMethodId_(_setMethodId),
          setReliable_(_setReliable) {
    }

    void setValue(const ValueType &_request,
                  CallStatus &_status,
                  ValueType &_response,
				  const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, _AttributeDepl> deployedRequest(_request, this->depl_);
        CommonAPI::Deployable<ValueType, _AttributeDepl> deployedResponse(this->depl_);
        ProxyHelper<
            SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>,
            SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>
        >::callMethodWithReply(
                this->proxy_,
                setMethodId_,
                setReliable_,
				(_info ? _info : &defaultCallInfo),
                deployedRequest,
                _status,
                deployedResponse);
        _response = deployedResponse.getValue();
    }

    std::future<CommonAPI::CallStatus> setValueAsync(const ValueType &_request,
                                                     AttributeAsyncCallback _callback,
													 const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, _AttributeDepl> deployedRequest(_request, this->depl_);
        CommonAPI::Deployable<ValueType, _AttributeDepl> deployedResponse(this->depl_);
        return ProxyHelper<
                    SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>,
                    SerializableArguments<CommonAPI::Deployable<ValueType, _AttributeDepl>>
               >::callMethodAsync(this->proxy_,
                                  setMethodId_,
                                  setReliable_,
								  (_info ? _info : &defaultCallInfo),
                                  deployedRequest,
                                  [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, _AttributeDepl> _response) {
                                      _callback(_status, _response.getValue());
                                  },
                                  std::make_tuple(deployedResponse));
    }

protected:
    const method_id_t setMethodId_;
    const bool setReliable_;
};

template <typename _AttributeType>
class ObservableAttribute: public _AttributeType {
public:
    typedef typename _AttributeType::ValueType ValueType;
    typedef typename _AttributeType::ValueTypeDepl ValueTypeDepl;
    typedef typename _AttributeType::AttributeAsyncCallback AttributeAsyncCallback;
    typedef typename _AttributeType::ChangedEvent ChangedEvent;

    template <typename... _AttributeTypeArguments>
    ObservableAttribute(Proxy &_proxy,
                        const eventgroup_id_t _eventgroupId,
                        const event_id_t _eventId,
                        _AttributeTypeArguments... _arguments)
        : _AttributeType(_proxy, _arguments...),
          changedEvent_(_proxy,
                        _eventgroupId,
                        _eventId,
                        std::make_tuple(CommonAPI::Deployable<ValueType, ValueTypeDepl>(this->depl_))) {
    }

    ChangedEvent& getChangedEvent() {
        return changedEvent_;
    }

protected:
    Event<ChangedEvent, CommonAPI::Deployable<ValueType, ValueTypeDepl>> changedEvent_;
};

} // namespace SomeIP
} // namespace CommonAPI


#endif // COMMONAPI_SOMEIP_ATTRIBUTE_HPP_
