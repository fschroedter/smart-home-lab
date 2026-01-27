import esphome.codegen as cg
import esphome.config_validation as cv

gfx_blend_ns = cg.global_ns.namespace("gfx_blend")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(cg.Component),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)
