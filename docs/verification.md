# Verification and publication record

## Stage 9 enrollment TLS diagnostics — 2026-10-23

The operator reported secure_smoke `65b1fd9` FLASH/boot/portal PASS, followed
by enrollment TLS -9984. This aggregate error does not establish a clock,
trust-chain or endpoint-name cause. Enrollment, MQTT and printer telemetry via
Agent remain unaccepted. No hardware action was performed in this increment.

Secure clock readiness now requires an observed SNTP callback this boot and
continuity against a monotonic anchor. The old plausible-epoch lower bound
alone was insufficient. CA verification, required hostname checks and no
plaintext fallback remain. Home behavior and NVS keys are unchanged.

Lab-only TIME/TLS commands report UTC/NTP, embedded public CA PEM SHA256 and
available X.509 verification flags. TLS connects only to configured Core and
sends no HTTP request or credential. A small WiFiClientSecure subclass preserves
flags before Arduino 2.0.17 frees its failed-handshake state; unavailable flags
remain unavailable. Host fakes exercise this lifetime, not actual mbedTLS trust.

Targeted verification: **9 native executables PASS**, **3 Python trust-hook
tests PASS**; Home, secure and secure_smoke ESP32 builds PASS. Both secure
environments without a CA failed as required. The existing public CA SHA256
matched `6494dae1329c5443d742f53e0b38b8b59184a53d268bac397d801c18f04a9ec6`.
Read-only Sonar checked the three changed Python files without findings; the
available analyzer does not support C++. No live TLS, enrollment, MQTT, flash,
serial, printer operation or NVS modification was performed.

The previous build artifact was preserved under ignored `stage9-restoration/`;
it is not a device flash dump. Resume and rollback instructions are maintained
in the coordinated backend `docs/deployment/stage9-hardware-remediation.md`.
The older records below describe their own verification scope and limitations.

## Stage 9.3b secure MQTT continuation — 2026-10-22

The local branch now implements server-authenticated MQTT TLS, separate
one-time MQTT provisioning, persisted attempt/readiness markers, issued-ID
topics/command filters, and reconnect subscription recovery. The coordinated
backend uses one Mosquitto broker for service and Agent traffic.

- Eight offline native executables PASS, including Home/secure MQTT identity,
  topic/filter selection, reconnect/reconstruction, one-time HTTPS MQTT
  provisioning, response bounds and interrupted storage. Sockets remain fake;
  these do not prove ESP32 TLS handshakes or power-cycle behavior.
- Three Python trust-hook tests PASS.
- Home, secure and secure_smoke ESP32 builds PASS. Both secure builds without
  a CA fail as required. The temporary public-root CA fixture was removed;
  no private key was read and no upload/serial operation occurred.

Physical telemetry, commands, uploads, TLS failures, reconnect/power cycle,
revoke/replace and restoration remain NOT RUN in the backend combined workbook.
No commit, push, published-main change or working broker/database cutover is
implied by this verification. Historical HTTPS-only records below remain dated.

## Stage 9.3b.2 local HTTPS increment

The sanitized baseline remains `14da63f360d4a288c7207340dc60cd9e78ce7907`.
New source changes are isolated in `stage9-agent-pairing`, not pushed or flashed.
The original records below describe the publication baseline, not these changes.

- Five offline native C++ executables pass with compiler warnings treated as
  errors. New cases cover UUID/code/host validation, separate credential-store
  commit markers, interrupted writes, unchanged Home namespace, response caps,
  secure upload paths, revoked-credential HTTP errors and fail-closed readiness.
- Enrollment tests compile the production client with the real ArduinoJson
  parser and fake HTTP/TLS. They cover non-200 responses, missing time/trust,
  malformed/oversized JSON, invalid returned identities, storage failure,
  one-request/no-retry behavior and connection cleanup. They do not prove TLS.
- Three Python trust-hook cases pass: Home without CA, secure fail-closed
  prerequisites, certificate-only/size validation and PEM-newline preservation.
- Real Home build succeeds: RAM 14.5%, flash 75.6%, program size 990353 bytes.
- Real secure build with a temporary public CA fixture succeeds: RAM 14.9%,
  flash 78.1%, program size 1023965 bytes. The separate opt-in
  `lolin_d32_secure_smoke` build passes: RAM 14.9%, flash 78.2%, program size
  1025105 bytes. Both secure environments without the CA fail as required.
  Temporary fixture and process-local environment are cleaned up.
- PlatformIO config confirms Home stays the default and secure mode is explicit.
- The existing open setup AP remains unchanged by operator instruction. No
  password, label/card, or local-portal authentication was introduced.

No firmware upload, NVS change, serial monitor, live backend/broker/printer,
actual TLS handshake, power-cycle or operational enrollment was exercised.
Secure MQTT remains disabled until the coordinated broker contract is ready.
The lab build adds USB-triggered production snapshot/model uploads and a
settings-preserving setup-portal command. Normal Home/secure builds contain
no console. The backend disposable smoke workbook is prepared for a separately
supervised real-device session; no console command was executed on hardware.
Numeric HTTP/TLS diagnostics contain no credentials. Final Python source changes
passed read-only Sonar snippet analysis; C++ is outside that tool's language list.

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
