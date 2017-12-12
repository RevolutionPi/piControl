
enum revpi_power_led_mode {
	REVPI_POWER_LED_OFF = 0,
	REVPI_POWER_LED_ON = 1,
	REVPI_POWER_LED_FLICKR = 2,
	REVPI_POWER_LED_ON_500MS = 3,
	REVPI_POWER_LED_ON_1000MS = 4,
};

void revpi_led_trigger_event(uint8_t *led_prev, uint8_t led);
void revpi_power_led_red_set(enum revpi_power_led_mode mode);
void revpi_power_led_red_run(void);

int bcm2835_cpufreq_clock_property(u32 tag, u32 id, u32 * val);
uint32_t bcm2835_cpufreq_get_clock(void);
