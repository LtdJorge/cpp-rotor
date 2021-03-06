//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/ev/supervisor_ev.h"

using namespace rotor::ev;

void supervisor_ev_t::async_cb(struct ev_loop *, ev_async *w, int revents) noexcept {
    assert(revents & EV_ASYNC);
    (void)revents;
    auto *sup = static_cast<supervisor_ev_t *>(w->data);
    sup->on_async();
}

static void timer_cb(struct ev_loop *, ev_timer *w, int revents) noexcept {
    assert(revents & EV_TIMER);
    (void)revents;
    auto *sup = static_cast<supervisor_ev_t *>(w->data);
    auto timer = static_cast<supervisor_ev_t::timer_t *>(w);
    sup->on_timer_trigger(timer->timer_id);
    sup->do_process();
}

supervisor_ev_t::supervisor_ev_t(supervisor_config_ev_t &config_)
    : supervisor_t{config_}, loop{config_.loop}, loop_ownership{config_.loop_ownership}, pending{false} {
    ev_async_init(&async_watcher, async_cb);
}

void supervisor_ev_t::do_initialize(system_context_t *ctx) noexcept {
    async_watcher.data = this;
    ev_async_start(loop, &async_watcher);
    supervisor_t::do_initialize(ctx);
}

void supervisor_ev_t::enqueue(rotor::message_ptr_t message) noexcept {
    bool ok{false};
    try {
        auto leader = static_cast<supervisor_ev_t *>(locality_leader);
        auto &inbound = leader->inbound;
        std::lock_guard<std::mutex> lock(leader->inbound_mutex);
        if (!leader->pending) {
            // async events are "compressed" by EV. Need to do only once
            intrusive_ptr_add_ref(this);
        }
        inbound.emplace_back(std::move(message));
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        ev_async_send(loop, &async_watcher);
    }
}

void supervisor_ev_t::start() noexcept {
    bool ok{false};
    try {
        auto leader = static_cast<supervisor_ev_t *>(locality_leader);
        std::lock_guard<std::mutex> lock(leader->inbound_mutex);
        if (!leader->pending) {
            leader->pending = true;
            intrusive_ptr_add_ref(leader);
        }
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        ev_async_send(loop, &async_watcher);
    }
}

void supervisor_ev_t::shutdown_finish() noexcept {
    supervisor_t::shutdown_finish();
    ev_async_stop(loop, &async_watcher);
}

void supervisor_ev_t::shutdown() noexcept {
    auto &sup_addr = supervisor->get_address();
    supervisor->enqueue(make_message<payload::shutdown_trigger_t>(sup_addr, address));
}

void supervisor_ev_t::start_timer(const rotor::pt::time_duration &timeout, request_id_t timer_id) noexcept {
    auto timer = std::make_unique<timer_t>();
    auto timer_ptr = timer.get();
    ev_tstamp ev_timeout = static_cast<ev_tstamp>(timeout.total_nanoseconds()) / 1000000000;
    ev_timer_init(timer_ptr, timer_cb, ev_timeout, 0);
    timer_ptr->timer_id = timer_id;
    timer_ptr->data = this;

    ev_timer_start(loop, timer_ptr);
    intrusive_ptr_add_ref(this);
    timers_map.emplace(timer_id, std::move(timer));
}

void supervisor_ev_t::cancel_timer(request_id_t timer_id) noexcept {
    auto &timer = timers_map.at(timer_id);
    ev_timer_stop(loop, timer.get());
    timers_map.erase(timer_id);
    intrusive_ptr_release(this);
}

void supervisor_ev_t::on_timer_trigger(request_id_t timer_id) noexcept {
    intrusive_ptr_release(this);
    timers_map.erase(timer_id);
    supervisor_t::on_timer_trigger(timer_id);
}

void supervisor_ev_t::on_async() noexcept {
    bool ok{false};
    try {
        auto leader = static_cast<supervisor_ev_t *>(locality_leader);
        auto &inbound = leader->inbound;
        auto &queue = leader->queue;
        std::lock_guard<std::mutex> lock(leader->inbound_mutex);
        std::move(inbound.begin(), inbound.end(), std::back_inserter(queue));
        inbound.clear();
        leader->pending = false;
        intrusive_ptr_release(leader);
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        do_process();
    }
}

supervisor_ev_t::~supervisor_ev_t() {
    if (loop_ownership) {
        ev_loop_destroy(loop);
    }
}
