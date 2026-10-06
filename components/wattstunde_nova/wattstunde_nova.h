#pragma once
// Wattstunde NOVA (Core/Base) BMS per BLE - EXPERIMENTELL
// Protokoll portiert aus aiobmsble ws_nova_bms.py (Apache-2.0)

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <vector>

namespace esphome {
namespace wattstunde_nova {

namespace espbt = esphome::esp32_ble_tracker;

#define NOVA_SENSOR(name) \
 protected: \
  sensor::Sensor *name##_sensor_{nullptr}; \
 public: \
  void set_##name##_sensor(sensor::Sensor *s) { this->name##_sensor_ = s; }

#define NOVA_BINARY_SENSOR(name) \
 protected: \
  binary_sensor::BinarySensor *name##_binary_sensor_{nullptr}; \
 public: \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *s) { this->name##_binary_sensor_ = s; }

class WattstundeNova : public ble_client::BLEClientNode, public PollingComponent {
 public:
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  NOVA_SENSOR(voltage)
  NOVA_SENSOR(current)
  NOVA_SENSOR(power)
  NOVA_SENSOR(soc)
  NOVA_SENSOR(remaining_capacity)
  NOVA_SENSOR(design_capacity)
  NOVA_SENSOR(stored_energy)
  NOVA_SENSOR(cycles)
  NOVA_SENSOR(cell_voltage_1)
  NOVA_SENSOR(cell_voltage_2)
  NOVA_SENSOR(cell_voltage_3)
  NOVA_SENSOR(cell_voltage_4)
  NOVA_SENSOR(min_cell_voltage)
  NOVA_SENSOR(max_cell_voltage)
  NOVA_SENSOR(delta_cell_voltage)
  NOVA_SENSOR(temperature_1)
  NOVA_SENSOR(temperature_2)
  NOVA_SENSOR(temperature_3)
  NOVA_SENSOR(temperature_4)
  NOVA_SENSOR(problem_code)
  NOVA_BINARY_SENSOR(charging)
  NOVA_BINARY_SENSOR(heater)
  NOVA_BINARY_SENSOR(connected)

 public:
  void set_on_demand(bool on_demand) { this->on_demand_ = on_demand; }

 protected:
  void send_request_();
  void on_notify_(const uint8_t *data, uint16_t len);
  void decode_(const std::vector<uint8_t> &msg);
  void publish_unavailable_();

  uint16_t char_handle_{0};
  bool write_no_response_{false};
  std::vector<uint8_t> frame_;
  uint32_t frames_ok_{0};
  uint32_t requests_{0};

  // Verbinden bei Bedarf: verbinden, Daten holen, selbst trennen
  bool on_demand_{true};
  bool cycle_active_{true};  // beim Start verbindet ble_client automatisch
  uint32_t last_data_ms_{0};
};

}  // namespace wattstunde_nova
}  // namespace esphome

#endif  // USE_ESP32
