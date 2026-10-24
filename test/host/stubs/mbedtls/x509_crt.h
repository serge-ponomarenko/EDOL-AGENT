#pragma once
#include <cstddef>
#include <cstdint>
#include "x509.h"
#define MBEDTLS_ASN1_CONTEXT_SPECIFIC 0x80
#define MBEDTLS_X509_SAN_IP_ADDRESS 7
struct mbedtls_x509_buf { int tag = 0; size_t len = 0; unsigned char *p = nullptr; };
struct mbedtls_x509_sequence { mbedtls_x509_buf buf; mbedtls_x509_sequence *next = nullptr; };
struct mbedtls_x509_crt { mbedtls_x509_sequence subject_alt_names; };
