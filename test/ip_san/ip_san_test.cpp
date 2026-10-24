#include "security/IpSanIdentity.h"
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
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

int main(int argc, char **argv)
{
    require(argc == 2, "Supply disposable fixture directory");
    require(mbedtls_version_get_number() == 0x021C0700, "Wrong mbedTLS version");
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
}
