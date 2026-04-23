import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@EwoutsAgents"]

CONF_DEGRADED_RSSI_THRESHOLD = "degraded_rssi_threshold"
CONF_HARD_FAILURE_TIMEOUT = "hard_failure_timeout"
CONF_DEGRADE_PERSIST_TIME = "degrade_persist_time"
CONF_HOLD_DOWN_TIME = "hold_down_time"
CONF_STANDBY_REFRESH_INTERVAL = "standby_refresh_interval"
CONF_FAILOVER_ELIGIBLE_DELAY = "failover_eligible_delay"
CONF_POST_FAILOVER_STABILIZE_TIME = "post_failover_stabilize_time"
CONF_PREFERRED_REATTACH_TIMEOUT = "preferred_reattach_timeout"
CONF_ENABLE_PARENT_RESPONSE_CALLBACK = "enable_parent_response_callback"
CONF_CONTROL_PLANE_ERROR_THRESHOLD = "control_plane_error_threshold"
CONF_STANDBY_REPLACE_HYSTERESIS = "standby_replace_hysteresis"

biparental_ed_ns = cg.esphome_ns.namespace("biparental_ed")
BiparentalEDComponent = biparental_ed_ns.class_(
    "BiparentalEDComponent", cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BiparentalEDComponent),
        cv.Optional(CONF_DEGRADED_RSSI_THRESHOLD, default=-70): cv.int_range(
            min=-120, max=-1
        ),
        cv.Optional(CONF_HARD_FAILURE_TIMEOUT, default="15s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DEGRADE_PERSIST_TIME, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HOLD_DOWN_TIME, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_STANDBY_REFRESH_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_FAILOVER_ELIGIBLE_DELAY, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_POST_FAILOVER_STABILIZE_TIME, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PREFERRED_REATTACH_TIMEOUT, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ENABLE_PARENT_RESPONSE_CALLBACK, default=True): cv.boolean,
        cv.Optional(CONF_CONTROL_PLANE_ERROR_THRESHOLD, default=3): cv.positive_int,
        cv.Optional(CONF_STANDBY_REPLACE_HYSTERESIS, default=5): cv.int_range(
            min=0, max=50
        ),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_degraded_rssi_threshold(config[CONF_DEGRADED_RSSI_THRESHOLD]))
    cg.add(
        var.set_hard_failure_timeout_ms(
            config[CONF_HARD_FAILURE_TIMEOUT].total_milliseconds
        )
    )
    cg.add(
        var.set_degrade_persist_time_ms(
            config[CONF_DEGRADE_PERSIST_TIME].total_milliseconds
        )
    )
    cg.add(var.set_hold_down_time_ms(config[CONF_HOLD_DOWN_TIME].total_milliseconds))
    cg.add(
        var.set_standby_refresh_interval_ms(
            config[CONF_STANDBY_REFRESH_INTERVAL].total_milliseconds
        )
    )
    cg.add(
        var.set_failover_eligible_delay_ms(
            config[CONF_FAILOVER_ELIGIBLE_DELAY].total_milliseconds
        )
    )
    cg.add(
        var.set_post_failover_stabilize_time_ms(
            config[CONF_POST_FAILOVER_STABILIZE_TIME].total_milliseconds
        )
    )
    cg.add(
        var.set_preferred_reattach_timeout_ms(
            config[CONF_PREFERRED_REATTACH_TIMEOUT].total_milliseconds
        )
    )
    cg.add(
        var.set_enable_parent_response_callback(
            config[CONF_ENABLE_PARENT_RESPONSE_CALLBACK]
        )
    )
    cg.add(
        var.set_control_plane_error_threshold(
            config[CONF_CONTROL_PLANE_ERROR_THRESHOLD]
        )
    )
    cg.add(
        var.set_standby_replace_hysteresis(config[CONF_STANDBY_REPLACE_HYSTERESIS])
    )
