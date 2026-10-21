# EDOL Agent POC

> Historical proposal. The current Arduino/PlatformIO implementation includes
> camera relay and does not implement the secure cloud transport proposed here.
> See the root README for actual behavior and security limitations.

## Executive Summary

Yes, an ESP32 LOLIN D32 can plausibly act as a transparent bridge between a Bambu Lab printer and EDOL Cloud, but only if we keep the agent extremely thin:

- MQTT and command forwarding are a good fit.
- Camera pass-through is a future add-on, not part of the first POC.
- FTPS streaming is the hardest part and is the real feasibility gate.

The best POC strategy is:

- keep all business logic in EDOL Cloud,
- let the agent forward bytes and commands only,
- use outbound-only cloud connectivity,
- stream printer files in chunks with no local persistence.

## Recommendation

### Hardware

- Primary target: `ESP32 LOLIN D32`.
- Secondary validation target: `Raspberry Pi Zero 2W` only if the ESP32 memory profile proves too tight.

### Runtime

- Recommended POC runtime: `ESP-IDF`.
- Why: best control over sockets, buffering, reconnect behavior, and memory usage.
- Not recommended for the first POC: `TinyGo` and `Rust Embedded`, because the team will move faster with ESP-IDF and its networking stack.

### Cloud Transport

- Recommended primary transport: persistent `MQTT`.
- Secondary option: `WebSocket` if your cloud team prefers an API gateway style integration.
- Not recommended for the first POC: `gRPC streaming`, because it adds complexity without solving the main bottleneck.

## Proposed Architecture

```text
                    EDOL Cloud
          +------------------------------+
          |  MQTT / WS command broker    |
          |  Upload endpoint for files    |
          |  3MF parsing and analytics    |
          +--------------^---------------+
                         |
          outbound only  | persistent TLS session
                         |
                    EDOL Agent
          +------------------------------+
          | MQTT bridge                  |
          | command relay                |
          | FTPS byte relay              |
          | camera pass-through later    |
          +--------------^---------------+
                         |
         local network / printer LAN
                         |
                    Bambu Printer
```

## Component Breakdown

### 1. Printer State Forwarding

Responsibilities:

- connect to local printer MQTT,
- subscribe to report topics,
- forward payloads to cloud,
- add lightweight envelope metadata such as:
  - printer serial,
  - agent id,
  - timestamp.

Non-goals:

- interpreting printer business state,
- inventory decisions,
- spool allocation,
- AMS decisions,
- reconciliation.

### 2. Command Forwarding

Responsibilities:

- keep one persistent outbound connection to cloud,
- receive command messages from cloud,
- publish the command to printer MQTT request topics,
- forward printer replies back to cloud.

The agent should not validate business rules. Cloud owns validation.

### 3. FTPS Streaming Bridge

This is the critical POC path.

Desired behavior:

- open the printer-side FTPS session,
- read file data in small chunks,
- write each chunk directly to the cloud upload endpoint,
- avoid storing the full file,
- avoid unzip/parsing on-device,
- avoid local flash persistence.

Target characteristics:

- constant memory usage,
- chunk-based transfer,
- no full-file buffering,
- no local file copy.

### 4. Metadata Ownership

Cloud owns:

- 3MF parsing,
- metadata extraction,
- filament estimation,
- thumbnail extraction,
- analytics.

Agent owns:

- transport only.

### 5. Camera Relay

Future-only capability.

Initial expectation:

- pass-through only,
- no transcoding,
- no recording on the agent.

## MQTT Bridge Design

The reference Bambu MQTT protocol exposes:

- local printer MQTT on `device/{DEVICE_ID}/report`,
- request topics on `device/{DEVICE_ID}/request`,
- JSON message bodies,
- TLS on port `8883`.

Bridge design:

- subscribe to printer report topics,
- forward incoming messages to cloud,
- accept cloud command envelopes,
- publish printer request messages,
- forward printer responses back to cloud.

Important design rule:

- do not implement a printer state machine in the agent.

### Example Flow

```text
Printer report topic
    -> Agent
    -> Cloud event stream

Cloud command
    -> Agent
    -> Printer request topic
    -> Printer response
    -> Agent
    -> Cloud event stream
```

## FTPS Streaming Design

The file bridge should use a pull-and-forward pattern:

```text
Printer FTPS read
    -> small buffer in agent
    -> cloud upload write
```

Implementation notes:

- use a fixed-size buffer,
- write each chunk as soon as it is received,
- keep only one chunk in memory at a time where possible,
- prefer streaming upload APIs on the cloud side,
- avoid any ZIP or 3MF interpretation on the ESP32.

Practical constraint:

- multiple concurrent network sockets can stress an ESP32, so the buffer size and reconnect behavior must be tuned carefully.

## Cloud Communication Design

Recommended approach:

- persistent outbound `MQTT` session from agent to cloud,
- all commands flow through that session,
- cloud sends an ack or command id,
- agent publishes execution/result events back.

Why this is the best fit:

- naturally outbound-only,
- reconnect behavior is straightforward,
- low protocol overhead,
- easy to multiplex commands and printer events.

WebSocket is a valid fallback if the team wants HTTP infrastructure, but MQTT is the simpler fit for device telemetry and command fanout.

## Sequence Diagrams

### Printer State Forwarding

```text
Printer -> Agent: MQTT report message
Agent   -> Cloud : event envelope
```

### Command Forwarding

```text
Cloud   -> Agent : command message
Agent   -> Printer: MQTT request
Printer -> Agent  : MQTT report / response
Agent   -> Cloud  : result envelope
```

### FTPS Stream Relay

```text
Printer -> Agent : FTPS chunk
Agent   -> Cloud : upload chunk
```

## ESP32 Feasibility

### Likely feasible

- MQTT bridge,
- command relay,
- simple reconnect logic,
- heartbeat,
- printer offline detection,
- cloud event forwarding.

### Feasible but tight

- FTPS streaming relay,
- simultaneous high-throughput network forwarding,
- consistent reconnect behavior under Wi-Fi loss.

### Risky or defer

- camera relay,
- any on-device file parsing,
- any unzip or 3MF metadata extraction,
- any persistent local buffering of large printer files.

## Risks and Limitations

1. TLS memory pressure can become the main failure point.
2. Wi-Fi interruptions may interrupt long file transfers.
3. The printer-side FTPS flow may not be as stream-friendly as the cloud-side upload flow.
4. If the cloud upload endpoint does not support true chunked ingestion, the relay will lose its main advantage.
5. Camera passthrough can easily become a separate project.

## Minimal Implementation Plan

### Phase 1

- bring up ESP32 networking,
- connect to cloud with persistent MQTT,
- connect to printer local MQTT,
- forward report messages end-to-end.

### Phase 2

- add cloud-to-printer command forwarding,
- add reconnect and heartbeat behavior,
- add offline detection.

### Phase 3

- implement FTPS chunk relay,
- confirm no full-file buffering,
- verify memory stays bounded.

### Phase 4

- validate camera relay as a separate investigation.

## Final Answer

An ESP32 LOLIN D32 can probably serve as a transparent printer bridge for EDOL Cloud if the agent remains a transport-only device and the FTPS path truly supports streaming chunk relay.

The architecture is promising for MQTT, command forwarding, and a thin FTPS bridge.

The main unresolved technical question is FTPS streaming of large printer files under tight memory limits. That is the POC worth proving first.

## References

- Bambu local MQTT reference: https://github.com/Doridian/OpenBambuAPI/blob/main/mqtt.md
- ESP-IDF HTTP client streaming APIs: https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32s3/api-reference/protocols/esp_http_client.html
- ESP-IDF HTTPS server docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/esp_https_server.html
