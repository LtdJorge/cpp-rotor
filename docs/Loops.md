# Event loops & platforms

## Event loops

[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[ev]: http://software.schmorp.de/pkg/libev.html
[libevent]: https://libevent.org/
[libuv]: https://libuv.org/
[gtk]: https://www.gtk.org/
[qt]: https://www.qt.io/
[issues]: https://github.com/basiliscos/cpp-rotor/issues

 event loop   | support status
--------------|---------------
[boost-asio]  | supported
[wx-widgets]  | supported
[ev]          | supported
[libevent]    | planned
[libuv]       | planned
[gtk]         | planned
[qt]          | planned

If you need some other event loop or speedup inclusion of a planned one, please file an [issue][issues].

## platforms

event loop   | support status
-------------|---------------
linux        | supported
windows      | supported
macos        | supported

## Adding loop support guide

Adding new event loop to `rotor` is rather simple: the new `supervisor` class
should be derived from `supervisor_t`, and the following methods should be
defined

~~~{.cpp}
void start_timer(const pt::time_duration &send, ) noexcept override;
void cancel_timer(request_id_t timer_id) noexcept override;

void start() noexcept override {}
void shutdown() noexcept override {}

void enqueue(rotor::message_ptr_t) noexcept override {}
~~~

The `enqueue` method is responsible for puting an `message` into `supervisor`
inbound queue *probably* in **thread-safe manner** to allow accept messages
from other supervisors/loops (thread-safety requirement) or just from some
outer context, when supervisor is still not running on the loop (can be
thread-unsafe). The second requirement is to let the supervisor process
all it's inbound messages *in a loop context*, may be with supervisor-specific
context.

The `start` and `shutdown` are just convenient methods to start processing
messages in event loop context (for `start`) and send a shutdown messages
and process event loop in event loop context .

Here is an skeleton example for `enqueue`:

~~~{.cpp}
void some_supervisor_t::enqueue(message_ptr_t message) noexcept {
    supervisor_ptr_t self{this};    // increase ref-counter
    auto& loop = ...                // get loop somehow, e.g. from loop-specific context
    loop.invoke_later([self = std::move(self), message = std::move(message)]() {
        auto &sup = *self;
        sup.put(std::move(message));    // put message into inbound queue
        sup.do_process();               // process inbound queue
    });
}
~~~

How to get loop and what method invoke on it, is the implementation-specific information.
For example, loop refrence can be passed on `supervisor` constructor. The `invoke_later`
(alternative names: `postpone`, `CallAfter`, `delay`, `dispatch`) is loop-specific
method how to invoke something on in a thread-safe way. Please note, that `supervisor`
instance is captured via intrusive pointer to make sure it is alive in the loop context
invocations.

The timer-related methods are loop- or application-specific.
