/**
 * Simple blinking fruit
 */

#define BOARD pico
#include "fraise.h"
#include "dmx.h"
#include "lamp.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
const int DMX_PIN = 4;
const int DMX_DRV_PIN = 5;
uart_inst_t *DMX_UART = uart1;

int ledPeriod = 250;

#define DMX_CHAN_COUNT 256
char dmxBuf0[DMX_CHAN_COUNT];
char dmxBuf1[DMX_CHAN_COUNT];
char *dmxBufs[2] = {dmxBuf0, dmxBuf1};
int dmxBuf_current_index = 0; // which dmxBuf we're currently writting to.
#define dmxBuf_now (dmxBufs[dmxBuf_current_index])
#define dmxBuf_old (dmxBufs[1 - dmxBuf_current_index])

absolute_time_t fill_frame_start_time;
int fill_frame_duration_us;
int frame_duration_us;
float cpuload = 0; // %

DmxMaster dmx(DMX_UART, DMX_PIN);

void fill_frame() {
    fill_frame_start_time = get_absolute_time();
    // ---- fill frame using dmxBuf_now
    Lamp::compute(dmxBuf_now);
    fill_frame_duration_us = absolute_time_diff_us(fill_frame_start_time, get_absolute_time());
}

void setup() {
    dmx.init();
    gpio_init(DMX_DRV_PIN);
    gpio_set_dir(DMX_DRV_PIN, GPIO_OUT);
    gpio_put(DMX_DRV_PIN, 0);

    Lamp::set_period_ms((DMX_CHAN_COUNT * 11) / 250);

    // RGBA: 18 rampes: 0 - 17 (DMX 1 - 72)
    int chan = 1;
    int id = 0;
    for(int i = 0; i < 18; i++) {
        new LampFire(id++, chan, 4);
        chan += 4;
    }

    chan = 80;
    id = 30;
    // 20 single: 30 - 49 (DMX 80 - 99)
    for(int i = 0; i < 20; i++) {
        new LampFire(id++, chan, 1);
        chan += 1;
    }

    chan = 120;
    id = 80;
    // RGBA: 10 parDMX: 80 - 89 (DMX 120 - 159)
    for(int i = 0; i < 10; i++) {
        new LampFire(id++, chan, 4);
        chan += 4;
    }

    chan = 180;
    id = 100;
    // RGB: 10 parPixel: 100 - 109 (DMX 180 - 209)
    for(int i = 0; i < 10; i++) {
        new LampFire(id++, chan, 3);
        chan += 3;
    }

    fill_frame();
}

void loop(){
	static absolute_time_t nextLed;
	static bool led = false;

    if(dmx.transfer_finished()) {
        frame_duration_us = absolute_time_diff_us(fill_frame_start_time, get_absolute_time());
        dmx.transfer_frame(dmxBuf_now, DMX_CHAN_COUNT);
        float load = (100.0 * fill_frame_duration_us) / (float(frame_duration_us));
        cpuload += (load - cpuload) * 0.1;
        dmxBuf_current_index = 1 - dmxBuf_current_index;
        fill_frame();
    }

	if(time_reached(nextLed)) {
		gpio_put(LED_PIN, led = !led);
		nextLed = make_timeout_time_ms(ledPeriod);
	}
}

void fraise_receivebytes(const char *data, uint8_t len){
	if(data[0] == 1) ledPeriod = (int)data[1] * 10;
	else if(data[0] == 100 && len > 1) Lamp::command(data + 1, len - 1);
	else if(data[0] == 101 && len > 1) Lamp::config(data + 1, len - 1);
	/*else {
		printf("rcvd ");
		for(int i = 0; i < len; i++) printf("%d ", (uint8_t)data[i]);
		putchar('\n');
	}*/
}

void fraise_receivechars(const char *data, uint8_t len){
	if(data[0] == 'E') { // Echo
		printf("E%s\n", data + 1);
	}
	else if(data[0] == 'L') { // get load
		printf("L %f\n", cpuload);
	}
}

