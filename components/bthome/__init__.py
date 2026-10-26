import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_device_base
from esphome.const import CONF_ID, CONF_MAC_ADDRESS

AUTO_LOAD = ["ble_device_base", "sensor", "binary_sensor"]
DEPENDENCIES = ["esp32_ble_tracker"]
MULTI_CONF = True

bthome_ns = cg.esphome_ns.namespace("bthome")
BTHomeHub = bthome_ns.class_(
    "BTHomeHub", cg.Component, ble_device_base.ESPBTDeviceListener
)
CONF_BTHOME_ID = "bthome_id"
CONF_STALE_TIMEOUT = "stale_timeout"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BTHomeHub),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_STALE_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_stale_timeout(config[CONF_STALE_TIMEOUT].total_milliseconds))