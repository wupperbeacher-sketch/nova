"""ESPHome-Komponente: Wattstunde NOVA (Core/Base) BMS direkt per BLE auslesen.

Protokoll portiert aus aiobmsble (ws_nova_bms.py, Apache-2.0),
https://github.com/patman15/aiobmsble
EXPERIMENTELL - noch nicht an echten Batterien getestet.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CODEOWNERS = []
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor"]
MULTI_CONF = True

nova_ns = cg.esphome_ns.namespace("wattstunde_nova")
WattstundeNova = nova_ns.class_(
    "WattstundeNova", ble_client.BLEClientNode, cg.PollingComponent
)


def _s(unit, decimals, device_class=None, state_class=STATE_CLASS_MEASUREMENT,
       icon=None, entity_category=None):
    kwargs = {"unit_of_measurement": unit, "accuracy_decimals": decimals,
              "state_class": state_class}
    if device_class:
        kwargs["device_class"] = device_class
    if icon:
        kwargs["icon"] = icon
    if entity_category:
        kwargs["entity_category"] = entity_category
    return sensor.sensor_schema(**kwargs)


SENSORS = {
    "voltage": _s("V", 2, "voltage"),
    "current": _s("A", 2, "current"),
    "power": _s("W", 1, "power"),
    "soc": _s("%", 0, "battery"),
    "remaining_capacity": _s("Ah", 1, icon="mdi:battery-50"),
    "design_capacity": _s("Ah", 0, icon="mdi:battery",
                          entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "stored_energy": _s("Wh", 0, icon="mdi:lightning-bolt"),
    "cycles": _s("", 0, icon="mdi:battery-sync",
                 state_class=STATE_CLASS_TOTAL_INCREASING,
                 entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "cell_voltage_1": _s("V", 3, "voltage"),
    "cell_voltage_2": _s("V", 3, "voltage"),
    "cell_voltage_3": _s("V", 3, "voltage"),
    "cell_voltage_4": _s("V", 3, "voltage"),
    "min_cell_voltage": _s("V", 3, "voltage"),
    "max_cell_voltage": _s("V", 3, "voltage"),
    "delta_cell_voltage": _s("V", 3, "voltage"),
    "temperature_1": _s("°C", 0, "temperature"),
    "temperature_2": _s("°C", 0, "temperature"),
    "temperature_3": _s("°C", 0, "temperature"),
    "temperature_4": _s("°C", 0, "temperature"),
    "problem_code": _s("", 0, icon="mdi:alert-circle-outline",
                       entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
}

BINARY_SENSORS = {
    "charging": binary_sensor.binary_sensor_schema(device_class="battery_charging"),
    "heater": binary_sensor.binary_sensor_schema(device_class="heat"),
    "connected": binary_sensor.binary_sensor_schema(
        device_class="connectivity", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WattstundeNova),
            # True: verbinden, Daten holen, selbst trennen (schont Batterie und Funk)
            cv.Optional("on_demand", default=True): cv.boolean,
            **{cv.Optional(k): v for k, v in SENSORS.items()},
            **{cv.Optional(k): v for k, v in BINARY_SENSORS.items()},
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_on_demand(config["on_demand"]))

    for key in SENSORS:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    for key in BINARY_SENSORS:
        if key in config:
            bs = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_binary_sensor")(bs))
