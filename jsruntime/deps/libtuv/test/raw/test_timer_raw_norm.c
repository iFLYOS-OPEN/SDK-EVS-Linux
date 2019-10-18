/* Copyright 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <uv.h>

#include "runner.h"


static int once_cb_called = 0;
static int once_close_cb_called = 0;
static int repeat_cb_called = 0;
static int repeat_close_cb_called = 0;

static uint64_t start_time = 0ll;


static void once_close_cb(uv_handle_t* handle) {
  TUV_ASSERT(handle != NULL);
  TUV_ASSERT(0 == uv_is_active(handle));

  once_close_cb_called++;
}

static void once_cb(uv_timer_t* handle) {
  TUV_ASSERT(handle != NULL);
  TUV_ASSERT(0 == uv_is_active((uv_handle_t*) handle));

  once_cb_called++;

  uv_close((uv_handle_t*)handle, once_close_cb);

  /* Just call this randomly for the code coverage. */
  uv_update_time(uv_default_loop());
}


static void repeat_close_cb(uv_handle_t* handle) {
  TUV_ASSERT(handle != NULL);

  repeat_close_cb_called++;
}


static void repeat_cb(uv_timer_t* handle) {
  TUV_ASSERT(handle != NULL);
  TUV_ASSERT(1 == uv_is_active((uv_handle_t*) handle));

  repeat_cb_called++;

  if (repeat_cb_called == 5) {
    uv_close((uv_handle_t*)handle, repeat_close_cb);
  }
}


static void never_cb(uv_timer_t* handle) {
  TUV_FATAL("never_cb should never be called");
}


//-----------------------------------------------------------------------------

typedef struct {
  uv_loop_t* loop;
  uv_timer_t once_timers[10];
  uv_timer_t repeat, never;
} timer_param_t;


static int timer_loop(void* vparam) {
  timer_param_t* param = (timer_param_t*)vparam;
  return uv_run(param->loop, UV_RUN_ONCE);
}

static int timer_final(void* vparam) {
  timer_param_t* param = (timer_param_t*)vparam;

  TUV_ASSERT(once_cb_called == 10);
  TUV_ASSERT(once_close_cb_called == 10);
  TUV_ASSERT(repeat_cb_called == 5);
  TUV_ASSERT(repeat_close_cb_called == 1);

  TUV_ASSERT(500 <= uv_now(param->loop) - start_time);

  // for platforms that needs cleaning
  TUV_ASSERT(0 == uv_loop_close(param->loop));

  // cleanup tuv param
  free(param);

  // jump to next test
  run_tests_continue();

  return 0;
}

TEST_IMPL(timer) {
  timer_param_t* param = (timer_param_t*)malloc(sizeof(timer_param_t));
  param->loop = uv_default_loop();

  uv_timer_t *once;
  unsigned int i;
  int r;

  once_cb_called = 0;
  once_close_cb_called = 0;
  repeat_cb_called = 0;
  repeat_close_cb_called = 0;

  start_time = uv_now(param->loop);
  TUV_ASSERT(0 < start_time);

  /* Let 10 timers time out in 500 ms total. */
  for (i = 0; i < ARRAY_SIZE(param->once_timers); i++) {
    once = param->once_timers + i;
    r = uv_timer_init(param->loop, once);
    TUV_ASSERT(r == 0);
    r = uv_timer_start(once, once_cb, i * 50, 0);
    TUV_ASSERT(r == 0);
  }

  /* The 11th timer is a repeating timer that runs 4 times */
  r = uv_timer_init(param->loop, &param->repeat);
  TUV_ASSERT(r == 0);
  r = uv_timer_start(&param->repeat, repeat_cb, 100, 100);
  TUV_ASSERT(r == 0);

  /* The 12th timer should not do anything. */
  r = uv_timer_init(param->loop, &param->never);
  TUV_ASSERT(r == 0);
  r = uv_timer_start(&param->never, never_cb, 100, 100);
  TUV_ASSERT(r == 0);
  r = uv_timer_stop(&param->never);
  TUV_ASSERT(r == 0);
  uv_unref((uv_handle_t*)&param->never);
  // for platforms that needs cleaning
  uv_close((uv_handle_t*) &param->never, NULL);

#if 1
  tuv_run(param->loop, timer_loop, timer_final, param);
#else
  loop->raw_loopcb = timer_loop;
  loop->raw_finalcb = timer_final;
  loop->raw_param = param;
  while(true) {
    int r = timer_loop(param);
    if (!r) break;
  }
  timer_final(param);
#endif

  return 0;
}
