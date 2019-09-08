//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

// in the example the usage of request-responce pattern is demonstrated
// the "server" actor takes the number from request and replies to
// "client" actor with square root if the value is >= 0, otherwise
// it replies with error.
//
// The key point here is that request is timeout supervised, i.e.
// if the server will not answer with the specified timeout,
// the client will know that.

#include "rotor.hpp"
#include "rotor/asio.hpp"
#include <iostream>
#include <cmath>
#include <system_error>

namespace asio = boost::asio;
namespace pt = boost::posix_time;

namespace payload {
struct sample_res_t {
    double value;
};
struct sample_req_t {
    using responce_t = sample_res_t;
    double value;
};
} // namespace payload

namespace message {
using request_t = rotor::request_traits_t<payload::sample_req_t>::request::message_t;
using responce_t = rotor::request_traits_t<payload::sample_req_t>::responce::message_t;
} // namespace message

struct server_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void on_request(message::request_t &req) noexcept {
        auto in = req.payload.request_payload.value;
        if (in >= 0) {
            auto value = std::sqrt(in);
            reply_to(req, value);
        } else {
            // IRL, it should be your custom error codes
            auto ec = std::make_error_code(std::errc::invalid_argument);
            reply_with_error(req, ec);
        }
    }

    void init_start() noexcept override {
        subscribe(&server_actor::on_request);
        rotor::actor_base_t::init_start();
    }
};

struct client_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    rotor::address_ptr_t server_addr;

    void set_server(const rotor::address_ptr_t addr) { server_addr = addr; }

    void init_start() noexcept override {
        subscribe(&client_actor::on_responce);
        rotor::actor_base_t::init_start();
    }

    void on_responce(message::responce_t &res) noexcept {
        if (!res.payload.ec) { // check for possible error
            auto &in = res.payload.req->payload.request_payload.value;
            auto &out = res.payload.res.value;
            std::cout << " in = " << in << ", out = " << out << "\n";
        }
        supervisor.do_shutdown(); // optional;
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_start(msg);
        auto timeout = rotor::pt::milliseconds{1};
        request<payload::sample_req_t>(server_addr, 25.0).send(timeout);
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    auto stand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = boost::posix_time::milliseconds{500};
    rotor::asio::supervisor_config_asio_t conf{timeout, std::move(stand)};
    auto sup = system_context->create_supervisor<rotor::asio::supervisor_asio_t>(conf);
    auto server = sup->create_actor<server_actor>(timeout);
    auto client = sup->create_actor<client_actor>(timeout);
    client->set_server(server->get_address());
    sup->do_process();
    return 0;
}
