/*
 *  Copyright 2014 The Luvit Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
/// @module uv
#include "private.h"

/***
Event loop.
The event loop is the central part of libuv's functionality. It takes care of
polling for I/O and scheduling callbacks to be run based on different sources of
events.

In luv, there is an implicit uv loop for every Lua state that loads the library.
You can use this library in an multi-threaded environment as long as each thread
has it's own Lua state with its corresponding own uv loop. This loop is not
directly exposed to users in the Lua module.
@section uv_loop
*/

/***
Closes all internal loop resources. In normal execution, the loop will
automatically be closed when it is garbage collected by Lua, so it is not
necessary to explicitly call `loop_close()`. Call this function only after the
loop has finished executing and all open handles and requests have been closed,
or it will return `EBUSY`.
@function loop_close
@return 0
@error
*/
static int luv_loop_close(lua_State* L) {
  int ret = uv_loop_close(luv_loop(L));
  if (ret < 0) return luv_error(L, ret);
  luv_set_loop(L, NULL);
  lua_pushinteger(L, ret);
  return 1;
}

// These are the same order as uv_run_mode which also starts at 0
static const char *const luv_runmodes[] = {
  "default", "once", "nowait", NULL
};

/***
This function runs the event loop. It will act differently depending on the
specified mode:

  - `"default"`: Runs the event loop until there are no more active and
  referenced handles or requests. Returns `true` if @{stop|uv.stop()} was called and
  there are still active handles or requests. Returns `false` in all other
  cases.

  - `"once"`: Poll for I/O once. Note that this function blocks if there are no
  pending callbacks. Returns `false` when done (no active handles or requests
  left), or `true` if more callbacks are expected (meaning you should run the
  event loop again sometime in the future).

  - `"nowait"`: Poll for I/O once but don't block if there are no pending
  callbacks. Returns `false` if done (no active handles or requests left),
  or `true` if more callbacks are expected (meaning you should run the event
  loop again sometime in the future).

Note: Luvit will implicitly call `uv.run()` after loading user code, but if
you use the luv bindings directly, you need to call this after registering
your initial set of event callbacks to start the event loop.
@function run
@tparam[opt='default'] string mode
@treturn bool
@error
*/
static int luv_run(lua_State* L) {
  int mode = luaL_checkoption(L, 1, "default", luv_runmodes);
  luv_ctx_t* ctx = luv_context(L);
  ctx->mode = mode;
  int ret = uv_run(ctx->loop, (uv_run_mode)mode);
  ctx->mode = -1;
  if (ret < 0) return luv_error(L, ret);
  lua_pushboolean(L, ret);
  return 1;
}

/***
If the loop is running, returns a string indicating the mode in use. If the loop
is not running, `nil` is returned instead.
@function loop_mode
@return 0
*/
static int luv_loop_mode(lua_State* L) {
  luv_ctx_t* ctx = luv_context(L);
  if (ctx->mode == -1) {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, luv_runmodes[ctx->mode]);
  }
  return 1;
}

/***
Returns `true` if there are referenced active handles, active requests, or
closing handles in the loop; otherwise, `false`.
@function loop_alive
@treturn bool
@error
*/
static int luv_loop_alive(lua_State* L) {
  int ret = uv_loop_alive(luv_loop(L));
  if (ret < 0) return luv_error(L, ret);
  lua_pushboolean(L, ret);
  return 1;
}

/***
Stop the event loop, causing @{run|uv.run()} to end as soon as possible. This
will happen not sooner than the next loop iteration. If this function was called
before blocking for I/O, the loop won't block for I/O on this iteration.
@function stop
*/
static int luv_stop(lua_State* L) {
  uv_stop(luv_loop(L));
  return 0;
}

/***
Get backend file descriptor. Only kqueue, epoll, and event ports are supported.

This can be used in conjunction with @{run|uv.run("nowait")} to poll in one thread
and run the event loop's callbacks in another
@function backend_fd
@treturn integer
@error
*/
static int luv_backend_fd(lua_State* L) {
  int ret = uv_backend_fd(luv_loop(L));
  // -1 is returned when there is no backend fd (like on Windows)
  if (ret == -1)
    lua_pushnil(L);
  else
    lua_pushinteger(L, ret);
  return 1;
}

/***
Get the poll timeout. The return value is in milliseconds, or -1 for no timeout.
@function backend_timeout
@treturn integer
*/
static int luv_backend_timeout(lua_State* L) {
  int ret = uv_backend_timeout(luv_loop(L));
  lua_pushinteger(L, ret);
  return 1;
}

/***
Returns the current timestamp in milliseconds. The timestamp is cached at the
start of the event loop tick, see @{update_time|uv.update_time()} for details and rationale.

The timestamp increases monotonically from some arbitrary point in time. Don't
make assumptions about the starting point, you will only get disappointed.

Note: Use @{hrtime|uv.hrtime()} if you need sub-millisecond granularity.
@function now
@treturn integer
*/
static int luv_now(lua_State* L) {
  uint64_t now = uv_now(luv_loop(L));
  lua_pushinteger(L, now);
  return 1;
}

/***
Update the event loop's concept of "now". Libuv caches the current time at the
start of the event loop tick in order to reduce the number of time-related
system calls.

You won't normally need to call this function unless you have callbacks that
block the event loop for longer periods of time, where "longer" is somewhat
subjective but probably on the order of a millisecond or more.
@function update_time
*/
static int luv_update_time(lua_State* L) {
  uv_update_time(luv_loop(L));
  return 0;
}

static void luv_walk_cb(uv_handle_t* handle, void* arg) {
  lua_State* L = (lua_State*)arg;
  luv_handle_t* data = (luv_handle_t*)handle->data;

  // Sanity check
  // Most invalid values are large and refs are small, 0x1000000 is arbitrary.
  assert(data && data->ref < 0x1000000);

  lua_pushvalue(L, 1);           // Copy the function
  luv_find_handle(L, data);      // Get the userdata
  data->ctx->pcall(L, 1, 0, 0);  // Call the function
}

/***
Walk the list of handles: `callback` will be executed with each handle.
@function walk
@tparam function callable
@usage
uv.walk(function (handle)
  if not handle:is_closing() then
    handle:close()
  end
end)
*/
static int luv_walk(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  uv_walk(luv_loop(L), luv_walk_cb, L);
  return 0;
}

#if LUV_UV_VERSION_GEQ(1, 0, 2)
static const char *const luv_loop_configure_options[] = {
  "block_signal",
#if LUV_UV_VERSION_GEQ(1, 39, 0)
  "metrics_idle_time",
#endif
  NULL
};

/***
Set additional loop options. You should normally call this before the first call
to uv_run() unless mentioned otherwise.

Supported options:

  - `"block_signal"`: Block a signal when polling for new events. The second argument
  to loop_configure() is the signal name (as a lowercase string) or the signal number.
  This operation is currently only implemented for `"sigprof"` signals, to suppress
  unnecessary wakeups when using a sampling profiler. Requesting other signals will
  fail with `EINVAL`.
  - `"metrics_idle_time"`: Accumulate the amount of idle time the event loop spends
  in the event provider. This option is necessary to use `metrics_idle_time()`.

@function loop_configure
@tparam string option
@param[opt] ... depends on option
@usage uv.loop_configure("block_signal", "sigprof")
@return 0
@error
*/
static int luv_loop_configure(lua_State* L) {
  uv_loop_t* loop = luv_loop(L);
  uv_loop_option option = 0;
  int ret = 0;
  switch (luaL_checkoption(L, 1, NULL, luv_loop_configure_options)) {
  case 0: option = UV_LOOP_BLOCK_SIGNAL; break;
#if LUV_UV_VERSION_GEQ(1, 39, 0)
  case 1: option = UV_METRICS_IDLE_TIME; break;
#endif
  default: break; /* unreachable */
  }
  if (option == UV_LOOP_BLOCK_SIGNAL) {
    // lua_isstring checks for string or number
    int signal;
    luaL_argcheck(L, lua_isstring(L, 2), 2, "block_signal option: expected signal as string or number");
    signal = luv_parse_signal(L, 2);
    ret = uv_loop_configure(loop, UV_LOOP_BLOCK_SIGNAL, signal);
  } else {
    ret = uv_loop_configure(loop, option);
  }
  return luv_result(L, ret);
}
#endif
