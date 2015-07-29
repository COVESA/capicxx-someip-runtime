// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <sys/stat.h>

#include <algorithm>
#include <sstream>

#include <CommonAPI/IniFileReader.hpp>
#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>
#include <CommonAPI/SomeIP/Config.hpp>

namespace CommonAPI {
namespace SomeIP {

const char *COMMONAPI_SOMEIP_DEFAULT_CONFIG_FILE = "commonapi-someip.ini";
const char *COMMONAPI_SOMEIP_DEFAULT_CONFIG_FOLDER = "/etc/";

std::shared_ptr<AddressTranslator> AddressTranslator::get() {
	static std::shared_ptr<AddressTranslator> theTranslator
		= std::make_shared<AddressTranslator>();
	return theTranslator;
}

AddressTranslator::AddressTranslator() {
	init();
}

void
AddressTranslator::init() {
	// Determine default configuration file
	const char *config = getenv("COMMONAPI_SOMEIP_CONFIG");
	if (config) {
		defaultConfig_ = config;
	} else {
		defaultConfig_ = COMMONAPI_SOMEIP_DEFAULT_CONFIG_FOLDER;
		defaultConfig_ += "/";
		defaultConfig_ += COMMONAPI_SOMEIP_DEFAULT_CONFIG_FILE;
	}

	(void)readConfiguration();
}

bool
AddressTranslator::translate(const std::string &_key, Address &_value) {
	return translate(CommonAPI::Address(_key), _value);
}

bool
AddressTranslator::translate(const CommonAPI::Address &_key, Address &_value) {
	bool result(true);
	std::lock_guard<std::mutex> itsLock(mutex_);

	const auto it = forwards_.find(_key);
	if (it != forwards_.end()) {
		_value = it->second;
	} else {
		COMMONAPI_ERROR(
			"Cannot determine SOME/IP address data for "
			"CommonAPI address \"", _key, "\"");
		result = false;
	}
	return result;
}

bool
AddressTranslator::translate(const Address &_key, std::string &_value) {
	CommonAPI::Address address;
	bool result = translate(_key, address);
	_value = address.getAddress();
	return result;
}

bool
AddressTranslator::translate(const Address &_key, CommonAPI::Address &_value) {
	bool result(true);
	std::lock_guard<std::mutex> itsLock(mutex_);

	const auto it = backwards_.find(_key);
	if (it != backwards_.end()) {
		_value = it->second;
	} else {
		COMMONAPI_ERROR(
			"Cannot determine CommonAPI address data for "
			"SOME/IP address \"", _key, "\"");
		result = false;
	}
	return result;
}

void
AddressTranslator::insert(
		const std::string &_address, const service_id_t _service, const instance_id_t _instance) {
	if (isValidService(_service) && isValidInstance(_instance)) {
		CommonAPI::Address address(_address);
		Address someipAddress(_service, _instance);

		std::lock_guard<std::mutex> itsLock(mutex_);
		auto fw = forwards_.find(address);
		auto bw = backwards_.find(someipAddress);
		if (fw == forwards_.end() && bw == backwards_.end()) {
			forwards_[address] = someipAddress;
			backwards_[someipAddress] = address;
			COMMONAPI_DEBUG(
				"Added address mapping: ", address, " <--> ", someipAddress);
		} else if(bw != backwards_.end() && bw->second != _address) {
			COMMONAPI_ERROR("Trying to overwrite existing SomeIP address which is "
					"already mapped to a CommonAPI address: ",
					someipAddress, " <--> ", _address);
		} else if(fw != forwards_.end() && fw->second != someipAddress) {
			COMMONAPI_ERROR("Trying to overwrite existing CommonAPI address which is "
					"already mapped to a SomeIP address: ",
					_address, " <--> ", someipAddress);
		}
	}
}

bool
AddressTranslator::readConfiguration() {
#define MAX_PATH_LEN 255
	std::string config;
	char currentDirectory[MAX_PATH_LEN];
#ifdef WIN32
	if (GetCurrentDirectory(MAX_PATH_LEN, currentDirectory)) {
#else
	if (getcwd(currentDirectory, MAX_PATH_LEN)) {
#endif
		config = currentDirectory;
		config += "/";
		config += COMMONAPI_SOMEIP_DEFAULT_CONFIG_FILE;

		struct stat s;
		if (stat(config.c_str(), &s) != 0) {
			config = defaultConfig_;
		}
	}

	IniFileReader reader;
	if (!reader.load(config))
		return false;

	for (auto itsMapping : reader.getSections()) {
		service_id_t service;
		std::string serviceEntry = itsMapping.second->getValue("service");

		std::stringstream converter;
		if (0 == serviceEntry.find("0x")) {
			converter << std::hex << serviceEntry.substr(2);
		} else {
			converter << std::dec << serviceEntry;
		}
		converter >> service;

		instance_id_t instance;
		std::string instanceEntry = itsMapping.second->getValue("instance");

		converter.str("");
		converter.clear();
		if (0 == instanceEntry.find("0x")) {
			converter << std::hex << instanceEntry.substr(2);
		} else {
			converter << std::dec << instanceEntry;
		}
		converter >> instance;

		insert(itsMapping.first, service, instance);
	}

	return true;
}

bool
AddressTranslator::isValidService(const service_id_t _service) const {
	if (_service < SOMEIP_MIN_SERVICE_ID || _service > SOMEIP_MAX_SERVICE_ID) {
		COMMONAPI_ERROR(
			"Found invalid service identifier (", _service, ")");
		return false;
	}

	return true;
}

bool
AddressTranslator::isValidInstance(const instance_id_t _instance) const {
	if (_instance < SOMEIP_MIN_INSTANCE_ID || _instance > SOMEIP_MAX_INSTANCE_ID) {
		COMMONAPI_ERROR(
			"Found invalid instance identifier (", _instance, ")");
		return false;
	}

	return true;
}

} /* namespace SomeIP */
} /* namespace CommonAPI */
