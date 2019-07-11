#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <string>
#include <system_error>

namespace rotor {

enum class error_code_t {
    success = 0,
    shutdown_timeout,
    missing_actor,
    supervisor_defined,
    supervisor_wrong_state,
};

namespace details {

class error_code_category : public std::error_category {
    virtual const char *name() const noexcept override;
    virtual std::string message(int c) const override;
};

} // namespace details

const details::error_code_category &error_code_category();

inline std::error_code make_error_code(error_code_t e) { return {static_cast<int>(e), error_code_category()}; }

} // namespace rotor

namespace std {
template <> struct is_error_code_enum<rotor::error_code_t> : std::true_type {};
} // namespace std
