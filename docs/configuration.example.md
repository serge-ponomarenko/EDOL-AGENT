# Fictional configuration example

Enter actual values only in the device setup portal in a controlled environment.
These values are examples, not working credentials or a complete deployment.

| Setting | Example |
| --- | --- |
| Wi-Fi SSID | `example-wifi` |
| Wi-Fi password | Enter locally; no password is provided here |
| Agent ID | `example-agent` |
| Agent password | Enter the existing backend-issued value locally |
| Printer host | `printer.example.invalid` |
| Printer serial | Enter the printer serial locally |
| Printer access code | Enter the printer LAN access code locally |
| EDOL MQTT host | `mqtt.example.invalid` |
| Core host | `core.example.invalid` |
| EDOL MQTT port | `1783` |
| Printer MQTT port | `8883` |
| Core port | `8080` |
| Model upload path | `/api/agent/upload` |
| FTPS port / user / directory | `990` / `bblp` / `/` (or `/cache` as required) |

No credential format changes are introduced. Use the values expected by the
existing backend and printer. Stored Preferences always take priority over
compile-time defaults.
