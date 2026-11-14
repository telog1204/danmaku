#include "pch.h"
#include "Records.h"
#include "Platform.h"

namespace {
    // 保存するファイル。中身は「版の番号・生存時間・倒した数」を1行ずつ書いたテキスト
    constexpr const char* FileName = "records.txt";
    constexpr int FileVersion = 1;
}

Records LoadRecords() {
    Records records;
    std::ifstream in(Platform::SaveFilePath(FileName));
    int version = 0;
    float seconds = 0.0f;
    int kills = 0;
    if (in >> version >> seconds >> kills && version == FileVersion && seconds >= 0.0f && kills >= 0) {
        records.hasRecord = true;
        records.bestSeconds = seconds;
        records.bestKills = kills;
    }
    return records;
}

void SaveRecords(const Records& records) {
    std::ofstream out(Platform::SaveFilePath(FileName), std::ios::trunc);
    out.precision(9);  // 読み戻したときに同じ値になるよう、桁を省かずに書く
    out << FileVersion << "\n" << records.bestSeconds << "\n" << records.bestKills << "\n";
}

bool UpdateRecords(Records& records, float seconds, int kills) {
    bool updated = false;
    if (!records.hasRecord || seconds > records.bestSeconds) {
        records.bestSeconds = seconds;
        updated = true;
    }
    if (!records.hasRecord || kills > records.bestKills) {
        records.bestKills = kills;
        updated = true;
    }
    // 初めて遊んだときは記録を作るだけで、「新記録」とは言わない
    const bool isNew = records.hasRecord && updated;
    records.hasRecord = true;
    return isNew;
}
