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
CONF_QUERY_KEY = "key"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
CONF_FILENAME = "filename"
CONF_UNIQUE_HEADER_FIELDS = "unique_header_fields"
CONF_HEADERS = "headers"
CONF_HEADER_CONTENT_TYPE = "content_type"
CONF_HEADER_CONTENT_DISPOSITION = "content_disposition"
CONF_HEADER_CACHE_CONTROL = "cache_control"
CONF_HEADER_CONNECTION = "connection"


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


#   this->set_header("Cache-Control", "no-cache");
#   this->set_header("Connection", "close");
ROUTE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ID): cv.declare_id(RouteEntry),
        cv.Optional(CONF_LAMBDA): cv.lambda_,
        cv.Optional(CONF_PATH): cv.string,
        cv.Optional(
            CONF_HEADERS,
            default=[
                "Cache-Control: no-cache",
                "Connection: close",
                "X-Custom-Header: ABC",
            ],
        ): cv.ensure_list(cv.string),
        cv.Optional(CONF_UNIQUE_HEADER_FIELDS, default=True): cv.boolean,
        cv.Optional(CONF_QUERY_KEY, default=""): cv.string,
        cv.Optional(CONF_HEADER_CACHE_CONTROL, default="no-cache"): cv.string,
        cv.Optional(CONF_HEADER_CONNECTION, default="close"): cv.string,
        cv.Optional(CONF_HEADER_CONTENT_TYPE, default=""): cv.string,
        cv.Exclusive(CONF_HEADER_CONTENT_DISPOSITION, "disposition"): cv.string,
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

    for route_conf in config[CONF_ROUTES]:

        unique_hf = route_conf[CONF_UNIQUE_HEADER_FIELDS]
        cg.add(var.set_unique_header_fields(unique_hf))

        route_id = route_conf[CONF_ID]
        path = normalize_path(route_conf[CONF_PATH])
        key = route_conf.get(CONF_QUERY_KEY, "")

        lambda_code = cg.RawExpression("nullptr")

        if CONF_LAMBDA in route_conf:
            lambda_code = await cg.process_lambda(
                route_conf[CONF_LAMBDA],
                [(WebServerRoutes.operator("ref"), "it")],
                return_type=cg.void,
            )

        route_var = cg.new_Pvariable(
            route_id,
            str(route_id),
            path,
            key,
            lambda_code,
        )

        header_cache_controle = route_conf.get(CONF_HEADER_CACHE_CONTROL, "")
        header_connection = route_conf.get(CONF_HEADER_CONNECTION, "")
        header_content_type = route_conf.get(CONF_HEADER_CONTENT_TYPE, "")

        header_content_disposition = ""
        if CONF_FILENAME in route_conf:
            header_content_disposition = (
                f'attachment; filename="{route_conf[CONF_FILENAME]}"'
            )
        elif CONF_HEADER_CONTENT_DISPOSITION in route_conf:
            header_content_disposition = route_conf[CONF_HEADER_CONTENT_DISPOSITION]

        cg.add(var.add_route(route_var))
        cg.add(route_var.add_header("Cache-Control", header_cache_controle))
        cg.add(route_var.add_header("Connection", header_connection))
        cg.add(route_var.add_header("Content-Type", header_content_type))
        cg.add(route_var.add_header("Content-Disposition", header_content_disposition))

        if CONF_HEADERS in route_conf:
            for header_string in route_conf[CONF_HEADERS]:
                cg.add(route_var.set_header(header_string))  # Add or update
