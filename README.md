# nova – Wattstunde NOVA BMS für ESPHome (experimentell)

ESPHome-Komponente `wattstunde_nova`, die Wattstunde-NOVA-Batterien (Core/Base)
direkt im ESP32 per Bluetooth LE ausliest – ohne BMS_BLE in Home Assistant.

Das Protokoll ist aus [aiobmsble](https://github.com/patman15/aiobmsble)
(`ws_nova_bms.py`, Apache-2.0) portiert.

**Status:** experimentell, noch nicht an echten Batterien getestet.

## Einbinden

```yaml
external_components:
  - source:
      type: git
      url: https://TOKEN@github.com/wupperbeacher-sketch/nova
    components: [ wattstunde_nova ]

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: nova_ble

wattstunde_nova:
  - ble_client_id: nova_ble
    update_interval: 30s
    voltage:
      name: "Nova Spannung"
    soc:
      name: "Nova Ladezustand"
```

Voraussetzungen: ESP32 mit ESP-IDF, ESPHome ≥ 2026.9.
