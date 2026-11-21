# NuggetProxy – Anleitung

Der **NuggetProxy** ist ein kleiner **M5Stack ATOM Lite** im Ford Nugget. Er sitzt bei den Batterien und erledigt mehrere Aufgaben gleichzeitig:

- 🔋 liest die beiden **Wattstunde-NOVA-Base-Batterien** per Bluetooth aus
- ☀️ empfängt die Werte von **Victron Orion XS** (Ladebooster) und **SmartSolar** (Solarregler)
- 🌀 steuert den **MaxxFan Deluxe** per Infrarot, wie die Originalfernbedienung
- 💡 zeigt mit seiner **LED** auf einen Blick, ob alles in Ordnung ist

Alle Werte landen als ganz normale Entitäten in Home Assistant.

---

## Inhalt

1. [Was du brauchst](#1-was-du-brauchst)
2. [Einrichten im ESPHome Builder](#2-einrichten-im-esphome-builder)
3. [WLAN: Nugget, Zuhause und Notfall-Hotspot](#3-wlan-nugget-zuhause-und-notfall-hotspot)
4. [Die Status-LED](#4-die-status-led)
5. [Die Taste am ATOM](#5-die-taste-am-atom)
6. [MaxxFan bedienen](#6-maxxfan-bedienen)
7. [Batterien und Victron](#7-batterien-und-victron)
8. [Fehlersuche](#8-fehlersuche)
9. [Ideen für später](#9-ideen-für-später)

---

## 1. Was du brauchst

| Was | Wofür |
|---|---|
| M5Stack ATOM Lite | der Proxy selbst (ESP32 mit Taste, RGB-LED und IR-Diode) |
| Home Assistant mit **ESPHome Builder** | zum Erstellen und Aufspielen der Firmware |
| WLAN im Nugget (Router „LippyG5“) | damit der ATOM mit Home Assistant spricht |
| Victron-**Bindkeys** | zum Entschlüsseln der Victron-Daten (in der VictronConnect-App unter *Produktinfo → Instant Readout*) |

Die Firmware nutzt drei Erweiterungen (`external_components`):

| Erweiterung | Quelle | Zweck |
|---|---|---|
| `wattstunde_nova` | dieses Repository | NOVA-Batterien |
| `victron_ble` | dieses Repository | Victron-Geräte, angepasst für ESPHome 2026.x (siehe [Abschnitt 7](#7-batterien-und-victron)) |
| `maxxfan_protocol` | [brown-studios/esphome-maxxfan-protocol](https://github.com/brown-studios/esphome-maxxfan-protocol) | Infrarot-Protokoll des MaxxFan |

---

## 2. Einrichten im ESPHome Builder

### 2.1 Geheimnisse in die `secrets.yaml`

Passwörter und Schlüssel stehen **nicht** in der Geräte-YAML, sondern in der `secrets.yaml` des Builders. Du findest sie oben rechts unter **„Secrets“**. Dort trägst du ein:

```yaml
wifi_ssid_1: "LippyG5"                  # Haupt-WLAN im Nugget
wifi_password_1: "…"

wifi_ssid_2: "NAME_DES_HEIM_WLANS"       # Reserve-WLAN zu Hause
wifi_password_2: "…"

nuggetproxy_ap_password: "…"             # Passwort für den Notfall-Hotspot (mind. 8 Zeichen)
nuggetproxy_api_key: "…"                 # API-Schlüssel (32 Byte, Base64)
```

> 💡 Ein neuer API-Schlüssel lässt sich auf [esphome.io](https://esphome.io/components/api/) mit einem Klick erzeugen. Wenn du ihn änderst, fragt Home Assistant beim nächsten Verbinden nach dem neuen Schlüssel.

### 2.2 Erweiterungen einbinden

In der Geräte-YAML des NuggetProxy:

```yaml
external_components:
  # NOVA-Batterien + reparierte Victron-Komponente aus diesem Repository
  - source: github://wupperbeacher-sketch/nova@main
    refresh: always
    components: [ wattstunde_nova, victron_ble ]
  - source: github://brown-studios/esphome-maxxfan-protocol@main
    components: [ maxxfan_protocol ]
```

### 2.3 Aufspielen

1. Im ESPHome Builder beim NuggetProxy auf **„Install“** klicken.
2. **„Wirelessly“** wählen. Das klappt nur, wenn der ATOM gerade im LippyG5-Netz ist (siehe [Abschnitt 3](#3-wlan-nugget-zuhause-und-notfall-hotspot)).
3. Der erste Build dauert ein paar Minuten, spätere gehen deutlich schneller.

---

## 3. WLAN: Nugget, Zuhause und Notfall-Hotspot

Der ATOM kennt **zwei WLANs** und wählt selbst:

| Netz | Priorität | Wann | IP des ATOM |
|---|---|---|---|
| **LippyG5** (Router im Nugget) | hoch | normalerweise | `192.168.8.132` |
| **Heim-WLAN** (FritzBox) | niedrig | nur wenn LippyG5 nicht da ist, z. B. Router aus, Nugget steht zu Hause | `192.168.178.132` (feste IP, siehe unten) |
| **NuggetProxyFallback** (eigener Hotspot) | Notfall | wenn **keines** der beiden WLANs erreichbar ist | `192.168.4.1` |

```yaml
wifi:
  networks:
    - ssid: !secret wifi_ssid_1
      password: !secret wifi_password_1
      priority: 10
    - ssid: !secret wifi_ssid_2
      password: !secret wifi_password_2
      priority: 5
  use_address: 192.168.8.132
  reboot_timeout: 0s
  ap:
    ssid: NuggetProxyFallback
    password: !secret nuggetproxy_ap_password
```

### Feste IP zu Hause (FritzBox)

Damit du den ATOM auch zu Hause immer unter derselben Adresse findest:

1. `fritz.box` öffnen → **Heimnetz → Netzwerk → Netzwerkverbindungen**
2. Bei **nuggetproxy** auf den Stift klicken
3. **„Diesem Netzwerkgerät immer die gleiche IPv4-Adresse zuweisen“** anhaken → `192.168.178.132`
4. **Übernehmen** und den ATOM neu starten

### Gut zu wissen

- `use_address` sagt dem Builder, wo er den ATOM findet, und gilt nur für **LippyG5**.
  Willst du **zu Hause** ein Update aufspielen, ändere die Zeile vorübergehend auf `192.168.178.132` und danach wieder zurück.
- Home Assistant findet den ATOM im Heimnetz meist von selbst wieder. Klappt das nach dem Wechsel zurück in den Nugget nicht, stellst du die Adresse unter *Einstellungen → Geräte & Dienste → ESPHome → NuggetProxy* wieder auf `192.168.8.132`.
- Ist der Nugget **unterwegs** und der Router fällt aus, gibt es kein Netz. Dann kommen keine Werte in HA an. Der ATOM läuft trotzdem weiter und verbindet sich von selbst neu, sobald der Router wieder da ist.
- **Tailscale:** HA erreicht den ATOM über den Router im Nugget. Läuft Tailscale auf dem Router und gibt das Netz `192.168.8.0/24` frei, behält der ATOM immer seine Adresse, egal wie der Router ins Internet kommt. Tailscale direkt auf dem ATOM ist nicht sinnvoll, weil der ATOM Lite dafür zu wenig Arbeitsspeicher hat.

---

## 4. Die Status-LED

Die LED zeigt immer das **wichtigste** aktuelle Thema. Was in der Tabelle weiter oben steht, hat Vorrang:

| LED | Bedeutung | Was tun? |
|---|---|---|
| 🔵 **Blau**, kurz | Taste gedrückt, Neustart folgt | nichts, das ist gewollt |
| 🔴 **Rot** | kein WLAN | Router im Nugget prüfen |
| 🟠 **Orange** | WLAN da, aber **Home Assistant nicht verbunden** | Internet/Tailscale des Routers prüfen, läuft HA zu Hause? |
| 🟡 **Gelb, pulsierend** | eine **NOVA-Batterie unter 10 %** | laden! |
| 🩵 **Türkis, schwach** | der **MaxxFan läuft** | nichts |
| 🟢 **Grün** | alles in Ordnung | nichts. Nach **5 Minuten** geht die LED aus, damit sie nachts nicht stört |
| ⚫ **Aus** | alles in Ordnung (Grün ist abgelaufen) | nichts |

Der ATOM prüft jede Sekunde den Zustand und ändert die LED nur, wenn sich etwas tut. Nach einer Störung, etwa wenn das WLAN kurz weg war, leuchtet sie wieder 5 Minuten grün und geht dann aus.

> Die LED wird nur vom ATOM selbst gesteuert und taucht in Home Assistant nicht als Licht auf.

---

## 5. Die Taste am ATOM

| Aktion | Wirkung |
|---|---|
| **kurz drücken** | LED blinkt kurz blau, dann **startet der ATOM neu** |

Das hilft, wenn mal etwas hängt. Ein Neustart ist auch in HA über **„NuggetProxy Neustart“** möglich, dazu gibt es **„NuggetProxy Neustart (Safe Mode)“** für den Notfall.

> Nach einem Neustart steht der MaxxFan in HA auf **aus**. Der Lüfter selbst bekommt dabei keinen Befehl und läuft einfach weiter, wie er war.

---

## 6. MaxxFan bedienen

### So funktioniert die Steuerung

Der ATOM sendet per Infrarot **immer den kompletten Zustand** an den Lüfter, genau wie die Originalfernbedienung: an/aus, Stufe, Richtung, Haube, Automatik und Soll-Temperatur. Eine Rückmeldung vom Lüfter gibt es nicht. HA zeigt also, **was zuletzt gesendet wurde**.

### Die Entitäten in Home Assistant

| Entität | Funktion |
|---|---|
| **MaxxFan** | Lüfter an/aus, **Stufe 1–10** (10 % bis 100 %), **Richtung** (Vorwärts = Abluft, Rückwärts = Zuluft) |
| **MaxxFan Haube offen** | Haube öffnen, ohne dass der Lüfter läuft |
| **MaxxFan Automatik** | Thermostat-Betrieb: der Lüfter regelt Haube und Drehzahl selbst |
| **MaxxFan Soll-Temperatur** | Zieltemperatur für die Automatik (−2 bis 37 °C) |
| **MaxxFan Zustand erneut senden** | schickt den aktuellen Zustand noch einmal, praktisch wenn ein Signal verloren ging |

### Wann ist die Haube offen?

Die Haube ist offen, sobald **der Lüfter läuft ODER „Haube offen“ an ist**:

| MaxxFan | Haube offen | Haube |
|---|---|---|
| aus | aus | **zu** |
| aus | an | auf |
| an | aus | auf |
| an | an | auf |

Zum **Schließen** müssen also **beide** aus sein.

### Automatik

- Beim Einschalten der Automatik gehen „MaxxFan“ und „Haube offen“ automatisch aus, ab dann regelt der Lüfter selbst.
- Schaltest du während der Automatik den Lüfter oder die Haube von Hand, wird die Automatik beendet.
- Änderst du die Soll-Temperatur, wird sie sofort gesendet, wenn die Automatik an ist.

### Tipps für ein sicheres IR-Signal

Die IR-Diode im ATOM ist klein und reicht nicht weit. Wenn ein Befehl mal nicht ankommt:

- den ATOM möglichst mit **freier Sicht** auf den Empfänger des MaxxFan anbringen
- eine **offene Haube** kann den Empfänger teilweise verdecken, deshalb klappt „zu“ manchmal schlechter als „auf“
- direkte **Sonne** stört Infrarot
- im Zweifel **„MaxxFan Zustand erneut senden“** drücken

---

## 7. Batterien und Victron

### NOVA-Base-Batterien

Beide Batterien werden alle 30 Sekunden kurz verbunden, ausgelesen und wieder getrennt (`on_demand`). Zu jeder Batterie gibt es unter anderem Spannung, Strom, Leistung, Ladezustand, Restladung, Zellspannungen, Zell-Delta, Temperaturen, Ladezyklen, Fehlercode und Heizung. Mit **„Nova Base 1/2 RSSI“** siehst du die Funkstärke aus Sicht des ATOM.

Details zur Komponente stehen in der [README](README.md).

### Victron Orion XS und SmartSolar

Die Victron-Geräte senden ihre Werte verschlüsselt per Bluetooth („Instant Readout“). Der ATOM entschlüsselt sie mit den **Bindkeys**. Die Werte werden über **30 Sekunden gemittelt**, das schont die Datenbank von Home Assistant.

### Warum liegt `victron_ble` in diesem Repository?

Die Original-Komponente von [Fabian-Schmidt/esphome-victron_ble](https://github.com/Fabian-Schmidt/esphome-victron_ble) wird seit Mitte 2025 nicht mehr gepflegt. Ab **ESPHome 2026.11** bricht sie beim Kompilieren ab:

```
error: 'str_snprintf' was not declared in this scope
```

Die Kopie unter `components/victron_ble/` ist bis auf eine Stelle identisch: `address_str()` in `victron_ble.h` nutzt jetzt `snprintf`. Die Komponente steht unter **GPL-3.0**, die Lizenzdatei liegt im Ordner.

---

## 8. Fehlersuche

| Problem | Mögliche Ursache und Lösung |
|---|---|
| **LED rot** | kein WLAN: Router im Nugget an? Steht das Heim-WLAN richtig in der `secrets.yaml`? |
| **LED orange** | HA nicht verbunden: Internet oder Tailscale am Router prüfen, HA zu Hause erreichbar? |
| **Builder: „Update queued“ / Gerät offline** | Der ATOM ist nicht unter `use_address` erreichbar, z. B. weil er gerade im Heim-WLAN ist. Siehe [Abschnitt 3](#3-wlan-nugget-zuhause-und-notfall-hotspot). |
| **Builder bricht beim Kompilieren ab** | Log ab der Zeile `FAILED` ansehen. Bei `str_snprintf` stimmt die Victron-Quelle nicht, sie muss aus diesem Repository kommen (Abschnitt 2.2). |
| **Haube geht nicht zu** | Sind **„MaxxFan“ und „Haube offen“ beide aus**? Dann „Zustand erneut senden“. Siehe [Abschnitt 6](#6-maxxfan-bedienen). |
| **NOVA-Werte fehlen** | „Nova Base X Verbunden“ und RSSI prüfen. Unter etwa −90 dBm ist der Empfang zu schwach. |
| **Victron-Werte fehlen** | Bindkey und MAC-Adresse prüfen. In VictronConnect muss „Instant Readout“ eingeschaltet sein. |
| **ATOM gar nicht erreichbar** | Mit dem Hotspot **NuggetProxyFallback** verbinden und `192.168.4.1` öffnen. Dort lassen sich WLAN-Daten neu eingeben. |

**Logs ansehen:** Im ESPHome Builder beim NuggetProxy auf **„Logs“** klicken. Alles, was der ATOM tut, steht dort live, etwa `Sende Zustand an MaxxFan`.

---

## 9. Ideen für später

- **IR-Einheit für den MaxxFan:** Die **M5Stack Mini Infrared Unit (U002)** kostet etwa 5 € und kommt an den Grove-Anschluss des ATOM. Mit Kabel lässt sie sich direkt auf den MaxxFan ausrichten. Außerdem kann sie die **Originalfernbedienung mithören**, sodass HA auch Änderungen per Fernbedienung mitbekommt.
- **Befehle mehrfach senden:** Jeden MaxxFan-Befehl 2–3× schicken, damit seltener ein Signal verloren geht.
- **Haube als eigenes Element:** In HA „Öffnen“ und „Schließen“ statt eines Schalters.

---

*Stand: November 2026 · ESPHome 2026.11*
