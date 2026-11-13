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
    constexpr float RapidGunDamage = 1.0f;
    constexpr float RapidGunSpreadDegrees = 2.0f; // 狙いからずれる最大の角度（度）
    constexpr float PlayerBulletRadius = 3.0f;
    constexpr int RapidGunEnergy = 1;

    // ビーム砲（基本型）。敵を貫き、貫くたびに威力が下がる。長い尾を含めた線全体に当たり判定がある
    constexpr float BeamInterval = 0.3f;
    constexpr float BeamSpeed = 1200.0f;
    constexpr float BeamDamage = 3.0f;
    constexpr float BeamDecay = 0.7f;             // 1つ貫くごとに、威力がこの割合になる
    constexpr float BeamTailLength = 80.0f;
    constexpr float BeamHalfWidth = 2.0f;         // 当たり判定の太さの半分
    constexpr int BeamEnergy = 3;

    // 誘導兵器（基本型）。撃ち出して一瞬まっすぐ進んでから追いかける。何かに当たると小さな爆発を起こす
    constexpr float MissileInterval = 0.8f;
    constexpr float MissileSpeed = 300.0f;
    constexpr float MissileDamage = 3.0f;
    constexpr float MissileTurnDegrees = 240.0f;  // 1秒間に曲がれる角度（度）
    constexpr float MissileStraightSeconds = 0.15f; // 撃ち出してからまっすぐ進む時間
    constexpr float MissileRadius = 4.0f;
    constexpr float MissileLifeSeconds = 4.0f;    // これより長く飛んだら消える
    constexpr float MissileExplosionRadius = 25.0f;
    constexpr float MissileExplosionDamage = 1.0f;
    constexpr float MissileExplosionSeconds = 0.12f;
    constexpr int MissileEnergy = 2;

    // 炸裂弾（基本型）。敵が多い場所の少し手前で炸裂し、残る爆炎と、前方へ円錐形の破片を出す
    constexpr float ShellInterval = 1.2f;
    constexpr float ShellSpeed = 500.0f;
    constexpr float ShellRadius = 5.0f;
    constexpr float ShellDirectDamage = 15.0f;    // 直撃した敵への威力
    constexpr float ShellShortDistance = 30.0f;   // 敵が多い場所の中心から、どれだけ手前で炸裂するか
    constexpr float CrowdRadius = 100.0f;         // 「敵が多い場所」を数えるときの範囲（半径）
    constexpr float ShellFlameRadius = 70.0f;
    constexpr float ShellFlameDamage = 8.0f;
    constexpr float ShellFlameSeconds = 0.4f;     // 爆炎がその場に残る時間
    constexpr int ShellFragmentCount = 14;
    constexpr float ShellFragmentSpreadDegrees = 30.0f; // 破片が広がる角度（片側）
    constexpr float ShellFragmentSpeed = 400.0f;
    constexpr float ShellFragmentSeconds = 0.4f;  // 破片が飛ぶ時間
    constexpr float ShellFragmentDamage = 3.0f;
    constexpr float FragmentRadius = 2.0f;
    constexpr int ShellEnergy = 3;

    // 開始時のエネルギー上限
    constexpr int StartEnergy = 3;

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

    // 敵の動き（航空機のような動き）。常に機首の向きへ進み、向きと速さは少しずつしか変えられない
    constexpr float EnemyAcceleration = 80.0f;    // 速さを変えられる量（ドット/秒を1秒あたり）
    constexpr float EnemyEdgeMargin = 60.0f;      // 画面の端からこの距離に入ったら、旋回して内側へ戻る

    // 突撃型：コアへ機首を向けて突っ込み、体当たりする
    constexpr int ChargerHp = 3;
    constexpr float ChargerSpeed = 80.0f;
    constexpr float ChargerTurnDegrees = 90.0f;   // 1秒間に曲がれる角度（度）
    constexpr float ChargerRadius = 12.0f;
    constexpr int ChargerRamDamage = 10;

    // 射撃型：コアの周りを旋回しながら、弾を撃つ
    constexpr int ShooterHp = 4;
    constexpr float ShooterSpeed = 60.0f;
    constexpr float ShooterTurnDegrees = 60.0f;
    constexpr float ShooterRadius = 12.0f;
    constexpr float ShooterKeepDistance = 220.0f;
    constexpr float ShooterShotInterval = 1.5f;

    // 回避型：遠めの距離で旋回して撃ちながら、飛んでくる弾の進路を先読みし、旋回してかわす
    constexpr int EvaderHp = 3;
    constexpr float EvaderSpeed = 90.0f;
    constexpr float EvaderDodgeSpeed = 130.0f;    // 避けるときに加速する速さの上限
    constexpr float EvaderTurnDegrees = 120.0f;
    constexpr float EvaderRadius = 11.0f;
    constexpr float EvaderKeepDistance = 260.0f;
    constexpr float EvaderShotInterval = 2.0f;
    constexpr float EvaderLookAheadSeconds = 0.8f;   // 何秒先までの弾の進路を見るか
    constexpr float EvaderDodgeMargin = 20.0f;       // 弾との間にあけたい隙間（ドット）
    constexpr float EvaderReactionMin = 0.1f;       // 弾を見直す間隔（秒）の最小と最大。この間でばらつく
    constexpr float EvaderReactionMax = 0.35f;
    constexpr float EvaderMissChance = 0.3f;        // 見直したときに、弾を見落とす確率

    // ゲームオーバーのあと、やり直しの操作を受け付けるまでの時間（秒）
    constexpr float RestartDelay = 0.5f;
}
