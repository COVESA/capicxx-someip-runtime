// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/Address.hpp>
#include <iomanip>

namespace CommonAPI {
namespace SomeIP {

Address::Address()
    : service_(0x0000), instance_(0x0000),
      major_version_(ANY_MAJOR_VERSION), minor_version_(ANY_MINOR_VERSION) {
}

Address::Address(const service_id_t _service, const instance_id_t _instance,
        major_version_t _major_version, minor_version_t _minor_version)
    : service_(_service), instance_(_instance),
      major_version_(_major_version), minor_version_(_minor_version) {
}

Address::Address(const Address &_source)
    : service_(_source.service_), instance_(_source.instance_),
      major_version_(_source.major_version_), minor_version_(_source.minor_version_) {
}

Address &
Address::operator=(const Address &_source) {
    service_ = _source.service_;
    instance_ = _source.instance_;
    major_version_ = _source.major_version_;
    minor_version_ = _source.minor_version_;

    return (*this);
}

bool
Address::operator==(const Address &_other) const {
    return (service_ == _other.service_
            && instance_ == _other.instance_
            && (major_version_ == ANY_MAJOR_VERSION
                || _other.major_version_ == ANY_MAJOR_VERSION
                || major_version_ == _other.major_version_));
}

bool
Address::operator!=(const Address &_other) const {
    return (service_ != _other.service_
            || instance_ != _other.instance_
            || (major_version_ != ANY_MAJOR_VERSION
                && _other.major_version_ != ANY_MAJOR_VERSION
                && major_version_ != _other.major_version_));
}

bool
Address::operator<(const Address &_other) const {
    return (service_ < _other.service_
            || (service_ == _other.service_
                && instance_ < _other.instance_)
            || (service_ == _other.service_
                && instance_ == _other.instance_
                && major_version_ != ANY_MAJOR_VERSION
                && _other.major_version_ != ANY_MAJOR_VERSION
                && major_version_ < _other.major_version_));
}

const service_id_t &
Address::getService() const {
    return service_;
}

void
Address::setService(const service_id_t _service) {
    service_ = _service;
}

const instance_id_t &
Address::getInstance() const {
    return instance_;
}
void
Address::setInstance(const instance_id_t _instance) {
    instance_ = _instance;
}

const major_version_t &
Address::getMajorVersion() const {
    return major_version_;
}

void
Address::setMajorVersion(const major_version_t _major_version) {
    major_version_ = _major_version;
}

const minor_version_t &
Address::getMinorVersion() const {
    return minor_version_;
}

void
Address::setMinorVersion(const minor_version_t _minor_version) {
    minor_version_ = _minor_version;
}

std::ostream &
operator<<(std::ostream &_out, const Address &_address) {
    _out << "["
         << std::setw(4) << std::setfill('0') << std::hex << _address.service_
         << "."
         << std::setw(4) << std::setfill('0') << std::hex << _address.instance_
         << "("
         << static_cast<int>(_address.major_version_)
         << "." << _address.minor_version_
         << ")"
         << "]";
    return (_out);
}

} // namespace SomeIP
} // namespace CommonAPI
