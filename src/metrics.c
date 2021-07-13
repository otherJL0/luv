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

/// @submodule uv
#include "luv.h"
#include "util.h"

#if LUV_UV_VERSION_GEQ(1, 39, 0)
/// @function metrics_idle_time
static int luv_metrics_idle_time(lua_State* L) {
  uint64_t idle_time = uv_metrics_idle_time(luv_loop(L));
  lua_pushinteger(L, idle_time);
  return 1;
}
#endif
