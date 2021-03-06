//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "supervisor_test.h"
#include "access.h"
#include "catch.hpp"
#include "cassert"

using namespace rotor::test;
using namespace rotor;

supervisor_test_t::supervisor_test_t(supervisor_config_test_t &config_)
    : supervisor_t{config_}, locality{config_.locality}, configurer{std::move(config_.configurer)} {}

supervisor_test_t::~supervisor_test_t() { printf("~supervisor_test_t, %p(%p)\n", (void *)this, (void *)address.get()); }

address_ptr_t supervisor_test_t::make_address() noexcept { return instantiate_address(locality); }

void supervisor_test_t::start_timer(const pt::time_duration &, request_id_t timer_id) noexcept {
    active_timers.emplace_back(timer_id);
}

void supervisor_test_t::cancel_timer(request_id_t timer_id) noexcept {
    auto it = active_timers.begin();
    while (it != active_timers.end()) {
        if (*it == timer_id) {
            active_timers.erase(it);
            return;
        } else {
            ++it;
        }
    }
    assert(0 && "should not happen");
}

subscription_container_t &supervisor_test_t::get_points() noexcept {
    auto plugin = get_plugin(plugin::lifetime_plugin_t::class_identity);
    return static_cast<plugin::lifetime_plugin_t *>(plugin)->access<to::points>();
}

request_id_t supervisor_test_t::get_timer(std::size_t index) noexcept {
    auto it = active_timers.begin();
    for (std::size_t i = 0; i < index; ++i) {
        ++it;
    }
    return *it;
}

void supervisor_test_t::enqueue(message_ptr_t message) noexcept { get_leader().queue.emplace_back(std::move(message)); }

pt::time_duration rotor::test::default_timeout{pt::milliseconds{1}};

size_t supervisor_test_t::get_children_count() noexcept { return manager->access<to::actors_map>().size(); }

supervisor_test_t &supervisor_test_t::get_leader() {
    return *static_cast<supervisor_test_t *>(access<to::locality_leader>());
}

void supervisor_test_t::configure(plugin::plugin_base_t &plugin) noexcept {
    supervisor_t::configure(plugin);
    if (configurer){
        configurer(*this, plugin);
    }
}
