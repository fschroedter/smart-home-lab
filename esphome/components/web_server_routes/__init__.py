import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.components import web_server_base

DEPENDENCIES = ["web_server_base"]

web_server_routes_ns = cg.esphome_ns.namespace("web_server_routes")
WebServerRoutes = web_server_routes_ns.class_("WebServerRoutes", cg.Component)

CONF_ROUTES = "routes"
CONF_PATH = "path"
CONF_SUBPATH = "subpath"
CONF_CONTENT_TYPE = "content_type"
CONF_CONTENT_DISPOSITION = "content_disposition"
CONF_KEY = "key"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
CONF_FILENAME = "filename"


def get_joined_path(conf_path, conf_subpath):
    """
    Normalizes and joins path and subpath for URL construction.
    """
    # Ensure inputs are strings and strip leading/trailing slashes
    path_part = (conf_path or "").strip("/")
    subpath_part = (conf_subpath or "").strip("/")

    # Filter out empty parts and join with a single slash
    return "/" + "/".join(filter(None, [path_part, subpath_part]))


def _validate_routes(config):
    # Get the global fallback/prefix path
    routes = config.get(CONF_ROUTES, [])

    seen = set()
    for route in routes:
        # If local path is missing or empty, fallback to global_path
        route_key = route.get(CONF_KEY, "")
        path = route.get(CONF_PATH) or config[CONF_PATH]
        subpath = route.get(CONF_SUBPATH, "")
        final_path = get_joined_path(path, subpath)

        # A route is unique based on its final path AND its key
        identifier = (final_path, route_key)

        if identifier in seen:
            raise cv.Invalid(
                f"Duplicate route found: Route '{final_path}' with "
                f"key '{route_key}' is defined more than once."
            )
        seen.add(identifier)

    return config


ROUTE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LAMBDA): cv.lambda_,
        cv.Optional(CONF_PATH): cv.string,
        cv.Optional(CONF_SUBPATH): cv.string,
        cv.Optional(CONF_KEY, default=""): cv.string,
        cv.Optional(CONF_CONTENT_TYPE, default=""): cv.string,
        cv.Exclusive(CONF_CONTENT_DISPOSITION, "disposition"): cv.string,
        cv.Exclusive(CONF_FILENAME, "disposition"): cv.string,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServerRoutes),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_PATH, default="download"): cv.string,
            cv.Required(CONF_ROUTES): cv.ensure_list(ROUTE_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_routes,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    cg.add(var.set_web_server_base(base))

    # Sort routes to ensure specific keys are matched before generic empty-key
    config[CONF_ROUTES].sort(key=lambda x: (x.get(CONF_KEY, "") == "",))

    for route in config[CONF_ROUTES]:
        lambda_ = await cg.process_lambda(
            route[CONF_LAMBDA],
            [(WebServerRoutes.operator("ref"), "it")],
            return_type=cg.void,
        )

        disposition = ""
        if CONF_FILENAME in route:
            disposition = f'attachment; filename="{route[CONF_FILENAME]}"'
        elif CONF_CONTENT_DISPOSITION in route:
            disposition = route[CONF_CONTENT_DISPOSITION]

        path = route.get(CONF_PATH) or config[CONF_PATH]
        subpath = route.get(CONF_SUBPATH)
        final_path = get_joined_path(path, subpath)

        key = route.get(CONF_KEY, "")

        cg.add(
            var.add_route(
                final_path,
                key,
                lambda_,
                route[CONF_CONTENT_TYPE],
                disposition,
            )
        )
