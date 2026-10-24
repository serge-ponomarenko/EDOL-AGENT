#include "security/IpSanIdentity.h"
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
#include <mbedtls/platform.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Certificate
{
    mbedtls_x509_crt value;
    explicit Certificate(const std::string &path)
    {
        mbedtls_x509_crt_init(&value);
        require(mbedtls_x509_crt_parse_file(&value, path.c_str()) == 0, "Fixture parse failed");
    }
    ~Certificate() { mbedtls_x509_crt_free(&value); }
};

size_t allocationAttempt = 0, failAllocation = 0, blockedAllocations = 0;
bool armedAllocator = false;

void *testCalloc(size_t count, size_t size)
{
    if (armedAllocator && ++allocationAttempt == failAllocation)
    {
        ++blockedAllocations;
        return nullptr;
    }
    return std::calloc(count, size);
}

struct VerificationTrace
{
    unsigned char ip[4] = {192, 0, 2, 10};
    unsigned depths = 0;
};

int tracedVerify(void *context, mbedtls_x509_crt *certificate, int depth, uint32_t *flags)
{
    auto *trace = static_cast<VerificationTrace *>(context);
    trace->depths |= 1U << depth;
    return IpSanIdentity::verify(trace->ip, certificate, depth, flags);
}

void memoryRegression(const std::string &directory)
{
    struct Result { int status; uint32_t flags; size_t allocations; size_t blocked; unsigned depths; };
    auto run = [&](size_t failAt) {
        // Load the identical leaf and correct CA before injecting a verifier allocation failure.
        Certificate root(directory + "/ca.pem"), leaf(directory + "/ip.pem");
        VerificationTrace trace;
        uint32_t flags = UINT32_MAX;
        allocationAttempt = blockedAllocations = 0;
        failAllocation = failAt;
        armedAllocator = true;
        int status = mbedtls_x509_crt_verify(&leaf.value, &root.value, nullptr, "192.0.2.10",
                                           &flags, tracedVerify, &trace);
        armedAllocator = false;
        return Result{status, flags, allocationAttempt, blockedAllocations, trace.depths};
    };
    const auto probe = run(0);
    require(probe.status == 0 && probe.flags == 0 && probe.depths == 3, "Correct-chain probe failed");
    size_t maskedFailure = 0;
    for (size_t index = 1; index <= probe.allocations; ++index)
    {
        const auto upload = run(index);
        if (upload.blocked == 1 && upload.status == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED
            && upload.flags == MBEDTLS_X509_BADCERT_NOT_TRUSTED && upload.depths == 1)
        {
            maskedFailure = index;
            break;
        }
    }
    require(maskedFailure != 0, "Did not reproduce allocation failure masked as leaf-only NOT_TRUSTED");
    for (unsigned attempt = 0; attempt < 3; ++attempt)
    {
        const auto pressure = run(maskedFailure);
        require(pressure.status == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED
                && pressure.flags == MBEDTLS_X509_BADCERT_NOT_TRUSTED && pressure.depths == 1,
                "Memory pressure must fail closed with the observed leaf-only trust flag");
        const auto released = run(0);
        require(released.status == 0 && released.flags == 0 && released.depths == 3,
                "Releasing memory did not restore normal chain verification");
    }
    std::cout << "PASS memory-pressure regression: correct CA/identical leaf, fault_at=" << maskedFailure
              << ", leaf-only NOT_TRUSTED, 3 rejection/recovery cycles\n";
}

int main(int argc, char **argv)
{
    require(argc == 2, "Supply disposable fixture directory");
    require(mbedtls_version_get_number() == 0x021C0700, "Wrong mbedTLS version");
    require(mbedtls_platform_set_calloc_free(testCalloc, std::free) == 0, "Allocator setup failed");
    const std::string directory = argv[1];
    Certificate root(directory + "/ca.pem"), foreign(directory + "/foreign-ca.pem");
    unsigned tests = 0;
    auto verify = [&](const char *file, const char *host, bool patch, bool trusted, uint32_t expected) {
        Certificate leaf(directory + "/" + file + ".pem");
        unsigned char ip[4] = {};
        if (patch) require(IpSanIdentity::parse(host, ip), "Invalid test IP");
        uint32_t flags = UINT32_MAX;
        int result = mbedtls_x509_crt_verify(&leaf.value, trusted ? &root.value : &foreign.value, nullptr,
            host, &flags, patch ? IpSanIdentity::verify : nullptr, patch ? ip : nullptr);
        require(flags == expected, file);
        require((result == 0) == (expected == 0), "Verifier result does not match flags");
        std::cout << "PASS " << file << " host=" << host << " callback=" << patch << " flags=" << flags << '\n';
        ++tests;
    };
    verify("ip", "192.0.2.10", false, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("ip", "192.0.2.10", true, true, 0);
    verify("ip", "192.0.2.11", true, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("wrong-ip", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("cn-only", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("dns-ip", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("malformed", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("mixed", "192.0.2.10", true, true, 0);
    verify("dns", "core.example.test", false, true, 0);
    verify("dns", "other.example.test", false, true, MBEDTLS_X509_BADCERT_CN_MISMATCH);
    verify("ip", "192.0.2.10", true, false, MBEDTLS_X509_BADCERT_NOT_TRUSTED);
    verify("expired", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_EXPIRED);
    verify("future", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_FUTURE);
    verify("bad-signature", "192.0.2.10", true, true, MBEDTLS_X509_BADCERT_NOT_TRUSTED);

    // Verify the exact SDK hook order: callback before ssl_setup, hostname still supplied.
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    require(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT) == 0, "TLS defaults failed");
    mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
    unsigned char ip[4];
    require(IpSanIdentity::parse("192.0.2.10", ip), "IP parse failed");
    mbedtls_ssl_set_verify(&ssl, IpSanIdentity::verify, ip);
    require(mbedtls_ssl_set_hostname(&ssl, "192.0.2.10") == 0, "Hostname setup failed");
    require(mbedtls_ssl_setup(&ssl, &config) == 0, "TLS setup failed");
    require(ssl.f_vrfy == IpSanIdentity::verify && ssl.p_vrfy == ip, "TLS setup dropped callback");
    require(std::strcmp(ssl.hostname, "192.0.2.10") == 0, "Hostname was disabled/changed");
    require(config.authmode == MBEDTLS_SSL_VERIFY_REQUIRED, "Required verification was disabled");
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    ++tests;

    // Malformed/ambiguous IP literals cannot become DNS-name or CN matches.
    for (const char *host : {"192.0.2.010", "192.0.2", "256.0.2.1", "192.0.2.1.evil", "192.0.2.1 ", "", "::1"})
        require(!IpSanIdentity::parse(host, ip), "Ambiguous IP accepted");
    uint32_t flags = MBEDTLS_X509_BADCERT_CN_MISMATCH | MBEDTLS_X509_BADCERT_EXPIRED;
    Certificate leaf(directory + "/ip.pem");
    require(IpSanIdentity::parse("192.0.2.10", ip), "IP parse failed");
    IpSanIdentity::verify(ip, &leaf.value, 1, &flags);
    require(flags == (MBEDTLS_X509_BADCERT_CN_MISMATCH | MBEDTLS_X509_BADCERT_EXPIRED), "Non-leaf flags changed");
    ++tests;
    std::cout << tests << " real-vendor verification cases PASS (no device or live endpoint)\n";
    memoryRegression(directory);
}
