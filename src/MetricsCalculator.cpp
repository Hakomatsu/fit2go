#include "MetricsCalculator.hpp"
#include <M5Stack.h>

MetricsCalculator::MetricsCalculator(PulseCounter& pc, Storage& storage) :
    pulseCounter(pc),
    storage(storage),
    // データメンバーは TrackerData 構造体のデフォルト値で初期化される
    lastCalcTimeMs(0),
    lastPulseObservedMs(0),
    lastTotalPulseCount(0),
    moving(false),
    timer_running(false),
    lastValidRpm(0.0f),
    lastValidSpeedKmh(0.0f),
    lastValidMets(1.0f)
{}

void MetricsCalculator::begin(DriveType type) {
    drive_type = type;

    // SDカードから累積データを読み込む
    if (!storage.loadCumulativeDataFromSD(data)) {
        Serial.println("Failed to load cumulative data from SD on begin. Starting from zero.");
        // 読み込み失敗またはファイルなしの場合、ゼロから開始 (data構造体のデフォルト値)
        data.cumulativeTimeMs = 0;
        data.cumulativeDistanceKm = 0.0f;
        data.cumulativeCaloriesKcal = 0.0f;
    }
    resetSession(); // セッションデータはリセット
    lastCalcTimeMs = millis(); // 初回計算時刻の基準
}

// セッションデータのみをリセットする
void MetricsCalculator::resetSession() {
    Serial.println("Resetting session data...");
    data.sessionStartTimeMs = 0;
    data.sessionElapsedTimeMs = 0;
    data.sessionDistanceKm = 0.0f;
    data.sessionCaloriesKcal = 0.0f;
    data.currentRpm = 0.0f;
    data.currentSpeedKmh = 0.0f;
    data.sessionPulseCount = 0; // セッションパルスもリセット
    data.currentMets = 1.0f; // METs初期値

    // ★ セッションリセット時の「前回の」パルスカウントを記録する ★
    //   PulseCounter自体のカウントはリセットしない
    lastTotalPulseCount = pulseCounter.getPulseCount();

    // ★★★ pulseCounter.resetPulseCount(); を削除した状態 ★★★

    lastPulseObservedMs = 0; // 最後に観測した時刻もリセット
    moving = false;          // 移動状態フラグもリセット
    timer_running = false;   // タイマー状態フラグもリセット
}


bool MetricsCalculator::update(unsigned long currentMillis) {
    // 最新のパルスカウントと最終パルス時刻を取得
    unsigned long currentPulseTotal = pulseCounter.getPulseCount();
    unsigned long currentLastPulseTime = pulseCounter.getLastPulseTime();

    // 動き出し判定
    bool hadRecentPulse = false;
    if (lastPulseObservedMs == 0) { // まだ一度も観測していない or リセット直後
        if (currentLastPulseTime > 0) { 
            hadRecentPulse = true; 
            Serial.println("First pulse detected after reset/idle.");
        }
    } else { // すでに観測履歴がある場合
        if (currentLastPulseTime > lastPulseObservedMs) { 
            hadRecentPulse = true; 
        }
    }

    if (hadRecentPulse) {
        lastPulseObservedMs = currentLastPulseTime; // 最終観測時刻を更新
        if (!moving) {
            // 完全に停止していた状態から動き出した場合
            moving = true;        // 移動中フラグON
            timer_running = true; // タイマー動作中フラグON
            if (data.sessionStartTimeMs == 0) { // 完全な新規セッション開始
                 data.sessionStartTimeMs = currentMillis; // セッション開始時刻
                 data.sessionElapsedTimeMs = 0;          // 経過時間はリセット
                 data.sessionDistanceKm = 0.0f;          // セッション距離リセット
                 data.sessionCaloriesKcal = 0.0f;        // セッションカロリーリセット
                 data.sessionPulseCount = 0;             // セッションパルスカウントリセット
                 // ★ 新セッション開始時の前回のカウントは現在の値を使う ★
                 lastTotalPulseCount = currentPulseTotal;
            }
            Serial.println("Movement started / resumed.");
            M5.Speaker.tone(440, 100); // 開始音
        } else {
            // すでに移動中の場合（タイマーが止まっていた場合も含む）
            timer_running = true; // タイマー動作中にする
        }
        // セッション中のパルスカウントを更新 (現在のトータル - 開始時のトータル = セッション中のカウント)
        // ただし、表示用なので、単純に currentPulseTotal - 開始時カウント でも良いかもしれない
        // ここでは resetSession で 0 にリセットされる PulseCounter の値をそのまま使う
        data.sessionPulseCount = currentPulseTotal;

    } else { // 最近のパルスがない場合
        if (moving) { // 直前まで動いていた場合 (moving==true)
            // タイマー停止判定 (TIMER_STOP_DELAY_MS: 3秒)
            if (timer_running && lastPulseObservedMs > 0 && (currentMillis - lastPulseObservedMs > TIMER_STOP_DELAY_MS)) {
                if (timer_running) {
                    timer_running = false;
                    Serial.println("Timer stopped (3s inactivity).");
                    storage.saveLatestDataToSD(data);
                }
            }
            // 移動停止判定（スリープタイムアウト） (SLEEP_TIMEOUT_MS: 63秒)
            if (lastPulseObservedMs > 0 && (currentMillis - lastPulseObservedMs > SLEEP_TIMEOUT_MS)) {
                if (moving) {
                    moving = false;
                    timer_running = false;
                    Serial.println("Movement stopped (Sleep timeout).");
                    data.currentRpm = 0.0f;
                    data.currentSpeedKmh = 0.0f;
                    storage.saveLatestDataToSD(data); // 最新の累積データ（時間含む）を保存
                    data.sessionStartTimeMs = 0; // 次回の新規セッション判定のため
                }
            }
        }
    }

    // --- メトリクス計算 ---
    bool calc_metrics = false;
    if (drive_type == DriveType::TIMER_DRIVEN && currentMillis - lastCalcTimeMs >= METRICS_CALC_INTERVAL_MS){
        calc_metrics = true;
    }
    else if (drive_type == DriveType::EVENT_DRIVEN && currentPulseTotal > lastTotalPulseCount){ // 改善対象; もっといい方法があると思う.
            calc_metrics = true;
    }
    if (calc_metrics){
        unsigned long intervalMs = currentMillis - lastCalcTimeMs;
        unsigned long intervalPulses = 0;
        // 差分を計算 (現在のカウント - 前回の計算時のカウント)
        if (currentPulseTotal >= lastTotalPulseCount) {
            intervalPulses = currentPulseTotal - lastTotalPulseCount;
        } else {
            // カウンタが一周した or リセットされた場合などは差分が負になる
            // 本来は一周を考慮すべきだが、ここでは無視して0とする
            if (currentPulseTotal != 0) {
                Serial.printf("Warning: Pulse count decreased? C:%lu L:%lu\n", currentPulseTotal, lastTotalPulseCount);
            }
            intervalPulses = 0;
        }

        // 計算実行条件
        if (timer_running || intervalPulses > 0) {
            calculateMetrics(intervalPulses, intervalMs); // RPM, Speed, Dist(S/C), Cal(S/C) 更新
            // ★★★ タイマー動作中にセッション時間と累積時間の両方を加算 ★★★
            if (timer_running && data.sessionStartTimeMs > 0) {
                data.sessionElapsedTimeMs += intervalMs;
                data.cumulativeTimeMs += intervalMs; // 累積時間もここで加算！
            }
        } else if (moving) { // STOPPING 状態
            data.currentRpm = 0.0f;
            data.currentSpeedKmh = 0.0f;
        } else { // IDLE 状態
            data.currentRpm = 0.0f;
            data.currentSpeedKmh = 0.0f;
        }

        // ★ 次回計算のために今回のカウントを保存 ★
        lastTotalPulseCount = currentPulseTotal;
        lastCalcTimeMs = currentMillis;
        
        calc_metrics = false;
        // Serial.printf("Data updated!\n");
        return true;
    }

    return false;
}

// calculateMetrics
void MetricsCalculator::calculateMetrics(unsigned long intervalPulses, unsigned long intervalMs) {
     if (intervalMs == 0 && intervalPulses == 0)
        return; // 何もなければ計算しない

     float currentRpm = 0.0;
     // RPM calculation
     if (intervalMs > 0) {
          double intervalSeconds = (double)intervalMs / 1000.0;
          double revolutions = (double)intervalPulses / PULSES_PER_REVOLUTION; // 1パルス=1回転と仮定
          currentRpm = (float)(revolutions / intervalSeconds * 60.0); 
          data.currentRpm = currentRpm;
          if (currentRpm > 300 || currentRpm < 0){
            Serial.println("[MetricsCalc] Use last valid value.");
            data.currentRpm = lastValidRpm; // 異常値補正
          }
     } else {
        data.currentRpm = lastValidRpm;
    }

     // Speed calculation
     data.currentSpeedKmh = data.currentRpm / 60.0f * DISTANCE_PER_REV_M * 3.6f;

     // Distance calculation
     double intervalRevolutions = (double)intervalPulses / PULSES_PER_REVOLUTION;
     double intervalDistanceKm = intervalRevolutions * DISTANCE_PER_REV_M / 1000.0;
     data.sessionDistanceKm += (float)intervalDistanceKm;
     data.cumulativeDistanceKm += (float)intervalDistanceKm; // ★ 累積距離はここで加算 ★

     // Calorie calculation
     if (intervalMs > 0) {
         double intervalSeconds = (double)intervalMs / 1000.0;
         double calorieRate = CALORIES_RPM_K1_FACTOR * data.currentRpm; // カロリー/秒
         if (calorieRate < 0.0)
            calorieRate = 0.0; // マイナスにならないように
         double intervalCalories = calorieRate * intervalSeconds;
         data.sessionCaloriesKcal += (float)intervalCalories;
         data.cumulativeCaloriesKcal += (float)intervalCalories; // ★ 累積カロリーはここで加算 ★
     }
     // METs calculation
     if (data.currentSpeedKmh < 1)
        data.currentMets = 1.0f;
     else if (data.currentSpeedKmh < 10)
        data.currentMets = 3.0f;
     else if (data.currentSpeedKmh < 16)
        data.currentMets = 4.0f;
     else
        data.currentMets = 6.0f;
    
     lastValidRpm = data.currentRpm;
     lastValidSpeedKmh = data.currentSpeedKmh;
     lastValidMets = data.currentMets;
}

// getData
const TrackerData& MetricsCalculator::getData() const {
    return data;
}

// isMoving
bool MetricsCalculator::isMoving() const {
    return moving;
}

// getLastPulseObservedMs
unsigned long MetricsCalculator::getLastPulseObservedMs() const {
    return lastPulseObservedMs;
}

// isTimerRunning
bool MetricsCalculator::isTimerRunning() const {
    return timer_running;
}

// saveCumulativeData (現状未使用)
void MetricsCalculator::saveCumulativeData() {
     Serial.println("[MetricsCalculator] saveCumulativeData called - Deprecated (Data is saved to SD)");
}

void MetricsCalculator::stoppingDataUpdate(){
    data.currentRpm = 0.0f;
    data.currentSpeedKmh = 0.0f;
}