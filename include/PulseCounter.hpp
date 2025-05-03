#ifndef PULSE_COUNTER_HPP
#define PULSE_COUNTER_HPP

#include "driver/pcnt.h"
#include "driver/gpio.h"
#include "config.hpp"

class PulseCounter {
public:
    PulseCounter(int pulse_pin);
    bool begin();
    // ソフトウェアで保持している累積カウント数を返す
    unsigned long getPulseCount();
    // 最後にパルスを検出した時刻(ms)を返す
    unsigned long getLastPulseTime();
    // ソフトウェアカウントをリセット (セッション開始時など)
    void resetPulseCount();

private:
    int pulsePin;
    pcnt_unit_t pcntUnit;
    pcnt_channel_t pcntChannel;

    // ISRからアクセスされるためstatic volatile
    static volatile unsigned long pulseCountSoftware;
    static volatile unsigned long lastPulseTimestamp;
    static volatile bool led_state;

    // ISR本体 (static)
    static void IRAM_ATTR pcnt_intr_handler(void *arg);
};

#endif // PULSE_COUNTER_HPP