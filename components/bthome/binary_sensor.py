import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import BTHomeHub, CONF_BTHOME_ID

DEPENDENCIES = ["bthome"]
CONF_BUTTON = "button"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BTHOME_ID): cv.use_id(BTHomeHub),
        cv.Required(CONF_BUTTON): binary_sensor.binary_sensor_schema(icon="mdi:gesture-tap-button"),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BTHOME_ID])
    entity = await binary_sensor.new_binary_sensor(config[CONF_BUTTON])
    cg.add(hub.set_button(entity))