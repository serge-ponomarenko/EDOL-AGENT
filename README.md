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

The existing factory-reset route and five consecutive unsuccessful boots can
erase configuration. A boot is marked successful after 15 seconds. Avoid repeated
short power cycles when preserving settings; no reset was performed for this work.

## Checks and publication

```sh
python scripts/run_host_tests.py
git diff --check
```

Host checks exercise production HTTP upload code, AgentService and scope cleanup
with fake clients. They do not contact devices or services. Review every intended file and
every outgoing commit for secrets before pushing. `.pio`, IDE state, local
configuration, keys, logs, dumps and firmware binaries must stay local.
If old history contains credentials, publish an explicitly reviewed file list
in a separate directory with fresh Git history. Never copy the original `.git`.

## Known limits and coordination

- EDOL MQTT and Core uploads use plaintext transports. Printer MQTT, FTPS and
  camera use TLS with certificate verification disabled (`setInsecure()`).
- The setup AP is open, its HTTP portal is unauthenticated, and it prepopulates
  stored credentials. Preferences storage is not encrypted by this firmware.
- Command forwarding trusts the connected broker. Pairing, tenant enforcement,
  replay protection and secure enrollment are not implemented by this cleanup.
- MQTT callbacks assume one instance per client type. Transfers are synchronous
  and can delay MQTT processing. Reconnect timing and existing topics, payloads,
  upload endpoints and credential formats are preserved.
- Camera support depends on the printer's existing camera protocol. Snapshot
  health now requires both successful streaming and the expected HTTP status.
- No hardware, reconnect, throughput or live backend acceptance was performed.
  Stage 9.3b pairing/security remains owned by the backend session and needs
  coordinated firmware work before any contract changes.

The [original POC](docs/edol-agent-poc.md) is historical design material, not a
claim that its proposed secure transport or future features are implemented.
