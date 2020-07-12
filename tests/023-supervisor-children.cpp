//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "system_context_test.h"

namespace r = rotor;
namespace rt = r::test;

namespace {
namespace to {
struct init_request {};
struct shutdown_request {};
} // namespace to
} // namespace

template <> auto &r::actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &r::actor_base_t::access<to::shutdown_request>() noexcept { return shutdown_request; }

struct sample_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    virtual void init_finish() noexcept override {}

    void confirm_init() noexcept { r::actor_base_t::init_finish(); }
};

struct fail_init_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void init_finish() noexcept override { supervisor->do_shutdown(); }
};

struct fail_start_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        supervisor->do_shutdown();
    }
};

struct custom_init_plugin1_t;
struct custom_init_plugin2_t;

struct fail_init_actor2_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    using plugins_list_t = std::tuple<r::internal::address_maker_plugin_t, r::internal::lifetime_plugin_t,
                                      r::internal::init_shutdown_plugin_t, custom_init_plugin1_t,
                                      r::internal::prestarter_plugin_t, r::internal::starter_plugin_t>;
};

struct fail_init_actor3_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void init_start() noexcept override {
        rt::actor_test_t::init_start();
        do_shutdown();
    }
};

struct fail_init_actor4_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void init_start() noexcept override {
        do_shutdown();
        rt::actor_test_t::init_start();
    }
};

struct fail_init_actor5_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void init_start() noexcept override {
        supervisor->do_shutdown();
        rt::actor_test_t::init_start();
    }
};

struct fail_init_actor6_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    using plugins_list_t = std::tuple<r::internal::address_maker_plugin_t, r::internal::lifetime_plugin_t,
                                      r::internal::init_shutdown_plugin_t, custom_init_plugin2_t,
                                      r::internal::prestarter_plugin_t, r::internal::starter_plugin_t>;
};

struct custom_init_plugin1_t : r::plugin_t {
    static const void *class_identity;

    void activate(r::actor_base_t *actor) noexcept override {
        r::plugin_t::activate(actor);
        reaction_on(reaction_t::INIT);
    }

    bool handle_init(r::message::init_request_t *) noexcept override { return false; }

    const void *identity() const noexcept override { return class_identity; }
};

const void *custom_init_plugin1_t::class_identity = static_cast<const void *>(typeid(custom_init_plugin1_t).name());

struct custom_init_plugin2_t : r::plugin_t {
    static const void *class_identity;

    void activate(r::actor_base_t *actor) noexcept override {
        r::plugin_t::activate(actor);
        reaction_on(reaction_t::INIT);
    }

    bool handle_init(r::message::init_request_t *message) noexcept override {
        auto ec = r::make_error_code(r::error_code_t::actor_misconfigured);
        actor->reply_with_error(*message, ec);
        actor->access<to::init_request>().reset();
        return false;
    }

    const void *identity() const noexcept override { return class_identity; }
};

const void *custom_init_plugin2_t::class_identity = static_cast<const void *>(typeid(custom_init_plugin2_t).name());

struct custom_shutdown_plugin_t : r::plugin_t {
    static const void *class_identity;

    void activate(r::actor_base_t *actor) noexcept override {
        r::plugin_t::activate(actor);
        reaction_on(reaction_t::SHUTDOWN);
    }

    bool handle_shutdown(r::message::shutdown_request_t *message) noexcept override {
        auto ec = r::make_error_code(r::error_code_t::actor_misconfigured);
        actor->reply_with_error(*message, ec);
        actor->access<to::shutdown_request>().reset();
        return false;
    }

    const void *identity() const noexcept override { return class_identity; }
};

const void *custom_shutdown_plugin_t::class_identity =
    static_cast<const void *>(typeid(custom_shutdown_plugin_t).name());

struct sample_supervisor_t : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;
    using child_ptr_t = r::intrusive_ptr_t<sample_actor_t>;

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::child_manager_plugin_t>(
            [this](auto &) { child = create_actor<sample_actor_t>().timeout(rt::default_timeout).finish(); });
    }

    child_ptr_t child;
};

struct post_shutdown_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void shutdown_start() noexcept override {
        rt::actor_test_t::shutdown_start();
        do_shutdown();
    }
};

struct pre_shutdown_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void shutdown_start() noexcept override {
        do_shutdown();
        rt::actor_test_t::shutdown_start();
    }
};

struct managed_supervisor_t : public rt::supervisor_test_t {
    using parent_t = rt::supervisor_test_t;
    using actor_t = rt::actor_test_t;
    using child_ptr_t = r::intrusive_ptr_t<actor_t>;
    using parent_t::parent_t;

    child_ptr_t child;

    void do_initialize(r::system_context_t *ctx) noexcept override {
        parent_t::do_initialize(ctx);
        child = create_actor<actor_t>().timeout(rt::default_timeout).finish();
    }
};

struct fail_shutdown_actor1_t : rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void shutdown_finish() noexcept override {}
};

struct fail_shutdown_actor2_t : rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    using plugins_list_t = std::tuple<r::internal::address_maker_plugin_t, r::internal::lifetime_plugin_t,
                                      r::internal::init_shutdown_plugin_t, r::internal::prestarter_plugin_t,
                                      custom_shutdown_plugin_t, r::internal::starter_plugin_t>;
};

TEST_CASE("supervisor is not initialized, while it child did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    act->confirm_init();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("supervisor is not initialized, while it 1 of 2 children did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act1 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    auto act2 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    act1->confirm_init();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("supervisor does not starts, if a children did not initialized", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act1 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    auto act2 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    act1->confirm_init();
    sup->do_process();
    REQUIRE(act1->get_state() == r::state_t::OPERATIONAL);

    REQUIRE(sup->active_timers.size() == 2);
    auto act2_init_req = sup->get_timer(1);
    sup->on_timer_trigger(act2_init_req);
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act2->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("supervisor create child during init phase", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<sample_supervisor_t>().timeout(rt::default_timeout).finish();
    auto &act = sup->child;

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    act->confirm_init();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("shutdown_failed policy", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .policy(r::supervisor_policy_t::shutdown_failed)
                   .timeout(rt::default_timeout)
                   .finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    auto act_init_req = sup->get_timer(1);
    sup->on_timer_trigger(act_init_req);
    sup->do_process();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("shutdown_self policy", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .policy(r::supervisor_policy_t::shutdown_self)
                   .timeout(rt::default_timeout)
                   .finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    auto act_init_req = sup->get_timer(1);
    sup->on_timer_trigger(act_init_req);
    sup->do_process();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor shutdown's self during start => supervisor is not affected", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_start_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor shutdown on init_finish()", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_init_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor shutdown on init_start(), post", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_init_actor3_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor shutdown on init_start(), pre", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_init_actor4_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor sends shutdown to sup during init_start()", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_init_actor5_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor replies with error to init", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_init_actor6_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("actor shutdown during init", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_init_actor2_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);

    act->do_shutdown();
    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("two actors shutdown during init", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act1 = sup->create_actor<fail_init_actor2_t>().timeout(rt::default_timeout).finish();
    auto act2 = sup->create_actor<fail_init_actor2_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);

    act1->do_shutdown();
    act2->do_shutdown();
    sup->do_process();
    REQUIRE(act1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("double shutdown attempt (post)", "[supervisor]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<post_shutdown_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("double shutdown attempt (pre)", "[supervisor]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<pre_shutdown_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("managed supervisor (autostart child)", "[supervisor]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<managed_supervisor_t>().timeout(rt::default_timeout).finish();
    auto act = sup->child;

    sup->do_process();
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("failed to shutdown actor (1)", "[supervisor]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_shutdown_actor1_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();

    CHECK(sup->active_timers.size() == 1);
    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();

    REQUIRE(system_context.ec == r::error_code_t::request_timeout);

    CHECK(act->get_state() == r::state_t::SHUTTING_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("failed to shutdown actor (2)", "[supervisor]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_shutdown_actor2_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(system_context.ec == r::error_code_t::actor_misconfigured);
    act->force_cleanup();

    CHECK(act->get_state() == r::state_t::SHUTTING_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
}
