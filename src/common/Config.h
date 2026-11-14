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
    constexpr float RapidTurnDegrees = 360.0f;    // 砲が1秒間に旋回できる角度（度）
    // 狙っていた相手がいなくなり、次の相手との角度の差がこれより大きいときは、撃つのをやめて速く旋回する
    constexpr float RapidRetargetDegrees = 45.0f;
    constexpr float RapidFastTurnDegrees = 720.0f; // そのときの旋回の速さ（度/秒）
    constexpr float RapidResumeDegrees = 10.0f;    // 角度の差がこれ以内になったら、撃つのを再開してふだんの旋回に戻る
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
    constexpr float MissileSpeed = 600.0f;
    constexpr float MissileDamage = 3.0f;
    constexpr float MissileTurnDegrees = 360.0f;  // 1秒間に曲がれる角度（度）
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

    // 派生版（上の基本型との違い）
    constexpr float RapidSweepDegrees = 30.0f;       // 速射砲 弾幕型・攻撃型：扇形に往復する幅（片側の角度）
    constexpr float RapidSweepSpeedDegrees = 120.0f; // 往復する速さ（度/秒）
    constexpr float RapidCloseInterval = 0.05f;      // 速射砲 近接防御型：撃つ間隔
    constexpr float RapidCloseRange = 150.0f;        // 速射砲 近接防御型：この距離に何か入ったら撃ち始め、いなくなったらやめる
    constexpr int RapidBarrageEnergy = 2;
    constexpr int RapidAttackEnergy = 2;
    constexpr int RapidCloseEnergy = 2;
    constexpr float BeamDecisiveDamageScale = 2.0f;  // ビーム砲 決戦型：威力（基本型に対する倍率）
    constexpr float BeamDecisiveIntervalScale = 2.0f; // ビーム砲 決戦型：撃つ間隔（基本型に対する倍率）
    constexpr int BeamAttackEnergy = 3;
    constexpr int BeamDecisiveEnergy = 3;
    constexpr float MissileAttackBlastScale = 2.0f;  // 誘導兵器 攻撃型：消えるときの爆発の半径（基本型に対する倍率）
    constexpr float MissileCloseRange = 180.0f;      // 誘導兵器 近接防御型：この距離に来たものだけを撃つ
    constexpr int MissileAttackEnergy = 3;
    constexpr int MissileCloseEnergy = 2;
    constexpr float ShellDecisiveRange = 2000.0f;    // 炸裂弾 決戦型：直撃を狙うので、途中で炸裂しない距離にする
    constexpr int ShellDecisiveEnergy = 4;

    // 連装型
    constexpr float MultiBarrelEnergyRate = 0.8f;    // エネルギー＝元のエネルギー×門数×この割合（四捨五入、最低でも元＋1）
    constexpr float RapidBarrelSpacing = 8.0f;       // 速射砲：並べた砲の間隔（ドット）
    constexpr float BeamBarrelSpacing = 10.0f;       // ビーム砲：並べた砲の間隔（ドット）
    constexpr float BeamVolleyGap = 0.05f;           // ビーム砲：一斉射で1門ずつずらす時間（秒）
    constexpr float MissileVolleyGap = 0.08f;        // 誘導兵器：一斉射で1門ずつずらす時間（秒）
    constexpr float ShellVolleyGap = 0.1f;           // 炸裂弾：一斉射で1門ずつずらす時間（秒）
    constexpr float ShellFanDegrees = 8.0f;          // 炸裂弾：扇形で1門ごとに向きを変える角度

    // 開始時のエネルギー上限
    constexpr int StartEnergy = 3;

    // 強化パーツの効果（1個あたり）
    constexpr float PartPowerRate = 0.5f;          // 威力強化：威力が元の値のこの割合ずつ増える（速射砲・ビーム砲・誘導兵器）
    constexpr float ShellPartPowerRate = 0.3f;     // 威力強化（炸裂弾）：直撃・爆炎・破片の威力
    constexpr float PartRateMultiplier = 0.8f;     // 連射強化：撃つ間隔がこの倍率ずつ短くなる
    constexpr float RapidFocusMultiplier = 0.5f;   // 収束：ばらつきがこの倍率ずつ小さくなる
    constexpr float RapidPierceChancePerPart = 0.25f; // 貫徹力強化（速射砲）：当たったときに貫く確率
    constexpr float RapidPierceDecay = 0.7f;       // 速射砲の弾が貫いたあとに残る威力の割合
    constexpr float BeamPierceDecayPerPart = 0.1f; // 貫徹力強化（ビーム砲）：貫いたあとに残る威力の割合が増える量
    constexpr float BeamDecayMax = 0.95f;
    constexpr float MissileHomingRate = 0.3f;      // 誘導力強化：曲がる力が増える割合
    constexpr float MissileBlastRate = 0.3f;       // 爆発範囲拡大：爆発の半径が増える割合
    constexpr float ShellFlameRadiusRate = 0.25f;  // 爆炎範囲拡大：爆炎の半径が増える割合
    constexpr float ShellFlameDurationRate = 0.5f; // 爆炎持続延長：爆炎の残る時間が増える割合
    constexpr int ShellFragmentsPerPart = 5;       // 破片増加：増える破片の数

    // 落とし物
    constexpr float DropChance = 0.1f;             // 敵を倒したときに、武装か強化パーツを落とす確率
    constexpr int ExpFirstNeed = 10;               // 最初のエネルギー上限解放パーツが出るまでに必要な経験値
    constexpr int ExpNeedGrowth = 5;               // 1つ出るたびに、次に必要な経験値が増える量
    constexpr int EvaderExp = 2;                   // 回避型を倒したときの経験値（ほかの敵は1）
    constexpr float PickupWaitSeconds = 0.3f;      // 落ちた場所にとどまる時間
    constexpr float PickupStartSpeed = 200.0f;     // コアへ吸い寄せられ始める速さ
    constexpr float PickupAcceleration = 600.0f;
    constexpr float PickupMaxSpeed = 700.0f;
    constexpr float PickupRadius = 8.0f;
    constexpr float MessageSeconds = 2.0f;         // 拾ったものを画面の上に表示する時間

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

    // 強敵（ボス）：重装型を大きく強くした形。コアの周りを旋回しながら、扇形に弾をばらまく
    constexpr float BossInterval = 60.0f;         // 強敵がいない時間がこれだけ続くと現れる（秒）
    constexpr float BossWarningSeconds = 3.0f;    // 現れる何秒前から「強敵接近」と知らせるか
    constexpr float BossHp = 150.0f;              // 元のHP（普通の敵と同じ割合で、時間とともに増える）
    constexpr float BossSpeed = 40.0f;
    constexpr float BossTurnDegrees = 30.0f;
    constexpr float BossRadius = 30.0f;
    constexpr float BossKeepDistance = 250.0f;
    constexpr float BossShotInterval = 1.5f;
    constexpr int BossShotCount = 5;              // 1回に扇形に撃つ弾の数
    constexpr float BossShotSpreadDegrees = 12.0f; // 扇形の弾どうしの間の角度
    constexpr int BossRamDamage = 30;
    constexpr float BossRamCooldown = 1.0f;       // 体当たりのあと、次に体当たりのダメージを与えるまでの時間

    // ゲームオーバーのあと、やり直しの操作を受け付けるまでの時間（秒）
    constexpr float RestartDelay = 0.5f;
}
