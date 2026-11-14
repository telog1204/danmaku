#pragma once

// 最高記録。ゲームオーバーのたびに更新して、ファイルに保存する
struct Records {
    bool hasRecord = false;  // 一度でも遊んだか（まだなら、タイトル画面に最高記録を出さない）
    float bestSeconds = 0.0f; // 最も長く生き残った時間（秒）
    int bestKills = 0;        // 最も多く倒した敵の数
};

// 保存した最高記録を読む（ファイルがない・読めないときは、記録なしを返す）
Records LoadRecords();

// 最高記録を保存する
void SaveRecords(const Records& records);

// 今回の結果で最高記録を更新する。どちらかを超えたら true（新記録）
bool UpdateRecords(Records& records, float seconds, int kills);
