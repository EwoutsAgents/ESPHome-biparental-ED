import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor
from esphome.const import CONF_ID, DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
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

CONF_NEIGHBOR_SCAN_INTERVAL = "neighbor_scan_interval"
CONF_NEIGHBOR_MAX_AGE = "neighbor_max_age"
CONF_PARENT_SEARCH_REFRESH_INTERVAL = "parent_search_refresh_interval"

CONF_RUNTIME_DEBUG_ENABLED = "runtime_debug_enabled"
CONF_RUNTIME_DEBUG_INTERVAL = "runtime_debug_interval"
CONF_VERBOSE_DIAGNOSTICS = "verbose_diagnostics"
CONF_DIAGNOSTICS_PUBLISH_INTERVAL = "diagnostics_publish_interval"

CONF_ACTIVE_PARENT_ID = "active_parent_id"
CONF_STANDBY_PARENT_ID = "standby_parent_id"
CONF_FAILOVER_COUNT = "failover_count"
CONF_DEGRADED_STATE = "degraded_state"
CONF_LAST_FAILOVER_REASON = "last_failover_reason"

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
        cv.Optional(CONF_NEIGHBOR_SCAN_INTERVAL, default="10s"): cv.All(
            cv.time_period, cv.time_period_in_milliseconds_
        ),
        cv.Optional(CONF_NEIGHBOR_MAX_AGE, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PARENT_SEARCH_REFRESH_INTERVAL, default="0s"): cv.All(
            cv.time_period, cv.time_period_in_milliseconds_
        ),
        cv.Optional(CONF_RUNTIME_DEBUG_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_RUNTIME_DEBUG_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_VERBOSE_DIAGNOSTICS, default=True): cv.boolean,
        cv.Optional(CONF_DIAGNOSTICS_PUBLISH_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ACTIVE_PARENT_ID): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_STANDBY_PARENT_ID): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_FAILOVER_COUNT): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DEGRADED_STATE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_LAST_FAILOVER_REASON): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
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
    cg.add(
        var.set_neighbor_scan_interval_ms(
            config[CONF_NEIGHBOR_SCAN_INTERVAL].total_milliseconds
        )
    )
    cg.add(
        var.set_neighbor_max_age_ms(config[CONF_NEIGHBOR_MAX_AGE].total_milliseconds)
    )
    cg.add(
        var.set_parent_search_refresh_interval_ms(
            config[CONF_PARENT_SEARCH_REFRESH_INTERVAL].total_milliseconds
        )
    )
    cg.add(var.set_runtime_debug_enabled(config[CONF_RUNTIME_DEBUG_ENABLED]))
    cg.add(
        var.set_runtime_debug_interval_ms(
            config[CONF_RUNTIME_DEBUG_INTERVAL].total_milliseconds
        )
    )
    cg.add(var.set_verbose_diagnostics(config[CONF_VERBOSE_DIAGNOSTICS]))
    cg.add(
        var.set_diagnostics_publish_interval_ms(
            config[CONF_DIAGNOSTICS_PUBLISH_INTERVAL].total_milliseconds
        )
    )

    if CONF_ACTIVE_PARENT_ID in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ACTIVE_PARENT_ID])
        cg.add(var.set_active_parent_id_text_sensor(sens))
    if CONF_STANDBY_PARENT_ID in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STANDBY_PARENT_ID])
        cg.add(var.set_standby_parent_id_text_sensor(sens))
    if CONF_FAILOVER_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_FAILOVER_COUNT])
        cg.add(var.set_failover_count_sensor(sens))
    if CONF_DEGRADED_STATE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DEGRADED_STATE])
        cg.add(var.set_degraded_state_binary_sensor(sens))
    if CONF_LAST_FAILOVER_REASON in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_FAILOVER_REASON])
        cg.add(var.set_last_failover_reason_text_sensor(sens))
