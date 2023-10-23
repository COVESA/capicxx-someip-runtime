// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <sys/stat.h>

#include <CommonAPI/IniFileReader.hpp>
#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/Address.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>
#include <CommonAPI/SomeIP/Configuration.hpp>
#include <CommonAPI/SomeIP/Constants.hpp>
#include <CommonAPI/SomeIP/Defines.hpp>

const char *COMMONAPI_SOMEIP_DEFAULT_CONFIG_FILE = "commonapi-someip.ini";
const char *COMMONAPI_SOMEIP_DEFAULT_CONFIG_FOLDER = "/etc/";

namespace CommonAPI {
namespace SomeIP {

std::shared_ptr<Configuration> Configuration::get() {
	static auto itsConfiguration = std::make_shared<Configuration>();
	return itsConfiguration;
}

void Configuration::load() {
    Configuration::get()->readConfiguration();
}

Configuration::Configuration() {
	// Determine default configuration file
	const char *config = getenv("COMMONAPI_SOMEIP_CONFIG");
	if (config) {
		defaultConfig_ = config;
		struct stat s;
		if (stat(defaultConfig_.c_str(), &s) != 0) {
			COMMONAPI_ERROR("Failed to load ini file passed via "
					"COMMONAPI_SOMEIP_CONFIG environment: ", defaultConfig_);
		}
	} else {
		defaultConfig_ = COMMONAPI_SOMEIP_DEFAULT_CONFIG_FOLDER;
		defaultConfig_ += "/";
		defaultConfig_ += COMMONAPI_SOMEIP_DEFAULT_CONFIG_FILE;
	}
}

const Address &
Configuration::getAddressAlias(const Address &_address) const {
    auto foundAddress = aliases_.find(_address);
    if (foundAddress != aliases_.end())
        return std::get<0>(foundAddress->second);
    return _address;
}

method_id_t
Configuration::getMethodAlias(const Address &_address, const method_id_t _method) const {
    auto foundAddress = aliases_.find(_address);
    if (foundAddress != aliases_.end()) {
        auto foundMethod = std::get<1>(foundAddress->second).find(_method);
        if (foundMethod != std::get<1>(foundAddress->second).end())
            return foundMethod->second;
    }
    return _method;
}

eventgroup_id_t
Configuration::getEventgroupAlias(const Address &_address, const eventgroup_id_t _eventgroup) const {
    auto foundAddress = aliases_.find(_address);
    if (foundAddress != aliases_.end()) {
        auto foundEventgroup = std::get<2>(foundAddress->second).find(_eventgroup);
        if (foundEventgroup != std::get<2>(foundAddress->second).end())
            return foundEventgroup->second;
    }
    return _eventgroup;
}

int64_t
Configuration::getMaxProcessingTime(const std::string &_name) const {

	const auto foundConnection = watchLimits_.find(_name);
	if (foundConnection == watchLimits_.end())
		return DEFAULT_MAX_PROCESSING_TIME;


	return foundConnection->second.first;
}

std::size_t
Configuration::getMaxQueueSize(const std::string &_name) const {

	const auto foundConnection = watchLimits_.find(_name);
	if (foundConnection == watchLimits_.end())
		return DEFAULT_MAX_QUEUE_SIZE;

	return foundConnection->second.second;
}


void
Configuration::readConfiguration() {
#define MAX_PATH_LEN 4095
    std::string config;
    bool tryLoadConfig(true);
    char currentDirectory[MAX_PATH_LEN];
#ifdef _WIN32
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
            if (stat(config.c_str(), &s) != 0) {
                tryLoadConfig = false;
            }
        }
    }

    IniFileReader reader;
    if (tryLoadConfig && !reader.load(config))
        return;

    COMMONAPI_INFO("Loading configuration file ", config);

    auto itsAddressTranslator = AddressTranslator::get();

    for (auto itsSection : reader.getSections()) {
        if (itsSection.first == "aliases") {
            for (auto itsMapping : itsSection.second->getMappings()) {
                if (itsMapping.first.find("service:") == 0) {
                    readServiceAlias(itsMapping.first.substr(8), itsMapping.second);
                } else if (itsMapping.first.find("method:") == 0) {
                    readMethodAlias(itsMapping.first.substr(7), itsMapping.second);
                } else if (itsMapping.first.find("event:") == 0) {
                    readMethodAlias(itsMapping.first.substr(6), itsMapping.second);
                } else if (itsMapping.first.find("eventgroup:") == 0) {
                    readEventgroupAlias(itsMapping.first.substr(11), itsMapping.second);
                } else {
                    COMMONAPI_ERROR("Found invalid alias configuration entry: ", itsMapping.first);
                }
            }
        } else if (itsSection.first.find("connection:") == 0) {
        	auto i = itsSection.first.find(":");
      		std::string itsConnectionName(itsSection.first.substr(i+1, itsSection.first.length()));
      		std::int64_t itsMaxProcessingTime;
      		std::size_t itsMaxQueueSize;

      		std::string itsValue = itsSection.second->getValue("max-processing-time");
      		if (itsValue.empty()) {
      			itsMaxProcessingTime = DEFAULT_MAX_PROCESSING_TIME;
      		} else {
      			std::stringstream itsConverter;
      			itsConverter << itsValue;
      			itsConverter >> std::dec >> itsMaxProcessingTime;
      		}

      		itsValue = itsSection.second->getValue("max-queue-size");
      		if (itsValue.empty()) {
      			itsMaxQueueSize = DEFAULT_MAX_QUEUE_SIZE;
      		} else {
      			std::stringstream itsConverter;
      			itsConverter << itsValue;
      			itsConverter >> std::dec >> itsMaxQueueSize;
      		}
      		watchLimits_[itsConnectionName] = std::make_pair(itsMaxProcessingTime, itsMaxQueueSize);
        } else {
            service_id_t service;
            std::string serviceEntry = itsSection.second->getValue("service");

            std::stringstream converter;
            if (0 == serviceEntry.find("0x")) {
                converter << std::hex << serviceEntry.substr(2);
            } else {
                converter << std::dec << serviceEntry;
            }
            converter >> service;

            instance_id_t instance;
            std::string instanceEntry = itsSection.second->getValue("instance");

            converter.str("");
            converter.clear();
            if (0 == instanceEntry.find("0x")) {
                converter << std::hex << instanceEntry.substr(2);
            } else {
                converter << std::dec << instanceEntry;
            }
            converter >> instance;

            major_version_t major_version(0);
            std::uint32_t major_temp(0);
            minor_version_t minor_version(0);

            std::string majorEntry = itsSection.second->getValue("major");
            converter.str("");
            converter.clear();
            converter << std::dec << majorEntry;
            converter >> major_temp;
            major_version = static_cast<std::uint8_t>(major_temp);

            std::string minorEntry = itsSection.second->getValue("minor");
            converter.str("");
            converter.clear();
            converter << std::dec << minorEntry;
            converter >> minor_version;

            itsAddressTranslator->insert(itsSection.first, service, instance, major_version, minor_version);
        }
    }
}

void
Configuration::readServiceAlias(const std::string &_source, const std::string &_target) {
	Address itsSourceAddress, itsTargetAddress;
	method_id_t itsDummy;

	if (readValue(_source, itsSourceAddress, itsDummy, false) &&
			readValue(_target, itsTargetAddress, itsDummy, false)) {

		auto findService = aliases_.find(itsSourceAddress);
		if (findService == aliases_.end()) {
			Alias_t itsTarget = std::make_tuple(itsTargetAddress, MethodAlias_t(), EventgroupAlias_t());
			aliases_.insert(std::make_pair(itsSourceAddress, itsTarget));
		} else {
			if (itsTargetAddress != std::get<0>(findService->second)) {
				COMMONAPI_ERROR("Found multiple aliases for address ", itsSourceAddress);
			}
		}
	}
}

void
Configuration::readMethodAlias(const std::string &_source, const std::string &_target) {
	Address itsSourceAddress, itsTargetAddress;
	method_id_t itsSourceMethod, itsTargetMethod;

	if (readValue(_source, itsSourceAddress, itsSourceMethod, true) &&
			readValue(_target, itsTargetAddress, itsTargetMethod, true)) {
		if (isValidMethod(itsSourceMethod) && isValidMethod(itsTargetMethod)) {
			auto findService = aliases_.find(itsSourceAddress);
			if (findService == aliases_.end()) {
				MethodAlias_t itsMethods;
				itsMethods.insert(std::make_pair(itsSourceMethod, itsTargetMethod));
				Alias_t itsTarget = std::make_tuple(itsTargetAddress, itsMethods, EventgroupAlias_t());
				aliases_.insert(std::make_pair(itsSourceAddress, itsTarget));
			} else {
				if (itsTargetAddress == std::get<0>(findService->second)) {
					auto findMethod = std::get<1>(findService->second).find(itsSourceMethod);
					if (findMethod == std::get<1>(findService->second).end()) {
						std::get<1>(findService->second).insert(std::make_pair(itsSourceMethod, itsTargetMethod));
					} else {
						if (findMethod->second != itsTargetMethod) {
							COMMONAPI_ERROR("Found multiple aliases for method ", itsSourceAddress, ".", itsSourceMethod);
						}
					}
				} else {
					COMMONAPI_ERROR("Found multiple aliases for address ", itsSourceAddress);
				}
			}
		}
	}
}

void
Configuration::readEventgroupAlias(const std::string &_source, const std::string &_target) {
	Address itsSourceAddress, itsTargetAddress;
	method_id_t itsSourceEventgroup, itsTargetEventgroup;

	if (readValue(_source, itsSourceAddress, itsSourceEventgroup, true) &&
			readValue(_target, itsTargetAddress, itsTargetEventgroup, true)) {
		if (isValidEventgroup(itsSourceEventgroup) && isValidEventgroup(itsTargetEventgroup)) {
			auto findService = aliases_.find(itsSourceAddress);
			if (findService == aliases_.end()) {
				EventgroupAlias_t itsEventgroups;
				itsEventgroups.insert(std::make_pair(itsSourceEventgroup, itsTargetEventgroup));
				Alias_t itsTarget = std::make_tuple(itsTargetAddress, MethodAlias_t(), itsEventgroups);
				aliases_.insert(std::make_pair(itsSourceAddress, itsTarget));
			} else {
				if (itsTargetAddress == std::get<0>(findService->second)) {
					auto findEventgroup = std::get<2>(findService->second).find(itsSourceEventgroup);
					if (findEventgroup == std::get<2>(findService->second).end()) {
						std::get<2>(findService->second).insert(std::make_pair(itsSourceEventgroup, itsTargetEventgroup));
					} else {
						if (findEventgroup->second != itsTargetEventgroup) {
							COMMONAPI_ERROR("Found multiple aliases for method ", itsSourceAddress, ".", itsSourceEventgroup);
						}
					}
				} else {
					COMMONAPI_ERROR("Found multiple aliases for address ", itsSourceAddress);
				}
			}
		}
	}
}

bool
Configuration::readValue(const std::string &_data,
		Address &_address, uint16_t &_id, bool _readId) {

	std::string itsServiceStr, itsInstanceStr, itsMajorStr("1"), itsMinorStr("0");
	std::string itsIdStr("0xFFFF"), itsTempStr;

	auto foundService = _data.find(':');
	if (foundService == std::string::npos) {
		return false;
	}

	itsServiceStr = _data.substr(0, foundService);

	auto foundInstance = _data.find(':', foundService+1);
	itsInstanceStr = _data.substr(foundService+1, foundInstance-foundService-1);

	if (foundService != std::string::npos) {
		auto foundMajor = _data.find(':', foundInstance+1);
		itsTempStr = _data.substr(foundInstance+1, foundMajor-foundInstance-1);

		if (foundMajor != std::string::npos) {
			itsMajorStr = itsTempStr;

			auto foundMinor = _data.find(':', foundMajor+1);
			itsMinorStr = _data.substr(foundMajor+1, foundMinor-foundMajor-1);

			if (foundMinor != std::string::npos) {
				itsIdStr = _data.substr(foundMinor+1);
			}
		} else {
			if (_readId) {
				itsIdStr = itsTempStr;
			}
		}
	} else if (_readId) {
		return false;
	}

	service_id_t itsService(0);
	instance_id_t itsInstance(0);
	major_version_t itsMajor(0);
	minor_version_t itsMinor(0);

	{
		std::stringstream itsConverter;
		if (itsServiceStr.find("0x") == 0) itsConverter << std::hex;
		itsConverter << itsServiceStr;
		itsConverter >> itsService;
	}

	{
		std::stringstream itsConverter;
		if (itsInstanceStr.find("0x") == 0) itsConverter << std::hex;
		itsConverter << itsInstanceStr;
		itsConverter >> itsInstance;
	}

	{
		std::stringstream itsConverter;
		if (itsMajorStr.find("0x") == 0) itsConverter << std::hex;
		itsConverter << itsMajorStr;
		int itsTempMajor(0);
		itsConverter >> itsTempMajor;
		itsMajor = major_version_t(itsTempMajor);
	}

	{
		std::stringstream itsConverter;
		if (itsMinorStr.find("0x") == 0) itsConverter << std::hex;
		itsConverter << itsMinorStr;
		itsConverter >> itsMinor;
	}

	{
		std::stringstream itsConverter;
		if (itsIdStr.find("0x") == 0) itsConverter << std::hex;
		itsConverter << itsIdStr;
		itsConverter >> _id;
	}

	_address.setService(itsService);
	_address.setInstance(itsInstance);
	_address.setMajorVersion(itsMajor);
	_address.setMinorVersion(itsMinor);

	return true;
}

bool
Configuration::isValidMethod(const method_id_t _method) const {
    if (_method < MIN_METHOD_ID || _method > MAX_METHOD_ID) {
        COMMONAPI_ERROR(
            "Found invalid method identifier (", _method, ")");
        return false;
    }

    return true;
}

bool
Configuration::isValidEventgroup(const eventgroup_id_t _eventgroup) const {
    if (_eventgroup < MIN_EVENTGROUP_ID || _eventgroup > MAX_EVENTGROUP_ID) {
        COMMONAPI_ERROR(
            "Found invalid eventgroup identifier (", _eventgroup, ")");
        return false;
    }

    return true;
}

} // namespace SomeIP
} // namespace CommonAPI
