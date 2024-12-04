// DMX512 master

#pragma once
#include "fraise.h"
#include "hardware/dma.h"
#include "hardware/uart.h"

class DmxMaster {
  private:
    uart_inst_t *uart;
    int tx_pin;
    int dma_chan = -1;
    dma_channel_config dma_config;
    enum {IDLE, BREAK, MAB, SENDING} state = IDLE;

  public:
    DmxMaster(uart_inst_t *uart_inst, int tx_pin_num);
    void init();
    bool transfer_frame(const char *frame, int count);
    bool transfer_finished();
    void set_break(bool on);
    
    void alarm();
    static int64_t alarm_callback(alarm_id_t id, void *user_data);
};
