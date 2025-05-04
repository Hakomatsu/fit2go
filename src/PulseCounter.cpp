#include "PulseCounter.hpp"
#include <M5Stack.h>
#include "esp_log.h"
#include "esp_err.h"
#include "soc/pcnt_struct.h" // ★ PCNTレジスタ定義ヘッダー (int_clrアクセス用)
#include "driver/gpio.h"

// staticメンバー変数の実体定義と初期化
volatile unsigned long PulseCounter::pulseCountSoftware = 0;
volatile unsigned long PulseCounter::lastPulseTimestamp = 0;
volatile bool PulseCounter::led_state = false;

static const char *TAG_PCNT = "PulseCounter"; // ログ用タグ

PulseCounter::PulseCounter(int pulse_pin) :
    pulsePin(pulse_pin),
    pcntUnit(PCNT_UNIT),
    pcntChannel(PCNT_CHANNEL)
{}

void IRAM_ATTR PulseCounter::pcnt_intr_handler(void *arg) {
    uint32_t status = 0;
    pcnt_get_event_status(PCNT_UNIT, &status);
    led_state = !led_state;
    gpio_set_level((gpio_num_t)DEBUG_LED_PIN, led_state);

    if (status & PCNT_EVT_THRES_1) {
        pulseCountSoftware++;
        lastPulseTimestamp = millis();
    }
    PCNT.int_clr.val = (1 << PCNT_UNIT);
}

bool PulseCounter::begin() {
    ESP_LOGI(TAG_PCNT, "Initializing PCNT for GPIO %d", pulsePin);
    pcnt_isr_service_uninstall();
    ESP_LOGI(TAG_PCNT, "Attempted to uninstall existing ISR service.");

    // --- GPIO設定 ---
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pulsePin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // ★ 内部プルアップ有効化 (外部推奨)
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
         ESP_LOGE(TAG_PCNT, "GPIO config failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG_PCNT, "GPIO %d configured as INPUT PULLUP", pulsePin);

    gpio_config_t debug_led_io_conf;
    debug_led_io_conf.intr_type = GPIO_INTR_DISABLE;
    debug_led_io_conf.mode = GPIO_MODE_OUTPUT;
    debug_led_io_conf.pin_bit_mask = (1ULL << DEBUG_LED_PIN);
    debug_led_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    debug_led_io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    err = gpio_config(&debug_led_io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PCNT, "Debug LED GPIO config failed: %s", esp_err_to_name(err));
        // Continue even if LED fails
    } else {
        ESP_LOGI(TAG_PCNT, "Debug LED GPIO %d configured as OUTPUT", DEBUG_LED_PIN);
        gpio_set_level((gpio_num_t)DEBUG_LED_PIN, 0); // 初期状態はOFF
    }

    // --- PCNT設定 ---
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = pulsePin,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .pos_mode = PCNT_COUNT_INC,       // 立ち上がりカウントアップ
        .neg_mode = PCNT_COUNT_DIS,       // 立ち下がり無視
        .counter_h_lim = 1,
        .counter_l_lim = 0,
        .unit = pcntUnit,
        .channel = pcntChannel,
    };
    err = pcnt_unit_config(&pcnt_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PCNT, "PCNT unit config failed: %s", esp_err_to_name(err));
        return false;
    }

    // --- PCNTフィルタ設定 ---
    pcnt_set_filter_value(pcntUnit, PCNT_FILTER_VALUE);
    pcnt_filter_enable(pcntUnit);
    ESP_LOGI(TAG_PCNT, "PCNT filter enabled with value %d", PCNT_FILTER_VALUE);

    // --- PCNT割り込み設定 ---
    pcnt_counter_pause(pcntUnit);
    pcnt_counter_clear(pcntUnit);

    // ISRサービスをインストール (まだなら)
    err = pcnt_isr_service_install(0);
     if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_PCNT, "PCNT ISR service install failed: %s", esp_err_to_name(err));
        return false;
    } else {
        ESP_LOGI(TAG_PCNT, "PCNT ISR service installed or already present.");
    }

    // 割り込みイベント (しきい値1到達)
    pcnt_set_event_value(pcntUnit, PCNT_EVT_THRES_1, PCNT_EVENT_THRESHOLD);
    pcnt_event_enable(pcntUnit, PCNT_EVT_THRES_1);
    ESP_LOGI(TAG_PCNT, "PCNT Threshold 1 event enabled at count %d", PCNT_EVENT_THRESHOLD);

    // PCNTユニットの割り込み機能を有効化
    pcnt_intr_enable(pcntUnit);
    ESP_LOGI(TAG_PCNT, "PCNT interrupt capability enabled.");

    // ISRハンドラをPCNTユニットに登録
    // ★★★ 引数を修正 (最後のハンドルポインタを削除) ★★★
    pcnt_isr_handler_remove(pcntUnit);
    ESP_LOGI(TAG_PCNT, "Attempted to remove existing ISR handler.");
    err = pcnt_isr_handler_add(pcntUnit, PulseCounter::pcnt_intr_handler, NULL);
     if (err != ESP_OK) {
        ESP_LOGE(TAG_PCNT, "PCNT ISR handler add failed: %s", esp_err_to_name(err));
        // 失敗したらサービスもアンインストールした方が良いかも
        pcnt_isr_service_uninstall();
        return false;
    }
    ESP_LOGI(TAG_PCNT, "PCNT ISR handler added successfully.");

    pcnt_counter_resume(pcntUnit); // カウント再開
    ESP_LOGI(TAG_PCNT, "PCNT setup complete.");
    return true;
}

unsigned long PulseCounter::getPulseCount() {
    noInterrupts();
    unsigned long count = pulseCountSoftware;
    interrupts();
    return count;
}

unsigned long PulseCounter::getLastPulseTime() {
    noInterrupts();
    unsigned long timestamp = lastPulseTimestamp;
    interrupts();
    return timestamp;
}

void PulseCounter::resetPulseCount() {
    noInterrupts();
    pulseCountSoftware = 0;
    lastPulseTimestamp = 0;
    interrupts();
    /* pcnt_counter_pause(pcntUnit);
    pcnt_counter_clear(pcntUnit);
    pcnt_counter_resume(pcntUnit); */
    ESP_LOGI(TAG_PCNT, "Software and Hardware pulse counters reset.");
}