// Copyright (C) 2015-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <sys/stat.h>

#include <algorithm>
#include <sstream>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/AddressTranslator.hpp>
#include <CommonAPI/SomeIP/Configuration.hpp>
#include <CommonAPI/SomeIP/Constants.hpp>

namespace CommonAPI {
namespace SomeIP {

#ifdef _WIN32
CRITICAL_SECTION critSec;
#endif

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
#ifdef _WIN32
    InitializeCriticalSection(&critSec);
#endif
}

AddressTranslator::~AddressTranslator()
{
#ifdef _WIN32
    DeleteCriticalSection(&critSec);
#endif
}

bool
AddressTranslator::translate(const std::string &_key, Address &_value) {
    return translate(CommonAPI::Address(_key), _value);
}

bool
AddressTranslator::translate(const CommonAPI::Address &_key, Address &_value) {
    bool result(true);
#ifdef _WIN32
    EnterCriticalSection(&critSec);
#else
    std::lock_guard<std::mutex> itsLock(mutex_);
#endif
#ifdef COMMONAPI_ENABLE_ADDRESS_ALIASES
    CommonAPI::Address itsKey(_key);

    // Check whether this is an alias
    auto itsOther = others_.find(_key);
    if (itsOther != others_.end())
        itsKey = itsOther->second;

    const auto it = forwards_.find(itsKey);
#else
    const auto it = forwards_.find(_key);
#endif // COMMONAPI_ENABLE_ADDRESS_ALIASES
    if (it != forwards_.end()) {
        _value = it->second;
    } else {
        COMMONAPI_ERROR(
            "Cannot determine SOME/IP address data for "
            "CommonAPI address \"", _key, "\"");
        result = false;
    }
#ifdef _WIN32
    LeaveCriticalSection(&critSec);
#endif
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
#ifdef _WIN32
    EnterCriticalSection(&critSec);
#else
    std::lock_guard<std::mutex> itsLock(mutex_);
#endif
    const auto it = backwards_.find(_key);
    if (it != backwards_.end()) {
        _value = it->second;
    } else {
        COMMONAPI_ERROR(
            "Cannot determine CommonAPI address data for "
            "SOME/IP address \"", _key, "\"");
        result = false;
    }
#ifdef _WIN32
    LeaveCriticalSection(&critSec);
#endif
    return result;
}

void
AddressTranslator::insert(
        const std::string &_address,
        const service_id_t _service, const instance_id_t _instance,
        major_version_t _major, minor_version_t _minor) {
    if (isValidService(_service) && isValidInstance(_instance)) {
        CommonAPI::Address address(_address);
        Address someipAddress(_service, _instance, _major, _minor);
#ifdef _WIN32
        EnterCriticalSection(&critSec);
#else
        std::lock_guard<std::mutex> itsLock(mutex_);
#endif
        auto fw = forwards_.find(address);
        auto bw = backwards_.find(someipAddress);
        if (fw == forwards_.end() && bw == backwards_.end()) {
            forwards_[address] = someipAddress;
            backwards_[someipAddress] = address;
            COMMONAPI_DEBUG(
                "Added address mapping: ", address, " <--> ", someipAddress);
        } else if(bw != backwards_.end() && bw->second != _address) {
#ifdef COMMONAPI_ENABLE_ADDRESS_ALIASES
            COMMONAPI_INFO("Trying to overwrite existing SomeIP address: ", someipAddress);
            COMMONAPI_INFO("Existing CommonAPI address: ", _address);
            COMMONAPI_INFO("Setting alias: ", _address, " --> ", bw->second);
            others_[_address] = bw->second;
#else
            COMMONAPI_WARNING("Trying to overwrite existing SomeIP address which is "
                    "already mapped to a CommonAPI address: ",
                    someipAddress, " <--> ", _address);
#endif // COMMONAPI_ENABLE_ADDRESS_ALIASES
        } else if(fw != forwards_.end() && fw->second != someipAddress) {
            COMMONAPI_WARNING("Trying to overwrite existing CommonAPI address which is "
                    "already mapped to a SomeIP address: ",
                    _address, " <--> ", someipAddress);
        }
#ifdef _WIN32
    LeaveCriticalSection(&critSec);
#endif
    }
}

bool
AddressTranslator::isValidService(const service_id_t _service) const {
    if (_service < MIN_SERVICE_ID || _service > MAX_SERVICE_ID) {
        COMMONAPI_ERROR(
            "Found invalid service identifier (", _service, ")");
        return false;
    }

    return true;
}

bool
AddressTranslator::isValidInstance(const instance_id_t _instance) const {
    if (_instance < MIN_INSTANCE_ID || _instance > MAX_INSTANCE_ID) {
        COMMONAPI_ERROR(
            "Found invalid instance identifier (", _instance, ")");
        return false;
    }

    return true;
}

} // namespace SomeIP
} // namespace CommonAPI
