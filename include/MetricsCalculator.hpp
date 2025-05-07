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
    void begin(DriveType type); // 初期化 (累積データロード含む)
    bool update(unsigned long currentMillis); // メトリクス更新処理
    void resetSession(); // 現在のセッションデータのみリセット
    const TrackerData& getData() const; // 計算済みデータを取得
    bool isMoving() const; // SLEEP_TIMEOUT_MS 以内か (活動中か)
    bool isTimerRunning() const; // ★ 追加: TIMER_STOP_DELAY_MS 以内か (タイマー動作中か) ★
    void saveCumulativeData(); // (現状未使用) NVSへの累積データ保存用だった名残
    unsigned long getLastPulseObservedMs() const; // 最後にパルスを観測した時刻
    void stoppingDataUpdate();

private:
    PulseCounter& pulseCounter; // パルスカウンターへの参照
    Storage& storage;           // ストレージへの参照
    TrackerData data;           // 計測データ保持用

    unsigned long lastCalcTimeMs;       // 前回計算した時刻
    unsigned long lastPulseObservedMs;  // 最後にパルスを検出した時刻
    unsigned long lastTotalPulseCount;  // 前回の計算時の累積パルス数 (リセット後からの)

    bool moving;        // SLEEP_TIMEOUT_MS 以内にパルスがあったか
    bool timer_running; // TIMER_STOP_DELAY_MS 以内にパルスがあったか
    DriveType drive_type;

    float lastValidRpm;
    float lastValidSpeedKmh;
    float lastValidMets;

    // 内部計算用メソッド
    void calculateMetrics(unsigned long intervalPulses, unsigned long intervalMs);
};

#endif // METRICS_CALCULATOR_HPP