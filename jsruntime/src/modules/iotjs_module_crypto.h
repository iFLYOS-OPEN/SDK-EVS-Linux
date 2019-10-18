/* Copyright 2018-present Samsung Electronics Co., Ltd. and other contributors
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
#ifndef IOTJS_MODULE_CRYPTO_H
#define IOTJS_MODULE_CRYPTO_H

size_t iotjs_sha1_encode(unsigned char **out_buff, const unsigned char *in_buff,
                         size_t buff_len);
size_t iotjs_sha256_encode(unsigned char **out_buff,
                           const unsigned char *in_buff, size_t buff_len);
#endif /* IOTJS_MODULE_CRYPTO_H */
