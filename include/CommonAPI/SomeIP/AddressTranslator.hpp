// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_ADDRESSTRANSLATOR_HPP_
#define COMMONAPI_SOMEIP_ADDRESSTRANSLATOR_HPP_

#include <map>
#include <memory>
#include <mutex>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Address.hpp>
#include <CommonAPI/SomeIP/Address.hpp>

namespace CommonAPI {
namespace SomeIP {

class AddressTranslator {
public:
	COMMONAPI_EXPORT static std::shared_ptr<AddressTranslator> get();

	COMMONAPI_EXPORT AddressTranslator();

	COMMONAPI_EXPORT void init();

	COMMONAPI_EXPORT bool translate(const std::string &_key, Address &_value);
	COMMONAPI_EXPORT bool translate(const CommonAPI::Address &_key, Address &_value);

	COMMONAPI_EXPORT bool translate(const Address &_key, std::string &_value);
	COMMONAPI_EXPORT bool translate(const Address &_key, CommonAPI::Address &_value);

	COMMONAPI_EXPORT void insert(const std::string &_address, service_id_t _service, instance_id_t _instance);

private:
	COMMONAPI_EXPORT bool readConfiguration();

	COMMONAPI_EXPORT bool isValidService(const service_id_t) const;
	COMMONAPI_EXPORT bool isValidInstance(const instance_id_t) const;

private:
	std::string defaultConfig_;

	std::map<CommonAPI::Address, Address> forwards_;
	std::map<Address, CommonAPI::Address> backwards_;

	std::mutex mutex_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_ADDRESSTRANSLATOR_HPP_
