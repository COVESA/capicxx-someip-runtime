// Copyright (C) 2013-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_HELPER_HPP_
#define COMMONAPI_SOMEIP_HELPER_HPP_

namespace CommonAPI {
namespace SomeIP {

template <size_t ...>
struct index_sequence {};


template <size_t N_, size_t ...S_>
struct make_sequence : make_sequence<N_-1, N_-1, S_...> {};

template <size_t ...S_>
struct make_sequence<0, S_...> {
    typedef index_sequence<S_...> type;
};


template <size_t N_, size_t Offset_, size_t ...S_>
struct make_sequence_range : make_sequence_range<N_-1, Offset_, N_-1+Offset_, S_...> {};

template <size_t Offset_, size_t ...S_>
struct make_sequence_range<0, Offset_, S_...> {
    typedef index_sequence<S_...> type;
};

static inline std::string
addressToString(vsomeip_sec_ip_addr_t _addr) {

    std::ostringstream itsConverter;
    itsConverter << std::dec
            << (_addr & 0xff) << '.'
            << ((_addr >> 8) & 0xff) << '.'
            << ((_addr >> 16) & 0xff) << '.'
            << ((_addr >> 24) & 0xff);
    return itsConverter.str();
}

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_HELPER_HPP_
