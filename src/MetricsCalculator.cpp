#include "MetricsCalculator.hpp"
#include <M5Stack.h>

MetricsCalculator::MetricsCalculator(PulseCounter& pc, Storage& storage) :
    pulseCounter(pc),
    storage(storage),
    // ... (他の初期化は変更なし) ...
    moving(false),
    timer_running(false)
{}

void MetricsCalculator::begin() {
    // ★★★ SDカードから累積データを読み込むように変更 ★★★
    if (!storage.loadCumulativeDataFromSD(data)) {
        Serial.println("Failed to load cumulative data from SD on begin. Starting from zero.");
        // 読み込み失敗またはファイルなしの場合、ゼロから開始
        data.cumulativeTimeMs = 0;
        data.cumulativeDistanceKm = 0.0f;
        data.cumulativeCaloriesKcal = 0.0f;
    }
    resetSession(); // セッションデータは常にリセット
    lastCalcTimeMs = millis();
}

// resetSession は変更なし (累積データは触らない)
void MetricsCalculator::resetSession() {
    // ... (元の実装) ...
    Serial.println("Resetting session data...");
    data.sessionStartTimeMs = 0;
    data.sessionElapsedTimeMs = 0;
    data.sessionDistanceKm = 0.0f;
    data.sessionCaloriesKcal = 0.0f;
    data.currentRpm = 0.0f;
    data.currentSpeedKmh = 0.0f;
    lastTotalPulseCount = pulseCounter.getPulseCount();
    lastPulseObservedMs = 0;
    moving = false;
    timer_running = false;
}


void MetricsCalculator::update(unsigned long currentMillis) {
    unsigned long currentPulseTotal = pulseCounter.getPulseCount();
    unsigned long currentLastPulseTime = pulseCounter.getLastPulseTime();
    bool hadRecentPulse = (currentLastPulseTime > 0 && currentLastPulseTime > lastPulseObservedMs);

    if (hadRecentPulse) {
        lastPulseObservedMs = currentLastPulseTime;
        if (!moving) {
            moving = true;
            timer_running = true;
            data.sessionStartTimeMs = currentMillis;
            data.sessionElapsedTimeMs = 0;
            Serial.println("Movement started.");
            M5.Speaker.tone(440, 100);
        } else {
            timer_running = true;
        }
    } else {
        if (moving) {
            if (timer_running && lastPulseObservedMs > 0 && (currentMillis - lastPulseObservedMs > TIMER_STOP_DELAY_MS)) {
                timer_running = false;
                Serial.println("Timer stopped (3s inactivity).");
                // ★★★ タイマー停止時に最新データをSDに保存 ★★★
                storage.saveLatestDataToSD(data);
            }
            if (lastPulseObservedMs > 0 && (currentMillis - lastPulseObservedMs > SLEEP_TIMEOUT_MS)) {
                moving = false;
                timer_running = false;
                Serial.println("Movement stopped (Sleep timeout).");
                data.currentRpm = 0.0f;
                data.currentSpeedKmh = 0.0f;
                if(data.sessionElapsedTimeMs > 0){
                    data.cumulativeTimeMs += data.sessionElapsedTimeMs;
                    Serial.printf("Adding session time %lu ms to cumulative %llu ms\n", data.sessionElapsedTimeMs, data.cumulativeTimeMs);
                }
                // ★★★ NVSへの保存呼び出しは削除 ★★★
                // saveCumulativeData(); // NVS保存は不要に
                // ★★★ 代わりに最新データをSDに保存（タイマー停止時と重複するが念のため）★★★
                storage.saveLatestDataToSD(data);
                data.sessionStartTimeMs = 0;
            }
        }
    }

    // メトリクス計算 (変更なし)
    if (currentMillis - lastCalcTimeMs >= METRICS_CALC_INTERVAL_MS) {
        // ... (計算ロジックは変更なし) ...
         unsigned long intervalMs = currentMillis - lastCalcTimeMs;
         unsigned long intervalPulses = 0;
         if (currentPulseTotal >= lastTotalPulseCount) {
              intervalPulses = currentPulseTotal - lastTotalPulseCount;
         }
         if (timer_running || intervalPulses > 0) {
              calculateMetrics(intervalPulses, intervalMs);
         } else if (moving) {
              data.currentRpm = 0.0f;
              data.currentSpeedKmh = 0.0f;
         }
         if (timer_running && data.sessionStartTimeMs > 0) {
             data.sessionElapsedTimeMs += intervalMs;
         }
         lastTotalPulseCount = currentPulseTotal;
         lastCalcTimeMs = currentMillis;
    }
}

// calculateMetrics は変更なし (モデルA1を使用)
void MetricsCalculator::calculateMetrics(unsigned long intervalPulses, unsigned long intervalMs) {
     // ... (元の実装 - RPM, Speed, Distance, Calories(Model A1) の計算) ...
      if (intervalMs == 0 && intervalPulses == 0) return;
     if (intervalMs > 0) {
          double intervalSeconds = (double)intervalMs / 1000.0;
          double revolutions = (double)intervalPulses;
          data.currentRpm = (float)(revolutions / intervalSeconds * 60.0);
          if (data.currentRpm > 1000) data.currentRpm = 0;
     } else { data.currentRpm = 0.0f; }
     data.currentSpeedKmh = data.currentRpm / 60.0f * DISTANCE_PER_REV_M * 3.6f;
     double intervalRevolutions = (double)intervalPulses;
     double intervalDistanceKm = intervalRevolutions * DISTANCE_PER_REV_M / 1000.0;
     data.sessionDistanceKm += (float)intervalDistanceKm;
     data.cumulativeDistanceKm += (float)intervalDistanceKm;
     if (intervalMs > 0) {
         double intervalSeconds = (double)intervalMs / 1000.0;
         double calorieRate = CALORIES_RPM_K1_FACTOR * data.currentRpm;
         if (calorieRate < 0.0) calorieRate = 0.0;
         double intervalCalories = calorieRate * intervalSeconds;
         data.sessionCaloriesKcal += (float)intervalCalories;
         data.cumulativeCaloriesKcal += (float)intervalCalories;
     }
}

// getData, isMoving, getLastPulseObservedMs は変更なし
const TrackerData& MetricsCalculator::getData() const { return data; }
bool MetricsCalculator::isMoving() const { return moving; }
unsigned long MetricsCalculator::getLastPulseObservedMs() const { return lastPulseObservedMs; }

// saveCumulativeData() は NVS 保存用だったので、ここでは使わない
// もし外部からSD保存をトリガーしたい場合は別途メソッドを用意するか、
// Storageクラスのメソッドを直接呼ぶ
void MetricsCalculator::saveCumulativeData() {
     // このメソッドはNVS用だったので、SDカード保存とは役割が異なる
     // 必要に応じて saveLatestDataToSD を呼ぶように変更するか、このメソッド自体を削除する
     Serial.println("[MetricsCalculator] saveCumulativeData called - Deprecated (use saveLatestDataToSD or appendHistoryDataToSD)");
     // storage.saveLatestDataToSD(data); // 必要ならここで呼ぶ
}