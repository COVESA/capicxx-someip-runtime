// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_MESSAGE_HPP_
#define COMMONAPI_SOMEIP_MESSAGE_HPP_

#include <string>

#include <vsomeip/vsomeip.hpp>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

class Address;
class Connection;

class Message {
 public:
	 COMMONAPI_EXPORT Message();
	COMMONAPI_EXPORT Message(const std::shared_ptr<vsomeip::message> &_source);
	COMMONAPI_EXPORT Message(const Message &_source);
	COMMONAPI_EXPORT Message(Message &&_source);

	COMMONAPI_EXPORT ~Message();

	COMMONAPI_EXPORT Message &operator=(const Message &_source);
	COMMONAPI_EXPORT Message &operator=(Message &&_source);
	COMMONAPI_EXPORT operator bool() const;

	COMMONAPI_EXPORT static Message createMethodCall(const Address &_address,
                                    const method_id_t _method, bool _reliable);

	COMMONAPI_EXPORT Message createResponseMessage() const;

	COMMONAPI_EXPORT static Message createNotificationMessage(const Address &_address,
                                             const event_id_t _event, bool _reliable);

	COMMONAPI_EXPORT bool isResponseType() const;
	COMMONAPI_EXPORT bool isErrorType() const;

	COMMONAPI_EXPORT byte_t* getBodyData() const;
	COMMONAPI_EXPORT message_length_t getBodyLength() const;

	COMMONAPI_EXPORT return_code_e getReturnCode() const;

	COMMONAPI_EXPORT const service_id_t getServiceId() const;
	COMMONAPI_EXPORT const instance_id_t getInstanceId() const;
	COMMONAPI_EXPORT const method_id_t getMethodId() const;

	COMMONAPI_EXPORT const client_id_t getClientId() const;
	COMMONAPI_EXPORT const session_id_t getSessionId() const;

	COMMONAPI_EXPORT void setPayloadData(const byte_t *data, message_length_t length);

 private:
    std::shared_ptr<vsomeip::message> message_;

    friend class Connection;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_MESSAGE_HPP_
