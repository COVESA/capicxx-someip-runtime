// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_ADDRESS_HPP_
#define COMMONAPI_SOMEIP_ADDRESS_HPP_

#include <iostream>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/SomeIP/Constants.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

class COMMONAPI_EXPORT Address {
public:
    Address();
    Address(const service_id_t _service, const instance_id_t _instance,
            major_version_t _major_version = ANY_MAJOR_VERSION,
            minor_version_t _minor_version = ANY_MINOR_VERSION);
    Address(const Address &_source);

    Address &operator=(const Address &_source);

    bool operator==(const Address &_other) const;
    bool operator!=(const Address &_other) const;
    bool operator<(const Address &_other) const;

    const service_id_t &getService() const;
    void setService(const service_id_t _service);

    const instance_id_t &getInstance() const;
    void setInstance(const instance_id_t _instance);

    const major_version_t &getMajorVersion() const;
    void setMajorVersion(const major_version_t _major_version);

    const minor_version_t &getMinorVersion() const;
    void setMinorVersion(const minor_version_t _minor_version);

private:
    service_id_t service_;
    instance_id_t instance_;
    major_version_t major_version_;
    minor_version_t minor_version_;

friend std::ostream &operator<<(std::ostream &_out, const Address &_address);
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_ADDRESS_HPP_
