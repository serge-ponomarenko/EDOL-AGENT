# EDOL Agent

ESP32 LOLIN D32 firmware for one Bambu printer. It forwards MQTT reports and
commands, streams model files from FTPS to EDOL, and relays camera snapshots.
Transfers use fixed 1024-byte buffers without saving files to flash.

## Build

Install PlatformIO Core, then run:

```sh
pio run -e lolin_d32
```

This builds firmware only. Uploading or using a serial monitor requires separate
device authorization. The verified dependency versions are recorded in
[verification.md](docs/verification.md).

## Configuration and setup

Publishable defaults in `src/config/Config.h` contain no deployment credentials
or private hosts. Configure the device through its existing setup portal:

1. An unprovisioned device starts an open `EDOL-Agent-XXXX` Wi-Fi access point.
2. Join it in a controlled environment and open its captive portal, or browse to
   the access point gateway address.
3. Enter Wi-Fi credentials, EDOL agent ID/password, printer host/serial/access
   code, MQTT host and Core host. Hosts have no scheme prefix.
4. Review advanced ports, upload path and FTPS directory, then save and reboot.

See [configuration.example.md](docs/configuration.example.md) for fictional
values. Never commit actual configuration. `Config_dev_secret.h`,
`Config.local.h` and `.env*` are ignored local notes; they are **not loaded** by
the firmware. No private header is needed for a normal build.

Existing Preferences values override defaults, including stored empty values.
The `edol_config` namespace and all keys are preserved. Provisioning still checks
for the four existing Wi-Fi/EDOL keys. Defaults alone do not mark a device as
provisioned. This cleanup does not erase or migrate NVS.

## Secure pairing development profile

Stage 9 source adds `lolin_d32_secure` for HTTPS enrollment/uploads and verified
TLS MQTT through the coordinated single-Mosquitto broker contract. Source and
disposable checks do not establish physical Agent acceptance.
Keep existing working deployments on their accepted firmware.

Home remains the default `lolin_d32` environment and needs no CA configuration.
For an explicitly selected secure build, set `EDOL_AGENT_ROOT_CA_FILE` to a
local public CA bundle, then run `pio run -e lolin_d32_secure`. The build hook
validates the bundle and generates an ignored build-local header. Do not commit
trust material or pass PEM through compiler flags. Missing/invalid CA input
fails the secure build. Building does not upload or contact EDOL.

The secure setup form accepts an owner-issued code from the EDOL printer Agent
page instead of manual agent ID/password. Enter the configured HTTPS DNS/IPv4
host and port (secure default 443; previously stored ports stay unchanged).
Save & Reboot saves the settings and starts the automatic sequence in the
existing portal: Wi-Fi → NTP → Core TLS → enrollment → MQTT credentials →
MQTT TLS connection/subscriptions → reboot into normal operation. Progress and
safe failure reasons appear on the same page; stay connected to the setup AP.
There is no preliminary unpaired configuration or mandatory serial command.
Network calls are bounded; the portal may pause responding for tens of seconds
while a step reconnects Wi-Fi or waits for NTP/TLS/HTTP. Reconnect to the same AP if its Wi-Fi channel
changes when the station joins the network, then reopen the existing page.

Pre-send Wi-Fi/NTP/TLS failures retry every 15 seconds after completion. Use
Retry saved settings without typing unchanged fields again. A new code is
held only in RAM for at most five minutes from entry (the server's expiry may
be earlier), and never prefilled or returned by status. Blank resubmission
does not extend that window. Success, code rejection or an uncertain enrollment
response clears it. An uncertain response/storage failure requires checking
Agent status in EDOL and owner replacement if already enrolled; do not replay
one-time requests. A power loss before enrollment loses only the pending code,
not saved settings: enter a fresh code in the same form.

With an existing identity, leave the code blank to change network/endpoints
without replacing identity. An explicitly entered new owner code requests new
enrollment. MQTT issuance failures after the request may have been sent require
owner replacement; a persistent attempt marker is never reset automatically.
After restart, that marker without a usable MQTT secret reopens the portal with
an explicit replacement message. Temporary TLS failure before sending the
request does not consume the marker. Existing identities and all NVS keys are
retained; the issued identity is stored separately in `edol_agent_sec`, using
a final commit marker. Existing Home keys in `edol_config` are unchanged.
Secure uploads use fixed v1 routes and the issued credential, never the Home
agent ID. Redirects and plaintext fallback are disabled for enrollment.

Secure MQTT uses CA-verified TLS to the configured broker (default port 8883).
On its first connection it requests a distinct one-time MQTT secret over the
authenticated HTTPS route `/api/agent/v1/mqtt/credentials`. It never uses the
HTTPS secret as the MQTT password. Username/client ID and all secure topics use
the issued Agent UUID. The MQTT secret and attempt/readiness markers live in
`edol_agent_sec`; Home keys remain unchanged. Lost provisioning responses require
owner replacement, not retries to retrieve a secret. Normal reconnect reuses the
retained MQTT secret and restores subscriptions without clearing identity.

The existing open setup AP and connection procedure are deliberately unchanged
by operator decision. This retains a local-trust setup boundary, including
legacy credential prefilling. The new pairing credential is never prefilled
or logged. No AP password or device label is required by this increment.

The existing factory-reset route and five consecutive unsuccessful boots can
erase configuration. A boot is marked successful after 15 seconds. Avoid repeated
short power cycles when preserving settings; no reset was performed for this work.

## Opt-in disposable hardware smoke console

`lolin_d32_secure_smoke` extends the secure environment and uses the same public
CA prerequisite. It adds a bounded USB serial console at 115200 baud with LF
line endings. Home and ordinary secure builds contain no smoke console.

- `STATUS` reports Wi-Fi/setup flags only.
- `TIME` attempts bounded SNTP synchronization and reports UTC epoch, build epoch,
  observed synchronization/readiness and the SHA256 of the embedded public CA PEM.
- `TLS` verifies a TLS-only connection to the configured Core host/port. It sends
  no HTTP request or credential and cannot consume a pairing code. Failure reports
  numeric TLS/X.509 flags when available, including time/name/trust categories.
- `SNAPSHOT` calls the production camera-to-HTTPS upload service once.
- `MODEL example.3mf` reads an existing printer file through the production FTPS
  service and streams it to Core. Only plain ASCII basenames are accepted.
- `SETUP` disconnects MQTT and opens the existing setup portal without erasing
  settings or identity. Use it for explicit owner-approved re-pairing. A backend
  denial leaves the stored identity intact; an interrupted write after a valid
  enrollment response invalidates readiness and requires owner rotation.

Uploads read real printer data and write only to the chosen Core destination.
They do not start a print or send motion commands. This console alone cannot
prove MQTT end-to-end behavior.
Do not deploy the lab build as normal firmware. Restore the ordinary approved
build after the test without erase, and verify recovery separately. Follow the
backend Stage 9 disposable smoke workbook and confirm the Agent's USB port;
an AMS terminal port is not evidence of the Agent port. Numeric TLS errors and
HTTP status logs contain no credential material.

## Checks and publication

```sh
pio run -e lolin_d32
python scripts/run_host_tests.py
python -m unittest discover -s test -p test_secure_trust.py
git diff --check
```

On Windows, `py -3` can replace `python`. Building Home first resolves the real
ArduinoJson parser used by the enrollment host tests. Ten C++ executables
exercise production Home upload/AgentService, credential storage, bounded
enrollment and secure upload routing with fake sockets; they do not establish
real TLS certificate behavior. The transport fake additionally checks clock
readiness and flag retention before Arduino clears failed-handshake state.
Secure transports require an observed SNTP callback during this boot and reject
clock jumps from its monotonic anchor; a plausible build-time lower bound alone
does not establish readiness. SNTP is not authenticated time. Certificate chain,
validity and endpoint checks remain the TLS verifier's responsibility.
The three Python cases check the CA build hook.
They do not contact devices or services. Review every intended file and
every outgoing commit for secrets before pushing. `.pio`, IDE state, local
configuration, keys, logs, dumps and firmware binaries must stay local.
If old history contains credentials, publish an explicitly reviewed file list
in a separate directory with fresh Git history. Never copy the original `.git`.

## Known limits and coordination

- Home EDOL MQTT and uploads use plaintext transports. The development secure
  profile adds CA-verified HTTPS/MQTT and uses a separate versioned MQTT namespace.
  Printer MQTT, FTPS and camera still use certificate verification disabled
  (`setInsecure()`); this is a distinct local-printer limitation.
- The setup AP is open, its HTTP portal is unauthenticated, and it prepopulates
  stored credentials. Preferences storage is not encrypted by this firmware.
- Home command forwarding trusts its connected broker. Secure broker credentials,
  ACLs and active-session revocation require the coordinated Core/Mosquitto release.
- MQTT callbacks assume one instance per client type. Transfers are synchronous
  and can delay MQTT processing. Home topics/settings remain unchanged; secure
  MQTT uses `edol/agent/v1/{issuedAgentId}/...`. Firmware sends QoS 0 reports and
  uses clean sessions; commands are not durable or retained.
- Camera support depends on the printer's existing camera protocol. Snapshot
  health now requires both successful streaming and the expected HTTP status.
- No hardware, reconnect, throughput or live backend acceptance was performed.
  Stage 9.3b remains owned by the backend session; HTTPS source/build checks
  do not establish a complete agent rollout or physical acceptance.

The [original POC](docs/edol-agent-poc.md) is historical design material, not a
claim that its proposed secure transport or future features are implemented.
