#pragma once
#include <cstddef>

// 異常終了したときの原因を調べるための記録。
// ゲームの処理の区切りごとに「今何をしているか」と物の数を覚えておき、
// 異常終了の瞬間に Platform（Android版）がそれを文章にしてファイルへ書き出す
namespace CrashState {
    // 今行っている処理
    enum class Phase {
        Menu,         // 戦闘以外の画面（タイトル・組み込み画面など）
        Spawn,        // 敵の出現
        Weapons,      // 武装（撃つ処理）
        Enemies,      // 敵の動き
        MoveBullets,  // 弾の移動
        MoveBeams,    // ビームの移動
        MoveMissiles, // 誘導弾の移動
        MoveShells,   // 炸裂弾の移動
        Explosions,   // 爆発の時間経過
        Collisions,   // 当たり判定
        Pickups,      // 落とし物
        Cleanup,      // 消えたものの取り除き
        Draw,         // 描画
    };

    void SetPhase(Phase phase);
    void SetElapsed(float seconds);
    void SetCounts(int enemies, int enemyBullets, int playerBullets, int missiles, int beams, int shells,
                   int explosions, int pickups);

    // 覚えている状況を、異常終了の種類（signalName）と合わせて文章にして buffer へ書く。書いた長さを返す。
    // 異常終了の瞬間に呼ぶので、メモリの確保や snprintf などを使わずに書く
    size_t FormatReport(char* buffer, size_t capacity, const char* signalName);
}
