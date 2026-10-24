# Verification and publication record

## Exact-vendor IPv4 SAN verification fix — 2026-10-23

Operator evidence on `7f00b48`: Core endpoint 192.168.0.200:18443, certificate
iPAddress SAN matches, OpenSSL IP verification succeeds, synchronized NTP and
correct CA, mbedTLS flags 0x00000004 only (CN_MISMATCH). This supersedes the
unknown-cause checkpoint below; no new hardware action was performed here.

Installed SDK metadata: Espressif32 6.13.0, Arduino-ESP32 2.0.17 / framework
3.20017.241212, ESP-IDF 4.4.7 at `38eeba213a`. The IDF tree's mbedTLS gitlink is
`2b8e772fc1cb0732cda3bae7d1e9d6f4cfaf63d9`, whose version is **2.28.7**.
The exact [vendor x509_crt.c](https://github.com/espressif/mbedtls/blob/2b8e772fc1cb0732cda3bae7d1e9d6f4cfaf63d9/library/x509_crt.c)
retains all raw SAN entries in x509_get_subject_alt_name, but x509_crt_check_san
accepts only dNSName. A SAN extension prevents CN fallback. Thus a correct
binary IP SAN still produces CN_MISMATCH when set_hostname receives the IP.

SecureIpClient installs mbedtls_ssl_set_verify per client/session before the
unchanged Arduino TLS setup/handshake. In the exact vendor ssl_tls.c,
ssl_setup retains that callback. The callback checks only the leaf identity:
canonical IPv4 must equal an exact four-byte context-specific primitive tag 7
SAN. A verified match replaces only the name flag, never other flags; failure
adds CN_MISMATCH even if native CN/textual DNS matching accepted the IP string.
Other chain depths are untouched. DNS uses native verification. The original
hostname and VERIFY_REQUIRED remain; no null hostname, setInsecure, global
linker hook, SDK installation patch or deployment trust-material change.
The secure platform is pinned, with an mbedTLS version compile guard for review
on SDK upgrades. Normal and smoke secure profiles share this implementation;
Home and local-printer transports do not use the new verifier.

### Targeted results

- **16 real-vendor cases PASS**: native IP-SAN failure reproduced; matching IP
  succeeds; foreign IP, wrong IP SAN, matching CN without IP SAN, textual DNS IP,
  malformed IP SAN rejected; mixed SAN accepted; DNS positive/negative retained;
  wrong CA, expired, future and corrupted signature rejected; pre-setup callback,
  hostname and VERIFY_REQUIRED preserved; parser/non-leaf isolation checked.
- **10 existing native executables PASS**, now including secure-client callback
  installation, independent clients, DNS reuse and ambiguous-IP rejection.
  The full runner still exits 1 only for Windows WinError 32/5 final temporary
  directory cleanup after tests pass. Do not label that runner exit a clean PASS.
- **3 CA-hook Python tests PASS**; Home, secure and secure_smoke ESP32 builds
  PASS. Secure ELF contains the IP verifier, four connect overrides and
  mbedtls_ssl_set_verify; Home ELF contains no SecureTransport::Client/IP verifier.
- Read-only Sonar: both changed Python runners have no findings. C++/CMake
  analysis is unsupported. git diff --check passed.
- **NOT RUN:** ESP32 TLS handshake/enrollment/MQTT/telemetry acceptance for the
  new firmware. Vendor tests execute real certificate parsing, signature/chain
  and identity verification on the host, plus context setup; they do not claim
  an ESP32 handshake or a live Stage 9 endpoint connection.

The native fixture uses the exact downloaded vendor source, verified by source
hashes. Archive SHA256:
`5bcafd9cc465b6009d9f9225c79af2bc62368b469f65ee7b52167540e157fffd`.
All generated test CA/private keys/certificates stay under ignored .pio; they
are unrelated to Stage 9. Never publish them or substitute them on the device.
The native build uses desktop crypto/configuration from the same vendor source;
it does not emulate ESP32 hardware acceleration or the whole embedded SDK config.

Maintainer reproduction (not an operator prerequisite; use the pinned vendor
archive extracted under ignored .pio and an installed OpenSSL):

```text
py scripts/run_ip_san_tests.py --mbedtls-source .pio/ip-san-vendor/mbedtls-2b8e772fc1cb0732cda3bae7d1e9d6f4cfaf63d9 --openssl "C:/Program Files/Git/usr/bin/openssl.exe"
py scripts/run_host_tests.py
py test/test_secure_trust.py
```

Pinned source archive:
https://codeload.github.com/espressif/mbedtls/zip/2b8e772fc1cb0732cda3bae7d1e9d6f4cfaf63d9

Before rebuilding, the existing local smoke binary was preserved as
`stage9-restoration/pre-ip-san-fix-7f00b48.bin`, SHA256
`777de1ab437b59732bbb06f2e6349b83e6baf2808b046776c748362a90687e0a`.
It is a saved local artifact, not a verified device dump. No flash, serial,
deployment, reset, NVS change or Stage 9 network access occurred.

## Single-form secure setup — 2026-10-23

Source now keeps one form open while automatically performing Wi-Fi, NTP,
verified Core TLS, one-time enrollment, distinct MQTT provisioning and MQTT TLS
connection/subscriptions; success reboots into normal operation. The code is
RAM-only, never prefilled/logged, bounded to five minutes locally, and cleared
on success/rejection/uncertain enrollment. A blank resubmission preserves its
original deadline. Settings stay in existing NVS keys. Existing identities can
be retained with a blank code; uncertainty never silently resets MQTT markers.
Restart with an attempted issuance but no usable secret reopens the same portal
with owner-replacement instructions. Home provisioning semantics are unchanged.

Production enrollment/MQTT clients now distinguish pre-send TLS failure from
an uncertain POST response. Verified TLS is established before the MQTT attempt
marker is committed; lost/invalid responses after that point remain fail closed.

Targeted host verification: all **10 native executables passed**, including
sequence, backoff from blocking-call completion, code expiry/wraparound, blank
resubmission, retained identity and no replay after restart. Enrollment tests
cover pre-send TLS retry with no POST versus uncertain response/code rejection;
MQTT tests prove TLS failure does not consume the one-time marker. The **3 CA
hook Python tests passed**. The full native runner reported Windows WinError
32/5 only during final temporary-directory cleanup after all executables passed;
this runner exit is not described as a clean PASS. Existing locked temp files
were preserved; no broad cleanup was attempted.

Home, secure and secure_smoke ESP32 builds passed. Read-only Sonar checked the
changed Python runner and extracted portal HTML/JavaScript; actionable findings
were fixed (accessible labels/language and equivalent polling initialization).
Final supported snippets had no findings; C++ analysis is unavailable. The CA
hook was unchanged, so previous missing-CA fail-closed evidence is retained.

No device, serial, flash, deployment, reset, NVS or existing Stage 9 environment
was accessed or changed. The physical TLS -9984 cause remains unknown without
actual TIME/TLS output; none was supplied during this source-only task. Setup
TLS failures now invoke the same lab diagnostics automatically. No physical
setup/enrollment/MQTT PASS is claimed.

Local build artifacts only: the pre-first-build binary for this increment was
not separately backed up. Existing restoration artifacts were retained, and
the intermediate smoke artifact was saved as
`stage9-restoration/pre-setup-final-build-20261023.bin`, SHA256
`2693bbe64a3f072a8858d6ba6752b443a3610695bda4f64166aff4ffc7594b8d`.
This is not a device dump. A previous-source rollback remains available from
`054782c` with the existing public CA; no installed device image was modified.

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

## October 24 FTPS regression increment

The offline FTPS test compiles the production `FtpsClient.cpp` with a scripted
control/data socket fake. It covers normal/bare PASV prefixes and malformed
tuples, multiline login replies, Unicode RETR, buffered EOF, PASV/RETR failures,
idle timeout and short HTTPS stream writes. These fakes do not verify a real
printer FTPS server or EDOL TLS; the existing secure transport code is unchanged.

```powershell
py scripts/run_host_tests.py
# Standalone reproduction if Windows temporary-directory cleanup stops the runner:
py -c "from pathlib import Path; from scripts.run_host_tests import run_test; run_test(Path.cwd(),'ftps','src/ftps/FtpsClient.cpp')"
py -m unittest discover -s test -p 'test_secure_trust.py'
# Secure builds require the existing public CA via EDOL_AGENT_ROOT_CA_FILE.
pio run -e lolin_d32 -e lolin_d32_secure -e lolin_d32_secure_smoke
```

Actual evidence: eleven distinct native executables PASS (ten in the full
runner, FTPS separately), three Python tests PASS, all three ESP32 builds
SUCCESS. The full runner exited with Windows WinError 32 while cleaning its
temporary directory after provisioning; it did not reach its final FTPS entry.
Do not describe that runner exit as PASS. No tests were skipped and counted as
executed. No pre-build secure_smoke binary existed, so no new restoration backup
is claimed; existing ignored restoration files remain intact.

New diagnostics contain only failure stage, numeric FTP reply code and byte
counts. A DOWNLOAD_FAILED report alone does not establish TLS or authorization
failure. Physical model/camera and full print lifecycle retests remain pending;
the October 24 operator's enrollment/MQTT/telemetry evidence remains accepted.
No upload, reset, NVS/configuration change, deployment or Stage 9 request was
performed in this source increment.

## Snapshot TLS lifecycle and revoke follow-up

Operator evidence at `5aba847` confirms flash, new pairing, ACTIVE/MQTT READY,
automatic Hub status and real camera JPEG capture. Snapshot HTTPS still failed
NOT_TRUSTED (flag 0x8). This is distinct from the accepted IPv4 SAN correction.

Source inspection of the installed Arduino-ESP32 2.0.17 found
`stop_ssl_socket` zeroes the descriptor after `WiFiClientSecure::stop` set it
to -1. Upload abort/preparation/reconnect can therefore configure/close fd 0;
fresh enrollment preparation starts at -1. The subclass now restores -1 after
every stop in both secure builds. Its ten-second timeout now uses the SDK's
seconds units. CA, VERIFY_REQUIRED and the IPv4 SAN decision are preserved.
This establishes a lifecycle defect and explains errno=9; it does **not** prove
why the observed certificate validation returned NOT_TRUSTED.

The lab verifier prints certificate-depth flags and the peer leaf DER SHA-256
for comparison across TLS probe/enrollment/upload. No certificate body, code,
credential, response body or private key is printed. Normal secure builds omit
these diagnostics. Trust/time/signature failures still reject the handshake.

Actual automated results for this increment:

- 12 distinct native executables PASS, run through the host helper. These include
  the new `tls_lifecycle` executable linking production enrollment, upload and
  secure transport together: repeated snapshot/abort/finish, certificate failure
  rejection before HTTP transmission, retry and preserved identity. The separate
  secure transport test models the actual SDK cleanup behavior and runs 25
  cleanup/reconnect cycles, exact IP identity and preserved trust-flag controls.
- 16 real-vendor mbedTLS certificate cases PASS using the existing pinned source
  and unrelated disposable fixtures. Wrong CA/signature/time/IP remain rejected.
- Three Python CA-hook tests PASS.
- Home, secure and secure_smoke ESP32 builds SUCCESS with the existing public CA.
  Initial cache/lock permission failures were resolved by authorized build access.
- Python runner Sonar analysis clean after extracting a repeated source literal.
  Available snippet analysis does not support C++. No suppression was added.

Targeted reproduction:

```powershell
py -c "from pathlib import Path; from scripts.run_host_tests import run_test; run_test(Path.cwd(),'secure_transport','src/security/SecureTransport.cpp',secure=True,smoke=True); run_test(Path.cwd(),'tls_lifecycle','src/http/HttpUploadClient.cpp',secure=True,smoke=True)"
```

These fake-socket checks and real-vendor verifier checks are separate evidence;
neither is a real snapshot-upload PASS. No new firmware was flashed. Backend
`docs/deployment/stage9-hardware-remediation.md` contains the operator-approved
probe/snapshot/revoke/re-pair sequence and remaining physical acceptance.
The existing protocol has no authenticated revocation confirmation; MQTT/TLS
failure does not automatically enter Setup Mode or clear credentials.

Before builds, the prior smoke binary was preserved as ignored
`stage9-restoration/pre-camera-tls-fix-secure-smoke-firmware.bin`, SHA-256
`5b9c36167864237dd7c869901088d56380cf0161ec412004c569224a2c665cc9`.
It is a local build artifact, not a dump of device flash. Rollback requires
separate operator approval and preserves current identity/NVS/CA. No Stage 9
runtime, database, authority, deployment, hardware or cleanup action occurred.

## TLS memory-pressure regression

### Mechanism and production change

The exact audited Espressif mbedTLS source performs issuer signature verification
in `x509_crt_check_signature`. `x509_crt_find_parent_in` treats a failed signature
check, including allocator errors, as an unsuitable trusted CA candidate.
`x509_crt_verify_chain` then emits NOT_TRUSTED when no parent remains. This can
produce leaf depth 0 only, without expired/future/name flags, despite the correct
CA and same leaf verifying when memory is available. The previous stop/sentinel
fix does not address this distinct mechanism.

At `0252423`, production snapshot orchestration keeps printer MQTT, EDOL MQTT
and camera TLS open before HTTPS verification (four TLS sessions). Model
streaming additionally uses separate FTPS control/data sessions (five TLS
sessions at data setup). The physical model log explicitly reports allocation
failure at that fifth session. The camera source/resource difference and new
real-vendor reproduction support the same memory-pressure explanation; no
on-device allocation trace identifies its exact failing allocation.

Secure operations now pause printer MQTT before bulk work and restore it only
after cleanup. The operation guard and EDOL identity/credentials stay intact;
the existing reconnect loop handles a failed restoration. Home skips this pause.
The common descriptor wrapper covers the remaining LAN clients as well as the
EDOL client. The additional lab heap/host diagnostics change no verifier flags.

### Actual automated results

- **16 exact-vendor verifier cases PASS**, plus a memory-pressure regression:
  `fault_at=1` reproduces NOT_TRUSTED at depth 0 for the correct CA/same leaf;
  normal verification visits depths 1 and 0 with flags zero. Three sequential
  rejection/recovery cycles PASS. The fault allocator is enabled only in the
  desktop test CMake build, not firmware. Vendor source files remain unchanged
  and pinned hashes are still checked. Foreign CA, bad signature, wrong IP and
  expired/future negatives remain rejected.
- **13 distinct native executable configurations PASS**. The new secure Agent
  orchestration run checks pause during transfer, cleanup before reconnect,
  failure exits, failed restore/main-loop recovery and already-offline behavior.
  Home asserts no pause/reconnect calls. Existing production upload/enrollment,
  repeated cleanup, FTPS, provisioning and credential controls also pass.
  The full runner stops on Windows temporary-directory cleanup after twelve
  successful executables; FTPS passed separately. The runner exit is not PASS.
- **Three Python CA-hook tests PASS**.
- **Home, secure and secure_smoke builds SUCCESS** with the existing public CA.
  The final smoke build includes host/port and heap diagnostics. A local
  PlatformIO lock permission issue was resolved by authorized build access.
- Python host runner Sonar analysis: no findings. The available analyzer does
  not support C++ or CMake. Both repositories pass `git diff --check`.

Reproduction uses the existing commands above:

```powershell
py scripts/run_ip_san_tests.py --mbedtls-source .pio/ip-san-vendor/mbedtls-2b8e772fc1cb0732cda3bae7d1e9d6f4cfaf63d9 --openssl "C:/Program Files/Git/usr/bin/openssl.exe"
py -c "from pathlib import Path; from scripts.run_host_tests import run_test; run_test(Path.cwd(),'agent','src/agent/AgentService.cpp'); run_test(Path.cwd(),'agent','src/agent/AgentService.cpp',secure=True)"
```

Fault injection establishes a concrete failure mechanism in real vendor code,
not a mock returning a predetermined trust flag. Host orchestration tests do
not establish the device's heap margin or a real JPEG upload. Physical camera
upload/display, Agent model transfer, revoke/re-pair and AMS remain pending.
No device, Stage 9 endpoint, deployment, authority or database was touched.

Before builds, the prior smoke artifact was preserved as ignored
`stage9-restoration/pre-pressure-verification-secure-smoke-firmware.bin`
(1,032,928 bytes), SHA-256
`03f262360cebc75f657a15029d74a3ceda65f32accbdbe76812a727b788aa3fc`.
This is a build artifact, not a flash dump. Existing restoration files remain
untouched. Rollback requires separate approval and never resets NVS/identity.

## Continuous MQTT / completion regression (source increment after 8db73079)

This section supersedes the historical MQTT-pause assertions above. No new
source was flashed and no physical acceptance is claimed.

Production ownership is now explicit: only the main loop touches Bambu/EDOL
MQTT; one low-priority worker owns camera/FTPS/HTTP. The production-backed
bulk-task test uses both real MQTT wrappers and the real event publisher with
offline PubSubClient/socket fixtures. While bulk I/O is blocked, it runs 1000
loops on each connection, forwards 1000 telemetry messages and 1000 commands,
checks both connections remain established and forbids worker-thread MQTT
access and recursive loop processing. This runs in Home and secure modes.
It also checks event retention across disconnect, recovery, three-attempt
publish bounds, one-second retry spacing and 60-second expiry. These establish
software scheduling/ownership, not actual device keepalive or heap capacity.

FTPS tests compile the production implementation, not a predetermined transfer
result. They include the Unicode filename and a synthetic 37,722-byte payload:
exact SIZE bytes are read but only 37,721 reach the HTTP-like sink until final
226 is checked and the held tail committed. Final 426 never commits the tail.
Negative TLS EOF after the payload, buffered EOF, invalid SIZE/overflow,
truncation/excess, preliminary/final reply failures, read/write/timeout failures
and fragmented-heap admission are covered. No real model contents are used.
The production camera test covers frame/count bounds, read/stream failure and
timeout. Secure HTTP tests cover prepared TLS reuse, lost prepared connection
without fallback reconnect, identity/trust failure and low-memory rejection.

### Actual targeted results

| Check | Result |
| --- | --- |
| Native configurations below | 12 distinct configurations: assertions PASS |
| transfer_mailbox runner exit | Windows WinError 32/5 during temp cleanup after assertions; not a clean runner PASS |
| bulk_task Home/secure, upload, secure_upload, agent Home/secure, FTPS, camera, MQTT Home/secure, TLS lifecycle secure+smoke | PASS |
| Exact audited vendor verifier | 16 real certificate controls PASS, allocation-fault regression PASS, three rejection/recovery cycles PASS |
| Home / secure / secure_smoke PlatformIO | Three SUCCESS; static RAM/flash reporting is not runtime heap-margin proof |
| Selected backend checks | 38 PASS, zero failures/errors/skips; no Docker or DB checks selected |
| Dashboard JavaScript | 5 PASS, zero failures/skips |
| Device, real JPEG/model, actual command execution | NOT RUN |

The selected suite does not claim a full native runner, full Maven reactor or
hardware acceptance. Initial MSVC strcpy/link fixtures and one Mockito generic/
MockMvc invocation expectation were fixed; the final selected checks passed.
The known Windows temp cleanup issue remains distinct from assertions/compile.
The native runner now includes the new tests for future full-suite execution.

From this worktree, native checks were invoked with the existing run_test helper
and absolute resolved root; for example:

```powershell
py -c "from pathlib import Path; from scripts.run_host_tests import run_test; run_test(Path.cwd().resolve(),'bulk_task','src/agent/BulkTransferTask.cpp')"
py -c "from pathlib import Path; from scripts.run_host_tests import run_test; run_test(Path.cwd().resolve(),'bulk_task','src/agent/BulkTransferTask.cpp',secure=True)"
py -c "from pathlib import Path; from scripts.run_host_tests import run_test; run_test(Path.cwd().resolve(),'ftps','src/ftps/FtpsClient.cpp')"
py scripts/run_ip_san_tests.py --mbedtls-source .pio/ip-san-vendor/mbedtls-2b8e772fc1cb0732cda3bae7d1e9d6f4cfaf63d9 --openssl "C:/Program Files/Git/usr/bin/openssl.exe"
pio run -e lolin_d32
pio run -e lolin_d32_secure
pio run -e lolin_d32_secure_smoke
```

The same helper selected: transfer_mailbox/None, upload/HttpUploadClient.cpp,
secure_upload/HttpUploadClient.cpp secure, agent/AgentService.cpp Home+secure,
camera/CameraClient.cpp, mqtt/EdolMqttClient.cpp Home+secure and
tls_lifecycle/HttpUploadClient.cpp secure+smoke. Paths above are under src/
with their normal component directories. Secure builds reused the existing
worktree-local public CA through EDOL_AGENT_ROOT_CA_FILE; no CA content was
printed, committed or changed. Local fixture/cache access required bounded
sandbox escalation where the native tools reported permissions.

The read-only Sonar analyzer supports the modified Python runner (no findings)
but not C++. Backend Java/HTML/JS results, memory floor/design comparison,
operator retest and rollback are recorded in the backend Stage 9 hardware
remediation runbook. EDOL trust/signature/time/IP checks are unchanged.
Two actual snapshots with continuous MQTT and five-session model capacity are
**unproven**. Reject the workload cleanly if it cannot fit; never disconnect
MQTT or bypass TLS as a fallback. Full-image RAM/flash staging and SDK buffer
changes were not introduced. Existing restoration artifacts are untouched.
