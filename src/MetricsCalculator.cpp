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
    timer_running(false)
{}

void MetricsCalculator::begin() {
    // SDカードから累積データを読み込む
    if (!storage.loadCumulativeDataFromSD(data)) {
        Serial.println("Failed to load cumulative data from SD on begin. Starting from zero.");
        // 読み込み失敗またはファイルなしの場合、ゼロから開始 (data構造体のデフォルト値)
        data.cumulativeTimeMs = 0;
        data.cumulativeDistanceKm = 0.0f;
        data.cumulativeCaloriesKcal = 0.0f;
    }
    resetSession(); // セッションデータは常にリセット
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

    // ★ セッションリセット時の現在パルス数を次の計算の基準にする ★
    //   PulseCounter自体もリセットするので、基準は0で良い
    lastTotalPulseCount = 0;
    // ★ pulseCounter自体のカウントもリセットする ★
    pulseCounter.resetPulseCount(); // SWカウントとTimestampをリセット

    lastPulseObservedMs = 0; // 最後に観測した時刻もリセット
    moving = false;          // 移動状態フラグもリセット
    timer_running = false;   // タイマー状態フラグもリセット
}


void MetricsCalculator::update(unsigned long currentMillis) {
    // 最新のパルスカウントと最終パルス時刻を取得
    unsigned long currentPulseTotal = pulseCounter.getPulseCount(); // これはリセット後からの累積カウント
    unsigned long currentLastPulseTime = pulseCounter.getLastPulseTime();

    // 動き出し判定
    bool hadRecentPulse = false;
    if (lastPulseObservedMs == 0) { // まだ一度も観測していない or リセット直後
        // lastPulseTimestamp が 0 でなくなった瞬間を検出
        if (currentLastPulseTime > 0) {
            hadRecentPulse = true;
            Serial.println("First pulse detected after reset/idle.");
        }
    } else { // すでに観測履歴がある場合
        // 以前と同じく、観測時刻が更新されたかチェック
        if (currentLastPulseTime > lastPulseObservedMs) {
            hadRecentPulse = true;
        }
    }


    if (hadRecentPulse) {
        // 新しいパルスがあった場合
        lastPulseObservedMs = currentLastPulseTime; // 最終観測時刻を更新
        if (!moving) {
            // 完全に停止していた状態から動き出した場合
            moving = true;        // 移動中フラグON
            timer_running = true; // タイマー動作中フラグON
            // 完全な新規セッション開始時のみ開始時刻などを記録
            if (data.sessionStartTimeMs == 0) {
                 data.sessionStartTimeMs = currentMillis; // セッション開始時刻
                 data.sessionElapsedTimeMs = 0;          // 経過時間はリセット
                 data.sessionDistanceKm = 0.0f;          // セッション距離リセット
                 data.sessionCaloriesKcal = 0.0f;        // セッションカロリーリセット
                 data.sessionPulseCount = 0;             // セッションパルスカウントリセット
                 // ★ 新セッション開始時の前回のカウントは 0 とする ★
                 lastTotalPulseCount = 0;
            }
            Serial.println("Movement started / resumed.");
            M5.Speaker.tone(440, 100); // 開始音
        } else {
            // すでに移動中の場合（タイマーが止まっていた場合も含む）
            timer_running = true; // タイマー動作中にする
        }
        // ★ セッション中のパルスカウントは、リセット後からの現在の合計カウントとする ★
        data.sessionPulseCount = currentPulseTotal;

    } else { // 最近のパルスがない場合
        if (moving) { // 直前まで動いていた場合 (moving==true)
            // タイマー停止判定 (TIMER_STOP_DELAY_MS: 3秒)
            if (timer_running && lastPulseObservedMs > 0 && (currentMillis - lastPulseObservedMs > TIMER_STOP_DELAY_MS)) {
                if (timer_running) { // timer_running が true から false に変わる瞬間のみ実行
                    timer_running = false; // ★ タイマー停止 ★
                    Serial.println("Timer stopped (3s inactivity).");
                    storage.saveLatestDataToSD(data); // タイマー停止時に最新累積データをSDに保存
                }
            }
            // 移動停止判定（スリープタイムアウト） (SLEEP_TIMEOUT_MS: 63秒)
            if (lastPulseObservedMs > 0 && (currentMillis - lastPulseObservedMs > SLEEP_TIMEOUT_MS)) {
                if (moving) { // moving が true から false に変わる瞬間のみ実行
                    moving = false; // ★ 移動停止 ★
                    timer_running = false; // 念のためタイマーも停止
                    Serial.println("Movement stopped (Sleep timeout).");
                    data.currentRpm = 0.0f; // 瞬間値も0に
                    data.currentSpeedKmh = 0.0f;
                    storage.saveLatestDataToSD(data); // 移動停止時にも保存
                    // セッション開始時刻をリセットして、次回の開始判定に備える
                    data.sessionStartTimeMs = 0;
                }
            }
        }
    }

    // --- メトリクス計算 (METRICS_CALC_INTERVAL_MS ごと) ---
    if (currentMillis - lastCalcTimeMs >= METRICS_CALC_INTERVAL_MS) {
         unsigned long intervalMs = currentMillis - lastCalcTimeMs;
         unsigned long intervalPulses = 0;
         // 差分を計算 (リセット後からの累積カウントを使う)
         if (currentPulseTotal >= lastTotalPulseCount) {
              intervalPulses = currentPulseTotal - lastTotalPulseCount;
         } else {
             // カウンタが減少することは通常ないはず (リセット時を除く)
             if (currentPulseTotal != 0) { // 0でないならログ表示
                  Serial.printf("Warning: Pulse count decreased? Current: %lu, Last: %lu\n", currentPulseTotal, lastTotalPulseCount);
             }
             intervalPulses = 0; // 安全のため0にする
         }


         // 計算実行条件
         // タイマー動作中、またはインターバル中にパルスがあった場合に計算
         if (timer_running || intervalPulses > 0) {
              calculateMetrics(intervalPulses, intervalMs); // RPM, Speed, Dist, Calorie 更新
              // タイマー動作中のみセッション時間を加算
              if (timer_running && data.sessionStartTimeMs > 0) {
                  data.sessionElapsedTimeMs += intervalMs;
              }
         } else if (moving) { // タイマーは止まったが、まだ移動中(Stopping)の場合
              // RPMと速度はゼロにする
              data.currentRpm = 0.0f;
              data.currentSpeedKmh = 0.0f;
         } else { // 完全に停止している場合 (IDLE)
              data.currentRpm = 0.0f;
              data.currentSpeedKmh = 0.0f;
         }

         // ★ 次回計算のために今回のカウントを保存 ★
         lastTotalPulseCount = currentPulseTotal;
         lastCalcTimeMs = currentMillis;
    }
}

// calculateMetrics : 内部計算メソッド
void MetricsCalculator::calculateMetrics(unsigned long intervalPulses, unsigned long intervalMs) {
     if (intervalMs == 0 && intervalPulses == 0) return; // 何もなければ計算しない

     // RPM calculation
     if (intervalMs > 0) {
          double intervalSeconds = (double)intervalMs / 1000.0;
          double revolutions = (double)intervalPulses / PULSES_PER_REVOLUTION; // 1パルス=1回転と仮定
          data.currentRpm = (float)(revolutions / intervalSeconds * 60.0);
          // RPMの異常値を補正 (オプション)
          if (data.currentRpm > 300) { // 例: 300RPMを超えることは通常ないとして0にする
              // Serial.printf("Warning: High RPM detected (%f), setting to 0.\n", data.currentRpm);
              data.currentRpm = 0.0f;
          }
     } else {
         // intervalMs=0 で intervalPulses>0 は通常ありえないが、念のためRPM=0
         data.currentRpm = 0.0f;
     }

     // Speed calculation
     data.currentSpeedKmh = data.currentRpm / 60.0f * DISTANCE_PER_REV_M * 3.6f;

     // Distance calculation
     double intervalRevolutions = (double)intervalPulses / PULSES_PER_REVOLUTION;
     double intervalDistanceKm = intervalRevolutions * DISTANCE_PER_REV_M / 1000.0;
     data.sessionDistanceKm += (float)intervalDistanceKm;
     data.cumulativeDistanceKm += (float)intervalDistanceKm;

     // Calorie calculation (Model A1: RPM based)
     if (intervalMs > 0) {
         double intervalSeconds = (double)intervalMs / 1000.0;
         // カロリー計算 (Model A1)
         double calorieRate = CALORIES_RPM_K1_FACTOR * data.currentRpm; // カロリー/秒
         if (calorieRate < 0.0) calorieRate = 0.0; // マイナスにならないように
         double intervalCalories = calorieRate * intervalSeconds;
         data.sessionCaloriesKcal += (float)intervalCalories;
         data.cumulativeCaloriesKcal += (float)intervalCalories;
     }
     // METs calculation (簡易的なもの、速度に応じて設定)
     // 必要に応じてより精緻な計算式を導入
     if (data.currentSpeedKmh < 1) data.currentMets = 1.0f; // Resting
     else if (data.currentSpeedKmh < 10) data.currentMets = 3.0f; // Light
     else if (data.currentSpeedKmh < 16) data.currentMets = 4.0f; // Moderate
     else data.currentMets = 6.0f; // Vigorous (example)
}

// getData: 計算済みデータを返す
const TrackerData& MetricsCalculator::getData() const {
    return data;
}

// isMoving: SLEEP_TIMEOUT_MS 以内に動きがあったか
bool MetricsCalculator::isMoving() const {
    return moving;
}

// getLastPulseObservedMs: 最後にパルスを観測した時刻
unsigned long MetricsCalculator::getLastPulseObservedMs() const {
    return lastPulseObservedMs;
}

// ★ 追加: isTimerRunning() の実装 ★
// TIMER_STOP_DELAY_MS 以内に動きがあったか
bool MetricsCalculator::isTimerRunning() const {
    return timer_running;
}

// saveCumulativeData (現状未使用)
void MetricsCalculator::saveCumulativeData() {
     Serial.println("[MetricsCalculator] saveCumulativeData called - Deprecated (Data is saved to SD)");
     // storage.saveLatestDataToSD(data); // 必要ならここで呼ぶ
}