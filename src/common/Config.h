#pragma once

// ゲーム全体の設定値。ゲームの調整に使う値はこのファイルにまとめる。
namespace Config {
    // ウィンドウのタイトル（Windows版）。Android版のアプリ名は android/app/src/main/res/values/strings.xml
    constexpr const char* WindowTitle = "弾幕";

    // ゲームの画面の大きさ。Windows版はこの大きさのウィンドウ、Android版はこれを端末の画面に拡大して表示する
    constexpr int ScreenWidth = 800;
    constexpr int ScreenHeight = 600;

    // ウィンドウの移動中などで1フレームが長引いたとき、時間が一気に進まないようにする上限（現実の秒）
    constexpr double MaxFrameSeconds = 0.1;

    // 背景色（Android版で、ゲームの画面の外側に余った部分もこの色で塗る）
    constexpr int BackgroundR = 0;
    constexpr int BackgroundG = 0;
    constexpr int BackgroundB = 0;

    // 1回に進める時間の上限（秒）。速い弾が敵をすり抜けないように、1フレームをこの長さ以下に分けて進める
    constexpr float StepSeconds = 1.0f / 240.0f;

    // コア
    constexpr float CoreRadius = 16.0f;
    constexpr int CoreMaxHp = 100;

    // 速射砲（基本型）
    constexpr float RapidGunInterval = 0.1f;      // 撃つ間隔（秒）
    constexpr float RapidGunBulletSpeed = 600.0f; // 弾の速さ（ドット/秒）
    constexpr int RapidGunDamage = 1;
    constexpr float RapidGunSpreadDegrees = 2.0f; // 狙いからずれる最大の角度（度）
    constexpr float PlayerBulletRadius = 3.0f;

    // 敵の出現。間隔は時間とともに短くなる（最初の間隔 − 経過秒 × 短くなる速さ。下限あり）
    constexpr float SpawnIntervalStart = 1.5f;
    constexpr float SpawnIntervalMin = 0.4f;
    constexpr float SpawnIntervalShrinkPerSecond = 0.01f;
    constexpr float SpawnMargin = 30.0f;          // 画面の外のどれだけ離れた位置から現れるか
    constexpr float EnemyHpGrowthSeconds = 60.0f; // この秒数ごとに、敵のHPが元の値1つ分ずつ増える

    // 敵の弾
    constexpr float EnemyBulletSpeed = 150.0f;
    constexpr float EnemyBulletRadius = 4.0f;
    constexpr int EnemyBulletDamage = 5;

    // 突撃型：コアへまっすぐ近づき、体当たりする
    constexpr int ChargerHp = 3;
    constexpr float ChargerSpeed = 80.0f;
    constexpr float ChargerRadius = 12.0f;
    constexpr int ChargerRamDamage = 10;

    // 射撃型：コアから一定の距離を保ちながら回り込み、弾を撃つ
    constexpr int ShooterHp = 4;
    constexpr float ShooterSpeed = 60.0f;
    constexpr float ShooterRadius = 12.0f;
    constexpr float ShooterKeepDistance = 220.0f;
    constexpr float ShooterShotInterval = 1.5f;

    // 回避型：遠めの距離を保って撃ちながら、飛んでくる弾の進路を先読みして避ける
    constexpr int EvaderHp = 3;
    constexpr float EvaderSpeed = 90.0f;
    constexpr float EvaderRadius = 11.0f;
    constexpr float EvaderKeepDistance = 260.0f;
    constexpr float EvaderShotInterval = 2.0f;
    constexpr float EvaderLookAheadSeconds = 0.8f;   // 何秒先までの弾の進路を見るか
    constexpr float EvaderDodgeMargin = 20.0f;       // 弾との間にあけたい隙間（ドット）
    constexpr float EvaderDodgeSpeedRate = 1.2f;     // 避けるときの速さ（ふだんの速さに対する倍率）
    constexpr float EvaderReactionMin = 0.1f;       // 弾を見直す間隔（秒）の最小と最大。この間でばらつく
    constexpr float EvaderReactionMax = 0.35f;
    constexpr float EvaderMissChance = 0.3f;        // 見直したときに、弾を見落とす確率

    // ゲームオーバーのあと、やり直しの操作を受け付けるまでの時間（秒）
    constexpr float RestartDelay = 0.5f;
}
