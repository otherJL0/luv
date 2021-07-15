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

/*** Poll handle
Poll handles are used to watch file descriptors for readability and writability,
similar to the purpose of [poll(2)](http://linux.die.net/man/2/poll).

The purpose of poll handles is to enable integrating external libraries that
rely on the event loop to signal it about the socket status changes, like c-ares
or libssh2. Using `uv_poll_t` for any other purpose is not recommended;
`uv_tcp_t`, `uv_udp_t`, etc. provide an implementation that is faster and more
scalable than what can be achieved with `uv_poll_t`, especially on Windows.

It is possible that poll handles occasionally signal that a file descriptor is
readable or writable even when it isn't. The user should therefore always be
prepared to handle EAGAIN or equivalent when it attempts to read from or write
to the fd.

It is not okay to have multiple active poll handles for the same socket, this
can cause libuv to busyloop or otherwise malfunction.

The user should not close a file descriptor while it is being polled by an
active poll handle. This can cause the handle to report an error, but it might
also start polling another socket. However the fd can be safely closed
immediately after a call to `uv.poll_stop()` or `uv.close()`.

**Note**: On windows only sockets can be polled with poll handles. On Unix any
file descriptor that would be accepted by poll(2) can be used.

@type poll
*/

static uv_poll_t* luv_check_poll(lua_State* L, int index) {
  uv_poll_t* handle = (uv_poll_t*)luv_checkudata(L, index, "uv_poll");
  luaL_argcheck(L, handle->type == UV_POLL && handle->data, index, "Expected uv_poll_t");
  return handle;
}

/// @function new_poll
static int luv_new_poll(lua_State* L) {
  luv_ctx_t* ctx = luv_context(L);
  int fd = luaL_checkinteger(L, 1);
  uv_poll_t* handle = (uv_poll_t*)luv_newuserdata(L, sizeof(*handle));
  int ret = uv_poll_init(ctx->loop, handle, fd);
  if (ret < 0) {
    lua_pop(L, 1);
    return luv_error(L, ret);
  }
  handle->data = luv_setup_handle(L, ctx);
  return 1;
}

/// @function new_socket_poll
static int luv_new_socket_poll(lua_State* L) {
  luv_ctx_t* ctx = luv_context(L);
  int fd = luaL_checkinteger(L, 1);
  uv_poll_t* handle = (uv_poll_t*)luv_newuserdata(L, sizeof(*handle));
  int ret = uv_poll_init_socket(ctx->loop, handle, fd);
  if (ret < 0) {
    lua_pop(L, 1);
    return luv_error(L, ret);
  }
  handle->data = luv_setup_handle(L, ctx);
  return 1;
}

static const char *const luv_pollevents[] = {
  "r", "w", "rw",
#if LUV_UV_VERSION_GEQ(1, 9, 0)
  "d", "rd", "wd", "rwd",
#endif
#if LUV_UV_VERSION_GEQ(1, 14, 0)
  "p", "rp", "wp", "rwp", "dp", "rdp", "wdp", "rwdp",
#endif
  NULL
};

static void luv_poll_cb(uv_poll_t* handle, int status, int events) {
  luv_handle_t* data = (luv_handle_t*)handle->data;
  lua_State* L = data->ctx->L;
  const char* evtstr;

  if (status < 0) {
    fprintf(stderr, "%s: %s\n", uv_err_name(status), uv_strerror(status));
    lua_pushstring(L, uv_err_name(status));
  }
  else {
    lua_pushnil(L);
  }

  switch (events) {
    case UV_READABLE: evtstr = "r"; break;
    case UV_WRITABLE: evtstr = "w"; break;
    case UV_READABLE|UV_WRITABLE: evtstr = "rw"; break;
#if LUV_UV_VERSION_GEQ(1, 9, 0)
    case UV_DISCONNECT: evtstr = "d"; break;
    case UV_READABLE|UV_DISCONNECT: evtstr = "rd"; break;
    case UV_WRITABLE|UV_DISCONNECT: evtstr = "wd"; break;
    case UV_READABLE|UV_WRITABLE|UV_DISCONNECT: evtstr = "rwd"; break;
#endif
#if LUV_UV_VERSION_GEQ(1, 14, 0)
    case UV_PRIORITIZED: evtstr = "p"; break;
    case UV_READABLE|UV_PRIORITIZED: evtstr = "rp"; break;
    case UV_WRITABLE|UV_PRIORITIZED: evtstr = "wp"; break;
    case UV_READABLE|UV_WRITABLE|UV_PRIORITIZED: evtstr = "rwp"; break;
    case UV_DISCONNECT|UV_PRIORITIZED: evtstr = "dp"; break;
    case UV_READABLE|UV_DISCONNECT|UV_PRIORITIZED: evtstr = "rdp"; break;
    case UV_WRITABLE|UV_DISCONNECT|UV_PRIORITIZED: evtstr = "wdp"; break;
    case UV_READABLE|UV_WRITABLE|UV_DISCONNECT|UV_PRIORITIZED: evtstr = "rwdp"; break;
#endif
    default: evtstr = ""; break;
  }
  lua_pushstring(L, evtstr);

  luv_call_callback(L, data, LUV_POLL, 2);
}

/// @function poll_start
static int luv_poll_start(lua_State* L) {
  uv_poll_t* handle = luv_check_poll(L, 1);
  int events, ret;
  switch (luaL_checkoption(L, 2, "rw", luv_pollevents)) {
    case 0: events = UV_READABLE; break;
    case 1: events = UV_WRITABLE; break;
    case 2: events = UV_READABLE | UV_WRITABLE; break;
#if LUV_UV_VERSION_GEQ(1, 9, 0)
    case 3: events = UV_DISCONNECT; break;
    case 4: events = UV_READABLE|UV_DISCONNECT; break;
    case 5: events = UV_WRITABLE|UV_DISCONNECT; break;
    case 6: events = UV_READABLE|UV_WRITABLE|UV_DISCONNECT; break;
#endif
#if LUV_UV_VERSION_GEQ(1, 14, 0)
    case 7: events = UV_PRIORITIZED; break;
    case 8: events = UV_READABLE|UV_PRIORITIZED; break;
    case 9: events = UV_WRITABLE|UV_PRIORITIZED; break;
    case 10: events = UV_READABLE|UV_WRITABLE|UV_PRIORITIZED; break;
    case 11: events = UV_DISCONNECT|UV_PRIORITIZED; break;
    case 12: events = UV_READABLE|UV_DISCONNECT|UV_PRIORITIZED; break;
    case 13: events = UV_WRITABLE|UV_DISCONNECT|UV_PRIORITIZED; break;
    case 14: events = UV_READABLE|UV_WRITABLE|UV_DISCONNECT|UV_PRIORITIZED; break;
#endif
    default: events = 0; /* unreachable */
  }
  luv_check_callback(L, (luv_handle_t*)handle->data, LUV_POLL, 3);
  ret = uv_poll_start(handle, events, luv_poll_cb);
  return luv_result(L, ret);
}

/// @function poll_stop
static int luv_poll_stop(lua_State* L) {
  uv_poll_t* handle = luv_check_poll(L, 1);
  int ret = uv_poll_stop(handle);
  return luv_result(L, ret);
}
