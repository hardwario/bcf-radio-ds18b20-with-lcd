#include <application.h>

/*

 SENSOR MODULE CONNECTION
==========================

Sensor Module R1.0 - 4 pin connector
VCC, GND, - , DATA

Sensor Module R1.1 - 5 pin connector
- , GND , VCC , - , DATA


 DS18B20 sensor pinout
=======================
VCC - red
GND - black
DATA- yellow (white)

*/

// Time after the sending is less frequent to save battery
#define SERVICE_INTERVAL_INTERVAL (10 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL   (30 * 60 * 1000)

#define UPDATE_SERVICE_INTERVAL            (5 * 1000)
#define UPDATE_NORMAL_INTERVAL             (1 * 60 * 1000)

#define TEMPERATURE_DS18B20_PUB_NO_CHANGE_INTEVAL (5  * 60* 1000)
#define TEMPERATURE_DS18B20_PUB_VALUE_CHANGE 0.5f

static twr_led_t led;

float temperature_on_display = NAN;

static twr_ds18b20_t ds18b20;

int device_index;
twr_gfx_t *gfx;

struct {
    event_param_t temperature;
    event_param_t temperature_ds18b20;

} params;

void handler_ds18b20(twr_ds18b20_t *s, uint64_t device_id, twr_ds18b20_event_t e, void *p);

void climate_module_event_handler(twr_module_climate_event_t event, void *event_param);

void switch_to_normal_mode_task(void *param);

void battery_event_handler(twr_module_battery_event_t e, void *p)
{
    (void) e;
    (void) p;

    float voltage;

    if (twr_module_battery_get_voltage(&voltage))
    {
        twr_radio_pub_battery(&voltage);
    }
}

void ds18b20_event_handler(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t e, void *p)
{
    (void) p;

    temperature_on_display = NAN;

    if (e == TWR_DS18B20_EVENT_UPDATE)
    {
        float value = NAN;

        twr_ds18b20_get_temperature_celsius(self, device_address, &value);

        //twr_log_debug("UPDATE %" PRIx64 "(%d) = %f", device_address, device_index, value);

        if ((fabs(value - params.temperature_ds18b20.value) >= TEMPERATURE_DS18B20_PUB_VALUE_CHANGE) || (params.temperature_ds18b20.next_pub < twr_scheduler_get_spin_tick()))
        {
            static char topic[64];
            snprintf(topic, sizeof(topic), "thermometer/%" PRIx64 "/temperature", device_address);
            twr_radio_pub_float(topic, &value);
            params.temperature_ds18b20.value = value;
            params.temperature_ds18b20.next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_DS18B20_PUB_NO_CHANGE_INTEVAL;
        }

        temperature_on_display = value;
    }

    twr_scheduler_plan_now(0);
}

// This task is fired once after the SERVICE_INTERVAL_INTERVAL milliseconds and changes the period
// of measurement. After module power-up you get faster updates so you can test the module and see
// instant changes. After SERVICE_INTERVAL_INTERVAL the update period is longer to save batteries.
void switch_to_normal_mode_task(void *param)
{
    twr_ds18b20_set_update_interval(&ds18b20, UPDATE_NORMAL_INTERVAL);

    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void application_init(void)
{
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // For multiple sensor you can call twr_ds18b20_init() more in sdk/_excamples/ds18b20_multiple
    twr_ds18b20_init_single(&ds18b20, TWR_DS18B20_RESOLUTION_BITS_12);

    twr_ds18b20_set_event_handler(&ds18b20, ds18b20_event_handler, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, UPDATE_SERVICE_INTERVAL);

    twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    twr_radio_pairing_request("printer-temperature-monitor", FW_VERSION);

    twr_led_pulse(&led, 2000);

    twr_module_lcd_init();
    gfx = twr_module_lcd_get_gfx();

    //twr_log_init(TWR_LOG_LEVEL_DEBUG, TWR_LOG_TIMESTAMP_ABS);
    twr_scheduler_plan_from_now(0, 1000);
}

void application_task(void)
{
    if (!twr_module_lcd_is_ready())
    {
    	return;
    }

    twr_system_pll_enable();

    twr_gfx_clear(gfx);
    twr_gfx_set_font(gfx, &twr_font_ubuntu_33);
    int x = twr_gfx_printf(gfx, 20,20, 1, "%.1f   ", temperature_on_display);

    twr_module_lcd_set_font(&twr_font_ubuntu_24);
    twr_gfx_draw_string(gfx, x - 20, 25, "\xb0" "C   ", 1);

    twr_gfx_update(gfx);

    twr_system_pll_disable();
}
