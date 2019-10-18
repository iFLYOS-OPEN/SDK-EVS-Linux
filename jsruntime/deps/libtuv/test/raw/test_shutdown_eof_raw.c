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

#include <stdio.h>
#include <stdlib.h>

#include <uv.h>
#include "runner.h"


#if defined(__TUV_HOST_IPEXIST__)
#include "tuv_host_ipaddress.h"
#else
#pragma message("Please create tuv_host_ipaddress.h for shutdown_eof test.")
#endif

// Please create a file with your host IP address like below.
// This information is use to connect to host that should be running
// "tuvhelper" prgoram.
/*
#ifndef __tuv_host_ipaddress_header__
#define __tuv_host_ipaddress_header__

#define HOST_IP_ADDRESS   "192.168.1.2"

#endif
*/

//-----------------------------------------------------------------------------

#if defined(__TUV_HOST_IPEXIST__)
static uv_tcp_t tcp;
static uv_connect_t connect_req;
static uv_write_t write_req;
static uv_shutdown_t shutdown_req;
static int got_q;
static int got_eof;

static int called_connect_cb;
static int called_shutdown_cb;


static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  buf->base = (char*)malloc(size);
  buf->len = size;
}


static void read_cb(uv_stream_t* t, ssize_t nread, const uv_buf_t* buf) {
  TUV_ASSERT((uv_tcp_t*)t == &tcp);


  if (buf->base)
    free(buf->base);

  if (nread == 0) {
    return;
  }

  if (nread > 0) {
    TUV_ASSERT(0);
    //TUV_ASSERT(!got_eof);
    //TUV_ASSERT(buf->base[0] == 'Q');
    //uv_buf_t qbuf = uv_buf_init((char*)"Q", 1);
    //int r = uv_write(&write_req, t, &qbuf, 1, NULL);
    //TUV_ASSERT(r == 0);
    //got_q = 1;
  } else {
    TUV_ASSERT(nread == UV_EOF);
    uv_close((uv_handle_t*)t, NULL);
    got_eof = 1;
  }
}


static void shutdown_cb(uv_shutdown_t *req, int status) {
  TUV_ASSERT(req == &shutdown_req);
  TUV_ASSERT(called_connect_cb == 1);
  TUV_ASSERT(!got_eof);

  called_shutdown_cb++;
}


static void connect_cb(uv_connect_t *req, int status) {
  TUV_ASSERT(status == 0);
  TUV_ASSERT(req == &connect_req);
  int r;

  /* Start reading from our connection so we can receive the EOF.  */
  r = uv_read_start((uv_stream_t*)&tcp, alloc_cb, read_cb);
  TUV_ASSERT(r == 0);

  /*
   * Write the letter 'Q' to gracefully kill the echo-server. This will not
   * effect our connection.
   */
  uv_buf_t qsbuf = uv_buf_init((char*)"Q", 1);
  r = uv_write(&write_req, (uv_stream_t*) &tcp, &qsbuf, 1, NULL);
  TUV_ASSERT(r == 0);

  /* Shutdown our end of the connection.  */
  //r = uv_shutdown(&shutdown_req, (uv_stream_t*) &tcp, shutdown_cb);
  //TUV_ASSERT(r == 0);

  called_connect_cb++;
  TUV_ASSERT(called_shutdown_cb == 0);
}


typedef struct {
  uv_loop_t* loop;
} shutdown_eof_param_t;

static int shutdown_eof_loop(void* vparam) {
  shutdown_eof_param_t* param = (shutdown_eof_param_t*)vparam;
  return uv_run(param->loop, UV_RUN_ONCE);
}

static int shutdown_eof_final(void* vparam) {
  shutdown_eof_param_t* param = (shutdown_eof_param_t*)vparam;

  TUV_ASSERT(0 == uv_loop_close(param->loop));

  TUV_ASSERT(1 == called_connect_cb);

  // cleanup tuv param
  free(param);

  // jump to next test
  run_tests_continue();

  return 0;
}
#endif

/*
 * This test has a client which connects to the echo_server and immediately
 * issues a shutdown. The echo-server, in response, will also shutdown their
 * connection. We check, with a timer, that libuv is not automatically
 * calling uv_close when the client receives the EOF from echo-server.
 */
TEST_IMPL(shutdown_eof) {
#if defined(__TUV_HOST_IPEXIST__)
  shutdown_eof_param_t* param;
  param = (shutdown_eof_param_t*)malloc(sizeof(shutdown_eof_param_t));
  param->loop = uv_default_loop();

  struct sockaddr_in server_addr;
  int r;

  got_q = 0;
  got_eof = 0;
  called_connect_cb = 0;
  called_shutdown_cb = 0;

  r = uv_ip4_addr(HOST_IP_ADDRESS, TEST_PORT, &server_addr);
  TUV_ASSERT(r == 0);
  r = uv_tcp_init(param->loop, &tcp);
  TUV_ASSERT(r == 0);
  r = uv_tcp_connect(&connect_req,
                     &tcp,
                     (const struct sockaddr*) &server_addr,
                     connect_cb);
  TUV_ASSERT(r == 0);

  tuv_run(param->loop, shutdown_eof_loop, shutdown_eof_final, param);
#else
  TDDLOG("'HOST_IP_ADDRESS' not defined. skip test.\r\n");
  run_tests_continue();
#endif
  return 0;
}
