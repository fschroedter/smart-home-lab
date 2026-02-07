import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.components import web_server_base

DEPENDENCIES = ["web_server_base"]

web_server_routes_ns = cg.esphome_ns.namespace("web_server_routes")
WebServerRoutes = web_server_routes_ns.class_("WebServerRoutes", cg.Component)
RouteEntry = WebServerRoutes.class_("RouteEntry")

CONF_ROUTES = "routes"
CONF_PATH = "path"
CONF_CONTENT_TYPE = "content_type"
CONF_CONTENT_DISPOSITION = "content_disposition"
CONF_QUERY_KEY = "key"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
CONF_FILENAME = "filename"
CONF_UNIQUE_HEADER_FIELDS = "unique_header_fields"


def normalize_path(path: str) -> str:
    if not path:
        return "/"
    return "/" + path.strip("/")


def _validate_routes(config):
    # Get the global fallback/prefix path
    routes = config.get(CONF_ROUTES, [])
    global_path = config.get(CONF_PATH)

    seen = set()
    for route in routes:

        if CONF_PATH not in route:
            route[CONF_PATH] = global_path

        key = route.get(CONF_QUERY_KEY, "")
        path = normalize_path(route[CONF_PATH])

        # A route is unique based on its final path AND its key
        identifier = (path, key)

        if identifier in seen:
            raise cv.Invalid(
                f"Duplicate route found: Route '{path}' with "
                f"key '{key}' is defined more than once."
            )
        seen.add(identifier)

    return config


ROUTE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ID): cv.declare_id(RouteEntry),
        cv.Optional(CONF_LAMBDA): cv.lambda_,
        cv.Optional(CONF_PATH): cv.string,
        cv.Optional(CONF_QUERY_KEY, default=""): cv.string,
        cv.Optional(CONF_CONTENT_TYPE, default=""): cv.string,
        cv.Optional(CONF_UNIQUE_HEADER_FIELDS, default=True): cv.boolean,
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
    config[CONF_ROUTES].sort(key=lambda x: (x.get(CONF_QUERY_KEY, "") == "",))

    for route in config[CONF_ROUTES]:

        unique_hf = route[CONF_UNIQUE_HEADER_FIELDS]
        cg.add(var.set_unique_header_fields(unique_hf))

        route_id = route[CONF_ID]
        path = normalize_path(route[CONF_PATH])
        key = route.get(CONF_QUERY_KEY, "")
        content_type = route.get(CONF_CONTENT_TYPE, "")

        content_disposition = ""
        if CONF_FILENAME in route:
            content_disposition = f'attachment; filename="{route[CONF_FILENAME]}"'
        elif CONF_CONTENT_DISPOSITION in route:
            content_disposition = route[CONF_CONTENT_DISPOSITION]

        lambda_code = cg.RawExpression("nullptr")

        if CONF_LAMBDA in route:
            lambda_code = await cg.process_lambda(
                route[CONF_LAMBDA],
                [(WebServerRoutes.operator("ref"), "it")],
                return_type=cg.void,
            )

        route_var = cg.new_Pvariable(
            route_id,
            str(route_id),
            path,
            key,
            content_type,
            content_disposition,
            lambda_code,
        )
        cg.add(var.add_route(route_var))
