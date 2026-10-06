# nova – Wattstunde NOVA BMS für ESPHome

ESPHome-Komponente `wattstunde_nova`, die Wattstunde-NOVA-Batterien direkt im ESP32
per Bluetooth LE ausliest – ohne BMS_BLE oder eine andere Integration in Home Assistant.
Die Werte landen als normale ESPHome-Sensoren in Home Assistant.

Das Protokoll ist aus [aiobmsble](https://github.com/patman15/aiobmsble)
(`ws_nova_bms.py`, Apache-2.0) portiert.

## Status

**Getestet** mit 2× Wattstunde NOVA Base 100 Ah an einem M5Stack ATOM Lite
(ESP32, ESP-IDF, ESPHome 2026.9.1). Spannung, Strom, Ladezustand, Restladung,
Zyklen, Zellspannungen und Temperaturen werden korrekt ausgelesen.

Die NOVA Core nutzt laut aiobmsble dasselbe Protokoll und sollte ebenfalls funktionieren,
ist mit dieser Komponente aber noch nicht getestet. Rückmeldungen sind willkommen.

## Verbinden bei Bedarf (`on_demand`)

Die NOVA-Batterien beenden eine Bluetooth-Verbindung nach etwa 6,5 Sekunden von sich aus.
Deshalb arbeitet die Komponente standardmäßig im Modus „Verbinden bei Bedarf“:

1. Im Abfrageintervall verbinden,
2. Daten abfragen (Antwort nach ca. 2,5 s),
3. Verbindung selbst sauber trennen.

Ein Zyklus dauert so nur etwa 3 Sekunden. Das schont Batterie und Funk und lässt dem
ESP32 Zeit für andere Bluetooth-Aufgaben (z. B. Victron-Geräte oder Bluetooth-Proxy).
Mit `on_demand: false` bleibt die Verbindung dauerhaft bestehen (wird dann aber
regelmäßig von der Batterie getrennt und neu aufgebaut).

## Einbinden

```yaml
external_components:
  - source: github://wupperbeacher-sketch/nova@main
    refresh: 1d
    components: [ wattstunde_nova ]

esp32:
  framework:
    type: esp-idf

esp32_ble_tracker:

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: nova_1_ble

wattstunde_nova:
  - ble_client_id: nova_1_ble
    update_interval: 30s
    on_demand: true
    voltage:
      name: "Nova Spannung"
    current:
      name: "Nova Strom"
    power:
      name: "Nova Leistung"
    soc:
      name: "Nova Ladezustand"
    remaining_capacity:
      name: "Nova Restladung"
    stored_energy:
      name: "Nova Energie"
    cell_voltage_1:
      name: "Nova Zelle 1"
    delta_cell_voltage:
      name: "Nova Zell-Delta"
    temperature_1:
      name: "Nova Temperatur"
    connected:
      name: "Nova Verbunden"
```

Für mehrere Batterien je einen `ble_client` und einen Eintrag unter `wattstunde_nova:` anlegen.

## Verfügbare Werte

| Option | Einheit | Beschreibung |
|---|---|---|
| `voltage` | V | Gesamtspannung |
| `current` | A | Strom (positiv = laden, negativ = entladen) |
| `power` | W | Leistung |
| `soc` | % | Ladezustand |
| `remaining_capacity` | Ah | Restladung (bei 100 % = tatsächliche Kapazität) |
| `design_capacity` | Ah | Nennkapazität |
| `stored_energy` | Wh | Gespeicherte Energie |
| `cycles` | – | Ladezyklen |
| `cell_voltage_1` … `cell_voltage_4` | V | Zellspannungen |
| `min_cell_voltage`, `max_cell_voltage`, `delta_cell_voltage` | V | Kleinste/größte Zellspannung und Differenz |
| `temperature_1` … `temperature_4` | °C | Temperaturen |
| `problem_code` | – | Fehlercode des BMS (0 = kein Fehler) |
| `charging` | Binär | Batterie lädt |
| `heater` | Binär | Heizung aktiv |
| `connected` | Binär | Daten aktuell (in den letzten 3 Intervallen empfangen) |

## Hinweise

- Nur ESP32 mit **ESP-IDF**, ESPHome ≥ 2026.9.
- Die NOVA-App und die Komponente können nicht gleichzeitig verbunden sein.
- Ist die Batterie zusätzlich in BMS_BLE eingerichtet, diesen Eintrag in Home Assistant entfernen.
- Für Rohdaten zur Fehlersuche: `logger: logs: wattstunde_nova: DEBUG`.

## Lizenz

Apache-2.0, siehe [LICENSE](LICENSE). Protokoll-Implementierung basierend auf
[aiobmsble](https://github.com/patman15/aiobmsble) von patman15.
