//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include <assert.h>
#include <iostream>
//#include <boost/core/demangle.hpp>

using namespace rotor;

supervisor_t::supervisor_t(supervisor_t *sup, const supervisor_config_t &config)
    : actor_base_t(*this), parent{sup}, last_req_id{1}, shutdown_timeout{config.shutdown_timeout} {}

address_ptr_t supervisor_t::make_address() noexcept {
    auto root_sup = this;
    while (root_sup->parent) {
        root_sup = root_sup->parent;
    }
    return instantiate_address(root_sup);
}

address_ptr_t supervisor_t::instantiate_address(const void *locality) noexcept {
    return new address_t{*this, locality};
}

actor_behavior_t *supervisor_t::create_behaviour() noexcept { return new supervisor_behavior_t(*this); }

void supervisor_t::do_initialize(system_context_t *ctx) noexcept {
    context = ctx;
    // should be created very early
    address = create_address();
    behaviour = create_behaviour();

    bool use_other = parent && parent->address->same_locality(*address);
    effective_queue = use_other ? parent->effective_queue : &queue;

    actor_base_t::do_initialize(ctx);
    subscribe(&supervisor_t::on_call);
    subscribe(&supervisor_t::on_initialize_confirm);
    subscribe(&supervisor_t::on_shutdown_confirm);
    subscribe(&supervisor_t::on_create);
    subscribe(&supervisor_t::on_external_subs);
    subscribe(&supervisor_t::on_commit_unsubscription);
    subscribe(&supervisor_t::on_state_request);

    // do self-bootstrap
    if (!parent) {
        request<payload::initialize_actor_t>(address, address).timeout(shutdown_timeout);
    }
}

void supervisor_t::on_create(message_t<payload::create_actor_t> &msg) noexcept {
    auto actor = msg.payload.actor;
    auto actor_address = actor->get_address();
    actors_map.emplace(actor_address, std::move(actor));
    request<payload::initialize_actor_t>(actor_address, actor_address).timeout(msg.payload.timeout);
}

void supervisor_t::on_initialize_confirm(message::init_response_t &msg) noexcept {
    auto &addr = msg.payload.req->payload.request_payload.actor_address;
    auto &ec = msg.payload.ec;
    if (ec) {
        static_cast<supervisor_behavior_t *>(behaviour)->on_init_fail(addr, ec);
    } else {
        send<payload::start_actor_t>(addr, addr);
    }
}

void supervisor_t::do_process() noexcept {
    while (effective_queue->size()) {
        auto message = effective_queue->front();
        auto &dest = message->address;
        effective_queue->pop_front();
        auto &dest_sup = dest->supervisor;
        auto internal = &dest_sup == this;
        // std::cout << "msg [" << (internal ? "i" : "e") << "] :" << boost::core::demangle((const char*)
        // message->get_type_index()) << "\n";
        if (internal) { /* subscriptions are handled by me */
            deliver_local(std::move(message));
        } else if (dest_sup.address->same_locality(*address)) {
            dest_sup.deliver_local(std::move(message));
        } else {
            dest_sup.enqueue(std::move(message));
        }
    }
}

void supervisor_t::deliver_local(message_ptr_t &&message) noexcept {
    auto it_subscriptions = subscription_map.find(message->address);
    if (it_subscriptions != subscription_map.end()) {
        auto &subscription = it_subscriptions->second;
        auto recipients = subscription.get_recipients(message->type_index);
        if (recipients) {
            for (auto &it : *recipients) {
                if (it.mine) {
                    it.handler->call(message);
                } else {
                    auto sup = it.handler->raw_supervisor_ptr;
                    auto wrapped_message = make_message<payload::handler_call_t>(sup->address, message, it.handler);
                    sup->enqueue(std::move(wrapped_message));
                }
            }
        }
    }
}

void supervisor_t::unsubscribe_actor(const actor_ptr_t &actor) noexcept {
    auto &points = actor->get_subscription_points();
    auto it = points.rbegin();
    while (it != points.rend()) {
        auto &addr = it->address;
        auto &handler = it->handler;
        unsubscribe_actor(addr, handler);
        ++it;
    }
}

void supervisor_t::do_shutdown() noexcept {
    auto upstream_sup = parent ? parent : this;
    send<payload::shutdown_trigger_t>(upstream_sup->get_address(), address);
}

void supervisor_t::on_shutdown_trigger(message::shutdown_trigger_t &msg) noexcept {
    auto &source_addr = msg.payload.actor_address;
    if (source_addr == address) {
        if (parent) { // will be routed via shutdown request
            do_shutdown();
        } else {
            shutdown_start();
        }
    } else {
        request<payload::shutdown_request_t>(source_addr, source_addr).timeout(shutdown_timeout);
    }
}

void supervisor_t::on_shutdown_confirm(message::shutdown_responce_t &msg) noexcept {
    auto &source_addr = msg.payload.req->payload.request_payload.actor_address;
    auto &ec = msg.payload.ec;
    if (ec) {
        return static_cast<supervisor_behavior_t *>(behaviour)->on_shutdown_fail(source_addr, ec);
    }
    // std::cout << "supervisor_t::on_shutdown_confirm\n";
    auto &actor = actors_map.at(source_addr);
    remove_actor(*actor);
}

void supervisor_t::on_external_subs(message_t<payload::external_subscription_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &addr = message.payload.target_address;
    assert(&addr->supervisor == this);
    subscribe_actor(addr, handler);
}

void supervisor_t::on_commit_unsubscription(message_t<payload::commit_unsubscription_t> &message) noexcept {
    commit_unsubscription(message.payload.target_address, message.payload.handler);
}

void supervisor_t::on_call(message_t<payload::handler_call_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &orig_message = message.payload.orig_message;
    handler->call(orig_message);
}

void supervisor_t::on_state_request(message::state_request_t &message) noexcept {
    auto &addr = message.payload.request_payload.subject_addr;
    state_t target_state = state_t::UNKNOWN;
    if (addr == address) {
        target_state = state;
    } else {
        auto it = actors_map.find(addr);
        if (it != actors_map.end()) {
            target_state = it->second->state;
        }
    }
    reply_to(message, target_state);
}

void supervisor_t::commit_unsubscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto &subscriptions = subscription_map.at(addr);
    auto left = subscriptions.unsubscribe(handler);
    if (!left) {
        subscription_map.erase(addr);
    }
}

void supervisor_t::remove_actor(actor_base_t &actor) noexcept {
    auto it_actor = actors_map.find(actor.address);
    actors_map.erase(it_actor);
    if (actors_map.empty() && state == state_t::SHUTTING_DOWN) {
        static_cast<supervisor_behavior_t *>(behaviour)->on_childen_removed();
    }
}

void supervisor_t::on_timer_trigger(timer_id_t timer_id) {
    auto it = request_map.find(timer_id);
    auto &request_curry = it->second;
    message_ptr_t &request = request_curry.request_message;
    auto timeout_message =
        request_curry.fn(request_curry.reply_to, *request, make_error_code(error_code_t::request_timeout));
    put(std::move(timeout_message));
    request_map.erase(it);
}
