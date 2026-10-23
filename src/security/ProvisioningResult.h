#pragma once

enum class ProvisioningResult
{
    Success, WifiTimeout, ClockTimeout, TlsFailed, InvalidEndpoint, RateLimited,
    CodeRejected, EnrollmentUncertain, MqttUncertain, BrokerFailed
};

inline const char *provisioningMessage(ProvisioningResult result)
{
    switch (result)
    {
    case ProvisioningResult::Success: return "Step completed.";
    case ProvisioningResult::WifiTimeout: return "Wi-Fi connection failed. Check SSID/password or wait for the network. Retrying.";
    case ProvisioningResult::ClockTimeout: return "Time synchronization failed. Allow NTP (UDP 123). Retrying.";
    case ProvisioningResult::TlsFailed: return "TLS connection or certificate verification failed. Check endpoint, CA and time. Retrying without sending credentials.";
    case ProvisioningResult::InvalidEndpoint: return "Invalid Core/MQTT host or port. Edit the saved settings.";
    case ProvisioningResult::RateLimited: return "Core rate limit reached. Waiting before retry.";
    case ProvisioningResult::CodeRejected: return "Pairing code expired, rejected or missing. Enter a fresh owner-issued code; saved settings are retained.";
    case ProvisioningResult::EnrollmentUncertain: return "Enrollment response or credential storage failed. Code cleared. Check Agent status in EDOL; replace if enrolled, then enter a fresh code.";
    case ProvisioningResult::MqttUncertain: return "MQTT credential issuance was attempted but no usable secret was saved. Owner replacement is required; saved settings and identity are retained.";
    case ProvisioningResult::BrokerFailed: return "MQTT connection/subscription failed. Saved credentials are retained. Check broker TLS/authority; retrying.";
    }
    return "Setup failed.";
}
