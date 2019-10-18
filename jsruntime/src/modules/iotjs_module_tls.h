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


#ifndef IOTJS_MODULE_TLS_H
#define IOTJS_MODULE_TLS_H

#include "iotjs_def.h"
#include "mbedtls/certs.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#pragma GCC diagnostic ignored "-Wcpp"
#include "mbedtls/net.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

// Default certificate
const char SSL_CA_PEM[] =
        //i:C = US, O = DigiCert Inc, OU = www.digicert.com, CN = DigiCert Global Root CA
        "-----BEGIN CERTIFICATE-----\n"
        "MIIEizCCA3OgAwIBAgIQBUb+GCP34ZQdo5/OFMRhczANBgkqhkiG9w0BAQsFADBh\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
        "QTAeFw0xNzExMDYxMjIzNDVaFw0yNzExMDYxMjIzNDVaMF4xCzAJBgNVBAYTAlVT\n"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
        "b20xHTAbBgNVBAMTFEdlb1RydXN0IFJTQSBDQSAyMDE4MIIBIjANBgkqhkiG9w0B\n"
        "AQEFAAOCAQ8AMIIBCgKCAQEAv4rRY03hGOqHXegWPI9/tr6HFzekDPgxP59FVEAh\n"
        "150Hm8oDI0q9m+2FAmM/n4W57Cjv8oYi2/hNVEHFtEJ/zzMXAQ6CkFLTxzSkwaEB\n"
        "2jKgQK0fWeQz/KDDlqxobNPomXOMJhB3y7c/OTLo0lko7geG4gk7hfiqafapa59Y\n"
        "rXLIW4dmrgjgdPstU0Nigz2PhUwRl9we/FAwuIMIMl5cXMThdSBK66XWdS3cLX18\n"
        "4ND+fHWhTkAChJrZDVouoKzzNYoq6tZaWmyOLKv23v14RyZ5eqoi6qnmcRID0/i6\n"
        "U9J5nL1krPYbY7tNjzgC+PBXXcWqJVoMXcUw/iBTGWzpwwIDAQABo4IBQDCCATww\n"
        "HQYDVR0OBBYEFJBY/7CcdahRVHex7fKjQxY4nmzFMB8GA1UdIwQYMBaAFAPeUDVW\n"
        "0Uy7ZvCj4hsbw5eyPdFVMA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEF\n"
        "BQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADA0BggrBgEFBQcBAQQo\n"
        "MCYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBCBgNVHR8E\n"
        "OzA5MDegNaAzhjFodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRHbG9i\n"
        "YWxSb290Q0EuY3JsMD0GA1UdIAQ2MDQwMgYEVR0gADAqMCgGCCsGAQUFBwIBFhxo\n"
        "dHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMA0GCSqGSIb3DQEBCwUAA4IBAQAw\n"
        "8YdVPYQI/C5earp80s3VLOO+AtpdiXft9OlWwJLwKlUtRfccKj8QW/Pp4b7h6QAl\n"
        "ufejwQMb455OjpIbCZVS+awY/R8pAYsXCnM09GcSVe4ivMswyoCZP/vPEn/LPRhH\n"
        "hdgUPk8MlD979RGoUWz7qGAwqJChi28uRds3thx+vRZZIbEyZ62No0tJPzsSGSz8\n"
        "nQ//jP8BIwrzBAUH5WcBAbmvgWfrKcuv+PyGPqRcc4T55TlzrBnzAzZ3oClo9fTv\n"
        "O9PuiHMKrC6V6mgi0s2sa/gbXlPCD9Z24XUMxJElwIVTDuKB0Q4YMMlnpN/QChJ4\n"
        "B0AFsQ+DU0NCO+f78Xf7\n"
        "-----END CERTIFICATE-----";
//    "-----BEGIN CERTIFICATE-----\n"
//    "MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4GA1UECxMX\n"
//    "R2xvYmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMT\n"
//    "Ckdsb2JhbFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1MDgwMDAwWjBMMSAwHgYDVQQL\n"
//    "ExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEGA1UEChMKR2xvYmFsU2lnbjETMBEGA1UE\n"
//    "AxMKR2xvYmFsU2lnbjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6+Lm8o\n"
//    "mUVCxKs+IVSbC9N/hHD6ErPLv4dfxn+G07IwXNb9rfF73OX4YJYJkhD10FPe+3t+c4isUoh7\n"
//    "SqbKSaZeqKeMWhG8eoLrvozps6yWJQeXSpkqBy+0Hne/ig+1AnwblrjFuTosvNYSuetZfeLQ\n"
//    "BoZfXklqtTleiDTsvHgMCJiEbKjNS7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzd\n"
//    "C9XZzPnqJworc5HGnRusyMvo4KD0L5CLTfuwNhv2GXqF4G3yYROIXJ/gkwpRl4pazq+r1feq\n"
//    "CapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6CygPCm48CAwEAAaOBnDCBmTAOBgNVHQ8BAf8E\n"
//    "BAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUm+IHV2ccHsBqBt5ZtJot39wZhi4w\n"
//    "NgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5nbG9iYWxzaWduLm5ldC9yb290LXIyLmNy\n"
//    "bDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG3lm0mi3f3BmGLjANBgkqhkiG9w0BAQUFAAOCAQEA\n"
//    "mYFThxxol4aR7OBKuEQLq4GsJ0/WwbgcQ3izDJr86iw8bmEbTUsp9Z8FHSbBuOmDAGJFtqkI\n"
//    "k7mpM0sYmsL4h4hO291xNBrBVNpGP+DTKqttVCL1OmLNIG+6KYnX3ZHu01yiPqFbQfXf5WRD\n"
//    "LenVOavSot+3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h+u/N5GJG79G+dwfCMNYxd\n"
//    "AfvDbbnvRG15RjF+Cv6pgsH/76tuIMRQyV+dTZsXjAzlAcmgQWpzU/qlULRuJQ/7TBj0/VLZ\n"
//    "jmmx6BEP3ojY+x1J96relc8geMJgEtslQIxq/H5COEBkEveegeGTLg==\n"
//    "-----END CERTIFICATE-----\n";


// Handshake states for tls context
enum {
  TLS_HANDSHAKE_READY = 0,
  TLS_HANDSHAKE_IN_PROGRESS = 1,
  TLS_CONNECTED = 2,
  TLS_CLOSED = 3
};

typedef struct {
  char *mem;
  size_t size;
  size_t read_index;
  size_t write_index;
} iotjs_bio_t;

typedef struct {
  iotjs_bio_t receive_bio;
  iotjs_bio_t send_bio;
} iotjs_bio_pair_t;

enum {
  SSL_BIO_SUCCESS = 0,
  SSL_BIO_ERROR = -1,
  SSL_BIO_UNSET = -2,
  SSL_BIO_SIZE = 4096
};

enum { SSL_CONTEXT_HAS_KEY = (1 << 0) };

typedef struct {
  int ref_count;
  uint32_t context_flags;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_pk_context pkey;
  mbedtls_x509_crt own_cert;
  mbedtls_x509_crt cert_auth;
} iotjs_tls_context_t;

typedef struct {
  jerry_value_t jobject;
  int state;

  iotjs_tls_context_t *tls_context;
  mbedtls_ssl_config conf;
  mbedtls_ssl_context ssl;

  iotjs_bio_pair_t bio;
} iotjs_tls_t;

#endif /* IOTJS_MODULE_TLS_H */
