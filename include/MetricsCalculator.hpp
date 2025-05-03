#ifndef METRICS_CALCULATOR_HPP
#define METRICS_CALCULATOR_HPP

#include <Arduino.h>
#include "config.hpp"
#include "TrackerData.hpp"
#include "PulseCounter.hpp"
#include "Storage.hpp"

class MetricsCalculator {
public:
    MetricsCalculator(PulseCounter& pc, Storage& storage);
    void begin();
    void update(unsigned long currentMillis);
    void resetSession();
    const TrackerData& getData() const;
    bool isMoving() const; // セッションがアクティブか (SLEEP_TIMEOUT以内か)
    void saveCumulativeData();
    // ★★★ 最後にパルスを観測した時刻を取得するメソッド ★★★
    unsigned long getLastPulseObservedMs() const;

private:
    PulseCounter& pulseCounter;
    Storage& storage;
    TrackerData data;

    unsigned long lastCalcTimeMs;
    unsigned long lastPulseObservedMs; // 最後にパルスを検出した時刻
    unsigned long lastTotalPulseCount;

    bool moving;        // SLEEP_TIMEOUT_MS 以内かどうか
    // ★★★ タイマー動作状態を追加 ★★★
    bool timer_running; // セッションタイマーが動作中か

    void calculateMetrics(unsigned long intervalPulses, unsigned long intervalMs);
};

#endif // METRICS_CALCULATOR_HPP