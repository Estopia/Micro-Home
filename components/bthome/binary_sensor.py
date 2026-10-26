import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import BTHomeHub, CONF_BTHOME_ID

DEPENDENCIES = ["bthome"]
CONF_BUTTON = "button"
CONF_BUTTON_B = "button_b"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BTHOME_ID): cv.use_id(BTHomeHub),
        cv.Required(CONF_BUTTON): binary_sensor.binary_sensor_schema(icon="mdi:gesture-tap-button"),
        cv.Optional(CONF_BUTTON_B): binary_sensor.binary_sensor_schema(icon="mdi:gesture-tap-button"),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BTHOME_ID])
    entity = await binary_sensor.new_binary_sensor(config[CONF_BUTTON])
    cg.add(hub.set_button(entity))
    if CONF_BUTTON_B in config:
        entity = await binary_sensor.new_binary_sensor(config[CONF_BUTTON_B])
        cg.add(hub.set_button_b(entity))