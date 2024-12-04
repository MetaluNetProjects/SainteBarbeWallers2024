// DMX512 master

#include "dmx.h"

DmxMaster::DmxMaster(uart_inst_t *uart_inst, int tx_pin_num) {
    uart = uart_inst;
    tx_pin = tx_pin_num;
}

void DmxMaster::init() {
    // Set the GPIO pin mux to the UART - pin 0 is TX, 1 is RX; note use of UART_FUNCSEL_NUM for the general
    // case where the func sel used for UART depends on the pin number    gpio_set_function(0, UART_FUNCSEL_NUM(uart, 0));
    gpio_set_function(tx_pin, GPIO_FUNC_UART /*UART_FUNCSEL_NUM(uart, 1)*/);
    uart_init(uart, 250000);
    uart_set_format (uart, /*data_bits*/ 8, /*stop_bits*/ 2, /*parity*/ UART_PARITY_NONE);

    if(dma_chan != -1) return; // already inited
    dma_chan = dma_claim_unused_channel(true);
    dma_config = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, uart_get_dreq(uart, /*is_tx*/ true));
    dma_channel_set_config(dma_chan, &dma_config, /*trigger*/ false);
    dma_channel_set_write_addr(dma_chan, &uart_get_hw(uart)->dr, false);
}

void DmxMaster::alarm() {
    if(state == BREAK) {
        set_break(false);
        state = MAB;
        add_alarm_in_us(12, alarm_callback, this, false);
    } else {
        //sleep_us(10);
        dma_channel_start(dma_chan);
        state = SENDING;
    }
}

int64_t DmxMaster::alarm_callback(alarm_id_t id, void *user_data) {
    ((DmxMaster*)user_data)->alarm();
    return 0;
}

bool DmxMaster::transfer_frame(const char *frame, int count) {
    //dma_channel_transfer_from_buffer_now(dma_chan, frame, count);
    dma_channel_set_read_addr(dma_chan, frame, false);
    dma_channel_set_trans_count(dma_chan, count, false);
    state = BREAK;
    set_break(true);
    add_alarm_in_us(100, alarm_callback, this, false);
    return true;
}

bool DmxMaster::transfer_finished() {
    if((state == SENDING) && (!dma_channel_is_busy(dma_chan)) && (! (uart_get_hw(uart)->fr & UART_UARTFR_BUSY_BITS))) {
        state = IDLE;
    }
    return state == IDLE;
}

void DmxMaster::set_break(bool on) {
    uart_set_break(uart, on);
}

