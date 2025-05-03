#ifndef TRACKER_DATA_HPP
#define TRACKER_DATA_HPP

#include <stdint.h> // For uint64_t

// データの入れ物
struct TrackerData {
    // 現在のセッションデータ
    unsigned long sessionStartTimeMs = 0; // ★ 名前変更
    unsigned long sessionElapsedTimeMs = 0;
    float sessionDistanceKm = 0.0f;
    float sessionCaloriesKcal = 0.0f;
    unsigned long sessionPulseCount = 0;

    // 瞬間的なデータ / 計算値
    float currentRpm = 0.0f;
    float currentSpeedKmh = 0.0f;
    float currentMets = 1.0f; // ★ floatに変更

    // 累積データ (NVSからロード/保存)
    uint64_t cumulativeTimeMs = 0;
    float cumulativeDistanceKm = 0.0f;
    float cumulativeCaloriesKcal = 0.0f;

    // ゼロクリア（セッション開始/リセット時）
    void resetSession() {
        sessionStartTimeMs = 0; // ★ 名前変更
        sessionElapsedTimeMs = 0;
        sessionDistanceKm = 0.0f;
        sessionCaloriesKcal = 0.0f;
        sessionPulseCount = 0;
        currentRpm = 0.0f;
        currentSpeedKmh = 0.0f;
        currentMets = 1.0f;
    }
};

#endif // TRACKER_DATA_HPP