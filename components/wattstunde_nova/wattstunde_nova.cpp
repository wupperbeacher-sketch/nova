// Wattstunde NOVA (Core/Base) BMS per BLE - EXPERIMENTELL
// Protokoll portiert aus aiobmsble ws_nova_bms.py (Apache-2.0)

#include "wattstunde_nova.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <algorithm>

#ifdef USE_ESP32

namespace esphome {
namespace wattstunde_nova {

static const char *const TAG = "wattstunde_nova";

static const uint16_t SERVICE_UUID = 0xFFF0;
static const uint16_t CHAR_UUID = 0xFFF1;  // Notify und Write
static const uint8_t FRAME_HEAD = 0x3A;    // ':'
static const uint8_t FRAME_TAIL = 0x7E;    // '~'
static const size_t MIN_FRAME_LEN = 238;
static const size_t MAX_FRAME_LEN = 512;
// Abfragebefehl ":015150000EFE~"
static const uint8_t REQUEST[] = {0x3A, 0x30, 0x31, 0x35, 0x31, 0x35, 0x30, 0x30,
                                  0x30, 0x30, 0x45, 0x46, 0x45, 0x7E};

static int hex_val(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static uint32_t be(const std::vector<uint8_t> &m, size_t pos, size_t size) {
  uint32_t v = 0;
  for (size_t i = 0; i < size; i++) v = (v << 8) | m[pos + i];
  return v;
}

void WattstundeNova::dump_config() {
  ESP_LOGCONFIG(TAG, "Wattstunde NOVA BMS (experimentell)");
  ESP_LOGCONFIG(TAG, "  MAC: %s", this->parent()->address_str());
  ESP_LOGCONFIG(TAG, "  Verbinden bei Bedarf: %s", YESNO(this->on_demand_));
  LOG_UPDATE_INTERVAL(this);
}

void WattstundeNova::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "[%s] verbunden", this->parent()->address_str());
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      this->node_state = espbt::ClientState::IDLE;
      this->char_handle_ = 0;
      this->frame_.clear();
      if (this->on_demand_ && !this->cycle_active_) {
        // von uns selbst getrennt, nachdem die Daten da waren
        ESP_LOGD(TAG, "[%s] Verbindung planmäßig getrennt", this->parent()->address_str());
      } else {
        ESP_LOGW(TAG, "[%s] Verbindung getrennt (Grund 0x%02X)", this->parent()->address_str(),
                 param->disconnect.reason);
        if (!this->on_demand_) this->publish_unavailable_();
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent()->get_characteristic(SERVICE_UUID, CHAR_UUID);
      if (chr == nullptr) {
        ESP_LOGE(TAG, "[%s] Charakteristik FFF1 in Service FFF0 nicht gefunden",
                 this->parent()->address_str());
        break;
      }
      this->char_handle_ = chr->handle;
      this->write_no_response_ = (chr->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) &&
                                 !(chr->properties & ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGD(TAG, "[%s] FFF1 gefunden: Handle 0x%02X, Properties 0x%02X",
               this->parent()->address_str(), chr->handle, chr->properties);
      auto status = this->parent()->register_for_notify(chr->handle);
      if (status) {
        ESP_LOGW(TAG, "register_for_notify fehlgeschlagen, Status=%d", status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->char_handle_) break;
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "[%s] Benachrichtigungen aktiv, frage Daten ab",
               this->parent()->address_str());
      if (this->connected_binary_sensor_ != nullptr) this->connected_binary_sensor_->publish_state(true);
      this->send_request_();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id() ||
          param->notify.handle != this->char_handle_)
        break;
      this->on_notify_(param->notify.value, param->notify.value_len);
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Schreiben fehlgeschlagen, Status=%d",
                 this->parent()->address_str(), param->write.status);
      }
      break;
    }
    default:
      break;
  }
}

void WattstundeNova::update() {
  // "Verbunden" = in den letzten 3 Intervallen Daten erhalten
  if (this->connected_binary_sensor_ != nullptr && this->last_data_ms_ != 0 &&
      millis() - this->last_data_ms_ > 3 * this->get_update_interval()) {
    this->connected_binary_sensor_->publish_state(false);
  }

  if (this->on_demand_) {
    if (this->node_state == espbt::ClientState::ESTABLISHED) {
      this->send_request_();
      return;
    }
    if (this->cycle_active_) {
      ESP_LOGD(TAG, "[%s] Abfrage läuft noch, warte auf Verbindung", this->parent()->address_str());
      return;
    }
    ESP_LOGD(TAG, "[%s] starte Abfrage", this->parent()->address_str());
    this->cycle_active_ = true;
    this->parent()->set_auto_connect(true);  // ble_client verbindet beim nächsten Advertisement
    return;
  }

  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "[%s] nicht verbunden, keine Abfrage", this->parent()->address_str());
    return;
  }
  this->send_request_();
}

void WattstundeNova::send_request_() {
  if (this->char_handle_ == 0) return;
  this->frame_.clear();
  this->requests_++;
  auto status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->char_handle_,
      sizeof(REQUEST), const_cast<uint8_t *>(REQUEST),
      this->write_no_response_ ? ESP_GATT_WRITE_TYPE_NO_RSP : ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "[%s] Abfrage senden fehlgeschlagen, Status=%d",
             this->parent()->address_str(), status);
  } else {
    ESP_LOGV(TAG, "[%s] Abfrage gesendet", this->parent()->address_str());
  }
}

void WattstundeNova::on_notify_(const uint8_t *data, uint16_t len) {
  if (len == 0) return;
  if (data[0] == FRAME_HEAD) this->frame_.clear();
  this->frame_.insert(this->frame_.end(), data, data + len);

  if (this->frame_.empty() || this->frame_[0] != FRAME_HEAD || this->frame_.size() > MAX_FRAME_LEN) {
    ESP_LOGD(TAG, "ungültiger Frame-Anfang, verworfen");
    this->frame_.clear();
    return;
  }
  if (this->frame_.back() != FRAME_TAIL) return;  // Frame noch nicht vollständig

  size_t n = this->frame_.size();
  if ((n % 2) != 0 || n < MIN_FRAME_LEN) {
    ESP_LOGD(TAG, "falsche Frame-Länge (%u)", (unsigned) n);
    return;
  }
  for (size_t i = 1; i < n - 1; i++) {
    if (hex_val(this->frame_[i]) < 0) {
      ESP_LOGD(TAG, "Frame enthält ungültige Zeichen, verworfen");
      this->frame_.clear();
      return;
    }
  }

  // Schlüssel = Hex-Zeichen 7..8, Nutzdaten = Zeichen 1 .. n-3 (ohne Prüfsumme und '~')
  uint8_t key = (hex_val(this->frame_[7]) << 4) | hex_val(this->frame_[8]);
  std::vector<uint8_t> msg;
  msg.reserve((n - 4) / 2);
  for (size_t i = 1; i + 1 < n - 3; i += 2) {  // Zeichen 1 .. n-4
    uint8_t b = (hex_val(this->frame_[i]) << 4) | hex_val(this->frame_[i + 1]);
    msg.push_back(b ^ key);
  }
  this->frame_.clear();

  if (msg.size() < 2 || msg[0] != 0x01 || msg[1] != 0x54) {
    ESP_LOGD(TAG, "falscher Frame-Typ (%02X %02X)", msg.size() > 0 ? msg[0] : 0,
             msg.size() > 1 ? msg[1] : 0);
    return;
  }
  this->frames_ok_++;
  this->last_data_ms_ = millis();
  this->decode_(msg);

  if (this->on_demand_ && this->cycle_active_) {
    // Daten da: nicht sofort wieder verbinden und Verbindung selbst trennen
    this->cycle_active_ = false;
    this->parent()->set_auto_connect(false);
    this->set_timeout("nova_disconnect", 100, [this]() {
      ESP_LOGD(TAG, "[%s] Daten erhalten, trenne Verbindung", this->parent()->address_str());
      this->parent()->disconnect();
    });
  }
}

void WattstundeNova::decode_(const std::vector<uint8_t> &m) {
  const char *mac = this->parent()->address_str();
  // Rohdaten für die Analyse ins Log (Level DEBUG)
  char hexbuf[format_hex_pretty_size(128)];
  ESP_LOGD(TAG, "[%s] Nachricht (%u Bytes): %s", mac, (unsigned) m.size(),
           format_hex_pretty_to(hexbuf, m.data(), std::min<size_t>(m.size(), 128)));

  if (m.size() < 112) {
    ESP_LOGW(TAG, "[%s] Nachricht zu kurz (%u Bytes)", mac, (unsigned) m.size());
    return;
  }

  const size_t S = 44;  // Startoffset der Datenfelder
  uint16_t problem = be(m, S + 0, 2) & 0x0FFC;
  uint32_t raw_cur = be(m, S + 10, 4);
  float current = (raw_cur & 0x7FFF) / 1000.0f * ((raw_cur >> 15) ? -1.0f : 1.0f);
  float voltage = be(m, S + 18, 2) / 1000.0f;
  uint16_t cycles = be(m, S + 23, 2);
  uint8_t soc = m[S + 25];
  uint32_t design_cap = be(m, S + 26, 4) / 1000;
  float remaining = be(m, S + 30, 4) / 1000.0f;
  bool heater = be(m, S + 64, 4) != 0;

  // Zellspannungen: bis zu 16 Werte ab Byte 12, Nullwerte werden übersprungen
  std::vector<float> cells;
  for (size_t i = 0; i < 16; i++) {
    uint16_t v = be(m, 12 + i * 2, 2);
    if (v) cells.push_back(v / 1000.0f);
  }
  // Temperaturen: 4 Werte ab Byte 48, je 1 Byte, Offset 40
  float temps[4];
  for (size_t i = 0; i < 4; i++) temps[i] = (float) m[48 + i] - 40.0f;

  ESP_LOGI(TAG, "[%s] %.2f V, %.2f A, SoC %u %%, %.1f/%u Ah, %u Zyklen, %u Zellen, Fehler 0x%03X",
           mac, voltage, current, soc, remaining, (unsigned) design_cap, cycles,
           (unsigned) cells.size(), problem);

  if (this->voltage_sensor_) this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_) this->current_sensor_->publish_state(current);
  if (this->power_sensor_) this->power_sensor_->publish_state(voltage * current);
  if (this->soc_sensor_) this->soc_sensor_->publish_state(soc);
  if (this->remaining_capacity_sensor_) this->remaining_capacity_sensor_->publish_state(remaining);
  if (this->design_capacity_sensor_) this->design_capacity_sensor_->publish_state(design_cap);
  if (this->stored_energy_sensor_) this->stored_energy_sensor_->publish_state(remaining * voltage);
  if (this->cycles_sensor_) this->cycles_sensor_->publish_state(cycles);
  if (this->problem_code_sensor_) this->problem_code_sensor_->publish_state(problem);
  if (this->charging_binary_sensor_) this->charging_binary_sensor_->publish_state(current > 0.0f);
  if (this->heater_binary_sensor_) this->heater_binary_sensor_->publish_state(heater);
  if (this->connected_binary_sensor_) this->connected_binary_sensor_->publish_state(true);

  sensor::Sensor *cell_s[4] = {this->cell_voltage_1_sensor_, this->cell_voltage_2_sensor_,
                               this->cell_voltage_3_sensor_, this->cell_voltage_4_sensor_};
  for (size_t i = 0; i < 4; i++) {
    if (cell_s[i] && i < cells.size()) cell_s[i]->publish_state(cells[i]);
  }
  if (!cells.empty()) {
    float mn = cells[0], mx = cells[0];
    for (float c : cells) {
      mn = std::min(mn, c);
      mx = std::max(mx, c);
    }
    if (this->min_cell_voltage_sensor_) this->min_cell_voltage_sensor_->publish_state(mn);
    if (this->max_cell_voltage_sensor_) this->max_cell_voltage_sensor_->publish_state(mx);
    if (this->delta_cell_voltage_sensor_) this->delta_cell_voltage_sensor_->publish_state(mx - mn);
  }

  sensor::Sensor *temp_s[4] = {this->temperature_1_sensor_, this->temperature_2_sensor_,
                               this->temperature_3_sensor_, this->temperature_4_sensor_};
  for (size_t i = 0; i < 4; i++) {
    if (temp_s[i]) temp_s[i]->publish_state(temps[i]);
  }
}

void WattstundeNova::publish_unavailable_() {
  if (this->connected_binary_sensor_ != nullptr) this->connected_binary_sensor_->publish_state(false);
}

}  // namespace wattstunde_nova
}  // namespace esphome

#endif  // USE_ESP32
