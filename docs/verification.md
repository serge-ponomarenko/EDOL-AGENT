# Verification and publication record

Date: 2026-10-21. Target: `lolin_d32`, Arduino, one printer.

## Executed checks

- `pio run -e lolin_d32`: successful firmware build; no upload.
- PlatformIO Core 6.2.0; Espressif32 6.13.0; Arduino ESP32
  3.20017.241212; Xtensa GCC 8.4.0; PubSubClient 2.8.0; ArduinoJson 7.4.3.
- `python scripts/run_host_tests.py`: two native C++ executables passed, compiled
  with MSVC warnings as errors. The actual HTTP upload and AgentService sources
  run against fake sockets/services. Coverage includes streamed bytes, filename
  encoding, unchanged upload headers/endpoints, response errors, timeout,
  incomplete/short writes, connection closure, abort/reuse, scope ownership,
  every model failure stage and snapshot health/resource/operation release.
- Preferences/configuration implementation, main startup/reconnect loop,
  heartbeat and factory-reset detector compared byte-for-byte with the working
  implementation saved before this task. No namespace, key or reset changes.
- MQTT/upload protocol literals reviewed against that same working snapshot.
  Log messages changed; topics, payload fields, credential formats and endpoints
  did not.
- Publication uses the exact paths in `publication-files.txt`. Review includes
  known private values from local configuration and original history, generic
  secret detection with Gitleaks, and every outgoing commit tree.
- Gitleaks 8.30.1: no generic findings in the 49-file publication snapshot.
  Known-value review found nine private values in the original baseline even
  though generic history scanning reported no findings. Generic detection alone
  is insufficient for environment-specific credentials.

## History boundary

Original repository baseline: `d74956f5dbe17bca86ea1d02246f42c4c56bf270`.
Its configuration contains private values. It is retained locally and must not
be pushed. The publication directory uses fresh Git history, without the
original `.git`, generated files or local configuration. A local cleanup commit
in the original repository does not make its ancestor safe to publish.

Previously embedded Wi-Fi passwords, printer LAN access codes and EDOL agent
credentials should be rotated if shared or exposed. Private host/SSID/serial
information should also be reviewed. No live credential was rotated here.

## Limits

No Sonar analyzer was exposed by this session, so no Sonar result is claimed.
No firmware upload, serial monitor, live MQTT, printer command, backend call,
NVS erase or factory reset was performed. Hardware reconnects, real camera
timeouts, throughput, TLS security, provisioning UI and Stage 9.3b acceptance
remain unverified. Host fakes do not establish compatibility with live systems.
See the README for existing security limitations.
