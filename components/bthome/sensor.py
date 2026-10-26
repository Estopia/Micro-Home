import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from . import BTHomeHub, CONF_BTHOME_ID

DEPENDENCIES = ["bthome"]
CONF_ACCELERATION = "acceleration"
CONF_HEADING = "heading"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BTHOME_ID): cv.use_id(BTHomeHub),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ACCELERATION): sensor.sensor_schema(
            unit_of_measurement="m/s\u00b2",
            accuracy_decimals=3,
            icon="mdi:axis-arrow",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HEADING): sensor.sensor_schema(
            unit_of_measurement="\u00b0",
            accuracy_decimals=2,
            icon="mdi:compass",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BTHOME_ID])
    if CONF_TEMPERATURE in config:
        entity = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(hub.set_temperature(entity))
    if CONF_ACCELERATION in config:
        entity = await sensor.new_sensor(config[CONF_ACCELERATION])
        cg.add(hub.set_acceleration(entity))
    if CONF_HEADING in config:
        entity = await sensor.new_sensor(config[CONF_HEADING])
        cg.add(hub.set_heading(entity))