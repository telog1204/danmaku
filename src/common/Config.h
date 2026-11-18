#pragma once

// ゲーム全体の設定値。ゲームの調整に使う値はこのファイルにまとめる。
namespace Config {
    // ウィンドウのタイトル（Windows版）。Android版のアプリ名は android/app/src/main/res/values/strings.xml
    constexpr const char* WindowTitle = "弾幕";

    // ゲームの画面の大きさ。Windows版はこの大きさのウィンドウ、Android版はこれを端末の画面に拡大して表示する
    constexpr int ScreenWidth = 800;
    constexpr int ScreenHeight = 600;

    // フィールド（敵や弾が動き回る広さ）。画面より広く、ViewScale 倍に縮めて画面に描く（カメラを引いた見え方）
    // HP・時間などの表示は縮めない
    constexpr int FieldWidth = 1600;
    constexpr int FieldHeight = 1200;
    constexpr float ViewScale = 0.5f;

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
    constexpr float RapidGunBulletSpeed = 1500.0f; // 弾の速さ（ドット/秒）
    constexpr float RapidGunSpreadDegrees = 2.0f; // 狙いからずれる最大の角度（度）
    constexpr float RapidTurnDegrees = 180.0f;    // 砲が1秒間に旋回できる角度（度）
    // 砲塔（速射砲・ビーム砲）：狙っていた相手がいなくなり、次の相手との角度の差がこれより大きいときは、撃つのをやめて速く旋回する
    constexpr float TurretRetargetDegrees = 45.0f;
    constexpr float RapidFastTurnDegrees = 360.0f; // そのときの旋回の速さ（度/秒）
    constexpr float TurretResumeDegrees = 10.0f;   // 角度の差がこれ以内になったら、撃つのを再開してふだんの旋回に戻る
    // 狙っていた相手がいなくなったとき、今の向きからこの角度（片側）以内に狙える相手がいれば、旋回せずに済むその相手を優先する
    constexpr float TurretKeepAimDegrees = 20.0f;
    constexpr float PlayerBulletRadius = 3.0f;
    constexpr float RapidBulletTailLength = 24.0f; // 弾の見た目の長さ（赤一色の短い線で描く。当たり判定は丸のまま）
    constexpr int RapidGunEnergy = 10;

    // ビーム砲（基本型）。敵を貫き、貫くたびに威力が下がる。長い尾を含めた線全体に当たり判定がある
    constexpr float BeamInterval = 0.6f;
    // ビーム砲は旋回中に撃たないので、旋回の速さは1つ（相手が替わったときも同じ）
    constexpr float BeamTurnDegrees = 360.0f;       // 砲が1秒間に旋回できる角度の最高（度）
    constexpr float BeamTurnAccelDegrees = 720.0f; // 旋回の角加速度（度/秒²）。0.5秒で最高の速さ。近づくと少しずつ遅くなって止まる
    constexpr float BeamAimedDegrees = 1.0f;       // 狙う方向とのずれがこれ以内なら旋回を終えたとみなし、撃てる
    constexpr float BeamHoldTurnDegrees = 30.0f;   // 旋回を終えて撃つ間隔を待つ間、相手を向き続ける低速の最高（度/秒）
    constexpr float BeamHoldBreakDegrees = 5.0f;   // 待つ間にずれがこれを超えたら、ふつうの旋回に戻る
    constexpr float BeamPostFireHoldSeconds = 0.15f; // 撃った後（連装は撃ち切った後）、砲を止めておく時間（秒）
    // 待ち伏せ：撃つ間隔が来たら、砲が向き直って止まるまでの時間に相手が進む先へ向き直って止まり、相手が来た瞬間に撃つ
    constexpr float BeamAmbushMarginSeconds = 0.05f;  // 向き直って止まるまでの時間に足す余裕（秒）
    constexpr float BeamAmbushMaxLeadSeconds = 1.0f;  // 相手が進む先を読む時間の最大（秒）
    constexpr float BeamAmbushMaxWaitSeconds = 0.3f;  // 止まってから、読んだ時間よりこれ以上待っても来なければ計算し直す（秒）
    constexpr float BeamPierceBulletWeight = 0.5f; // 攻撃型：線の上に乗る数を数えるときの、敵の弾1つの重み（敵本体は1）
    constexpr float BeamSpeed = 2400.0f;
    constexpr float BeamDamage = 1.0f;            // 攻撃力（重の敵のHPを減らす量。軽・中には効くか効かないかだけ）
    constexpr float BeamDecay = 0.7f;             // 1つ貫くごとに、威力がこの割合になる
    constexpr float BeamTailLength = 320.0f;
    constexpr float BeamHalfWidth = 2.0f;         // 当たり判定の太さの半分
    constexpr int BeamEnergy = 30;

    // 誘導兵器（基本型）。撃ち出して一瞬まっすぐ進んでから追いかける。何かに当たると小さな爆発を起こす
    constexpr float MissileInterval = 0.8f;
    constexpr float MissileSpeed = 1500.0f;
    constexpr float MissileDamage = 1.0f;         // 直撃したときの攻撃力（重の敵のHPを減らす量）
    constexpr float MissileTurnDegrees = 360.0f;  // 1秒間に曲がれる角度（度）
    constexpr float MissileStraightSeconds = 0.25f; // 撃ち出してからまっすぐ進む時間（近接防御型はまっすぐ進まない）。この間は爆発に巻き込まれない
    constexpr float MissileViewDegrees = 120.0f;  // 誘導弾が見える前方の範囲（度。左右に半分ずつ）。後ろ側は見えない
    constexpr int MissileTargetCap = 2;           // 同じ相手を追いかけられる誘導弾の数（撃った武装に関係なく、すべての誘導弾で数える）
    constexpr float MissileRadius = 4.0f;
    constexpr float MissileLifeSeconds = 4.0f;    // これより長く飛んだら消える
    constexpr float MissileExplosionRadius = 25.0f;
    constexpr float MissileExplosionSeconds = 0.12f;
    constexpr int MissileEnergy = 20;

    // 炸裂弾（基本型）。敵が多い場所の少し手前で炸裂し、残る爆炎と、前方へ円錐形の破片を出す
    constexpr float ShellInterval = 1.2f;
    constexpr float ShellSpeed = 1250.0f;
    constexpr float ShellRadius = 5.0f;
    constexpr float ShellDirectDamage = 2.0f;     // 直撃したときの攻撃力（重の敵のHPを減らす量）
    constexpr float ShellShortDistance = 30.0f;   // 敵が多い場所の中心から、どれだけ手前で炸裂するか
    constexpr float CrowdRadius = 100.0f;         // 「敵が多い場所」を数えるときの範囲（半径）
    constexpr float CrowdRefreshSeconds = 0.1f;   // 速射砲の攻撃型：「軽の敵と敵の弾が多い場所」を数え直す間隔（数が多く重いため）
    constexpr float ShellFlameRadius = 70.0f;
    constexpr float ShellFlameSeconds = 0.4f;     // 爆炎がその場に残る時間
    constexpr int ShellFragmentCount = 14;
    constexpr float ShellFragmentSpreadDegrees = 30.0f; // 破片が広がる角度（片側）
    constexpr float ShellFragmentSpeed = 1000.0f;
    constexpr float ShellFragmentSeconds = 0.4f;  // 破片が飛ぶ時間
    constexpr float FragmentRadius = 2.0f;
    constexpr int ShellEnergy = 30;

    // 派生版（上の基本型との違い）
    constexpr float RapidSweepDegrees = 15.0f;       // 速射砲 弾幕型・攻撃型：扇形に往復する幅（片側の角度）。端でゆっくりになる滑らかな往復
    constexpr float RapidSweepPeriodMin = 0.8f;      // 1往復の時間（秒）。速射砲ごとにこの範囲でランダムに決め、全体で揃わないようにする
    constexpr float RapidSweepPeriodMax = 1.2f;
    constexpr float RapidCloseInterval = 0.05f;      // 速射砲 近接防御型：撃つ間隔
    constexpr float RapidCloseRange = 300.0f;        // 速射砲 近接防御型：この距離（射程）に入った相手を狙う（狙った相手が外へ出ても狙い続ける）
    constexpr float RapidCloseTurnDegrees = 360.0f;     // 速射砲 近接防御型：旋回の速さ（ほかの速射砲の2倍）
    constexpr float RapidCloseFastTurnDegrees = 720.0f; // 速射砲 近接防御型：相手が替わって大きく向き直るときの旋回の速さ
    constexpr int RapidBarrageEnergy = 10;
    constexpr int RapidAttackEnergy = 10;
    constexpr int RapidCloseEnergy = 20;
    constexpr float BeamDecisiveDamageScale = 1.0f;  // ビーム砲 決戦型：威力（基本型に対する倍率）
    constexpr float BeamDecisiveIntervalScale = 1.0f; // ビーム砲 決戦型：撃つ間隔（基本型に対する倍率）
    constexpr int BeamAttackEnergy = 30;
    constexpr int BeamDecisiveEnergy = 30;
    constexpr float MissileAttackBlastScale = 2.0f;  // 誘導兵器 攻撃型：消えるときの爆発の半径（基本型に対する倍率）
    constexpr float MissileCloseRange = 360.0f;      // 誘導兵器 近接防御型：この距離に来たものだけを撃つ
    constexpr int MissileAttackEnergy = 20;
    constexpr int MissileCloseEnergy = 20;
    constexpr int ShellDecisiveEnergy = 30;

    // 連装型
    constexpr float MultiBarrelEnergyRate = 0.8f;    // 連装＝単装×2、三連装＝単装×3、四連装＝連装×2、八連装＝四連装×2 に、この割合をかける（四捨五入）
    constexpr float RapidBarrelSpacing = 8.0f;       // 速射砲：並べた砲の間隔（ドット）
    constexpr float BeamBarrelSpacing = 10.0f;       // ビーム砲：並べた砲の間隔（ドット）
    constexpr float RapidBarrelLength = 28.0f;       // 速射砲：画面に描く砲身の長さ（コアの中心から先端まで）
    constexpr float BeamBarrelLength = 48.0f;        // ビーム砲：画面に描く砲身の長さ
    constexpr float BeamVolleyGap = 0.05f;           // ビーム砲：一斉射で1門ずつずらす時間（秒）
    constexpr float MissileVolleyGap = 0.15f;        // 誘導兵器：一斉射で1門ずつずらす時間（秒）
    // 誘導兵器の発射装置：コアの左右から生える四角（固定で回らない）。撃ち出す向きが右寄りなら右、左寄りなら左の装置を使い、
    // 装置のふちに撃ち出す向きに対して左右に並べた2つの出口から、1発ごとに交互に撃ち出す（2列になる）
    constexpr float MissileLauncherLength = 30.0f;   // 発射装置：コアの中心から先端までの長さ
    constexpr float MissileLauncherHeight = 14.0f;   // 発射装置：高さ
    constexpr float MissileTubeSpacing = 8.0f;       // 発射装置：2つの出口の間隔
    constexpr float ShellVolleyGap = 0.4f;           // 炸裂弾：一斉射で1門ずつずらす時間（秒）。爆炎が残る時間と同じにして、爆炎が重ならずに続けて燃えるように
    constexpr float ShellBarrelLength = 40.0f;       // 炸裂弾：画面に描く砲身の長さ（コアの中心から先端まで）
    constexpr float ShellBarrelSpacing = 10.0f;      // 炸裂弾：並べた砲の間隔（平行に並べ、弾も平行に出る）

    // ロックオン：「最も近い敵」を狙う武装は、近い順にこの数までの中からランダムに選んでロックオンする
    constexpr size_t LockOnCandidates = 3;

    // エネルギー（コスト）。単位を細かくするため、10倍の数字で扱う
    constexpr int StartEnergy = 30;               // 開始時のエネルギー上限
    constexpr int EnergyPerUnlock = 10;           // エネルギー上限解放パーツ1つで増える量
    constexpr int PartEnergyUnit = 10;            // 強化パーツを付けたときに増える量（1個目×1、2個目×2、3個目×3…）

    // 強化パーツの効果（1個あたり）
    constexpr float PartPowerRate = 0.5f;          // 威力強化：攻撃力が元の値のこの割合ずつ増える（ビーム砲・誘導兵器）
    constexpr float ShellPartPowerRate = 0.3f;     // 威力強化（炸裂弾）：直撃の攻撃力
    constexpr float RapidMediumKillChanceBase = 0.01f;    // 速射砲の弾が中の敵に当たったときに倒す確率（重には効かない）
    constexpr float RapidMediumKillChancePerPart = 0.02f; // 威力強化（速射砲）：中の敵を倒す確率が、1個につき増える量
    constexpr float PartRateMultiplier = 0.8f;     // 連射強化：撃つ間隔がこの倍率ずつ短くなる
    constexpr float RapidFocusMultiplier = 0.5f;   // 収束：ばらつきがこの倍率ずつ小さくなる
    constexpr float RapidPierceChancePerPart = 0.25f; // 貫徹力強化（速射砲）：当たったときに貫く確率
    constexpr float BeamPierceDecayPerPart = 0.1f; // 貫徹力強化（ビーム砲）：貫いたあとに残る威力の割合が増える量
    constexpr float BeamDecayMax = 0.95f;
    constexpr float MissileHomingRate = 0.3f;      // 誘導力強化：曲がる力が増える割合
    constexpr float MissileBlastRate = 0.3f;       // 爆発範囲拡大：爆発の半径が増える割合
    constexpr float ShellFlameRadiusRate = 0.25f;  // 爆炎範囲拡大：爆炎の半径が増える割合
    constexpr float ShellFlameDurationRate = 0.5f; // 爆炎持続延長：爆炎の残る時間が増える割合
    constexpr int ShellFragmentsPerPart = 5;       // 破片増加：増える破片の数

    // 手に入るもの（敵を倒した瞬間に手に入る）
    constexpr float DropChance = 0.1f;             // 敵を倒したときに、武装か強化パーツが手に入る確率
    constexpr int ExpFirstNeed = 10;               // 最初のエネルギー上限解放パーツが手に入るまでに必要な経験値
    constexpr int ExpNeedGrowth = 5;               // 1つ手に入るたびに、次に必要な経験値が増える量
    constexpr float MessageSeconds = 2.0f;         // 手に入れたものを画面の上に表示する時間

    // 敵の出現。間隔は時間とともに短くなる（最初の間隔 − 経過秒 × 短くなる速さ。下限あり）
    constexpr float SpawnIntervalStart = 1.5f;
    constexpr float SpawnIntervalMin = 0.4f;
    constexpr float SpawnIntervalShrinkPerSecond = 0.01f;
    constexpr float SpawnMargin = 60.0f;          // フィールドの外のどれだけ離れた位置から現れるか

    // 敵の弾
    constexpr float EnemyBulletSpeed = 375.0f;
    constexpr float EnemyBulletRadius = 4.0f;
    constexpr int EnemyBulletDamage = 5;

    // 敵の動き（航空機のような動き）。常に機首の向きへ進み、向きと速さは少しずつしか変えられない
    constexpr float EnemyAcceleration = 160.0f;    // 速さを変えられる量（ドット/秒を1秒あたり）
    constexpr float EnemyPreEntryLimit = 120.0f;  // まだフィールドに入っていない敵が、フィールドの外へこれ以上離れたら内側へ戻る
    constexpr float EnemyEdgeMargin = 120.0f;     // フィールドの端からこの距離に入ったら、旋回して内側へ戻る

    // 敵の区分（軽・中・重）。軽・中はHPを持たず、効く攻撃が当たれば1発で倒れる。HPを持つのは重だけ
    // （武装の攻撃力は、重の敵とのバランスにだけ使う）
    // 軽：どの攻撃でも1発で倒せる。速く、同じ型がまとまって現れ、当たりそうな弾を旋回してかわす
    constexpr float LightSpeed = 325.0f;
    constexpr float LightTurnDegrees = 180.0f;    // 1秒間に曲がれる角度（度）
    constexpr float LightDodgeSpeed = 450.0f;     // 避けるときに加速する速さの上限
    constexpr float LightRadius = 8.0f;
    constexpr int LightExp = 1;
    constexpr int LightGroupMin = 3;              // まとまって現れる数
    constexpr int LightGroupMax = 5;
    // 中：遅い。ビーム・誘導弾の直撃・炸裂弾の直撃で1発で倒れる。速射砲の弾・炸裂弾の破片・爆発は効かない
    constexpr float MediumSpeed = 84.0f;
    constexpr float MediumTurnDegrees = 42.0f;
    constexpr float MediumRadius = 12.0f;
    constexpr int MediumExp = 2;
    // 重：とても遅い。艦隊の旗艦としてだけ現れる。効く攻撃は中と同じで、攻撃力の分だけHPが減り、0で倒れる
    constexpr float HeavySpeed = 60.0f;
    constexpr float HeavyTurnDegrees = 30.0f;
    constexpr float HeavyRadius = 19.0f;
    constexpr float HeavyHpStart = 2.0f;          // 最初のHP
    constexpr float HeavyHpPerGrandFleet = 1.0f;  // 大艦隊を倒すたびに増えるHP（そのあとに現れる重から）
    constexpr int HeavyExp = 4;
    constexpr float HeavyAttackIntervalRate = 0.7f; // 重の攻撃の間隔（中に対する倍率）

    // 軽の型
    constexpr float StraferShotInterval = 1.5f;   // 離脱型：コアへ向かいながら撃つ間隔
    constexpr float StraferBreakDistance = 240.0f; // 離脱型：ここまで近づいたら旋回して離れる
    constexpr float StraferReturnDistance = 600.0f; // 離脱型：ここまで離れたら、またコアへ向かう
    constexpr int KamikazeRamDamage = 10;         // 自爆型：コアに体当たりしたときのダメージ
    // 当たりそうな弾をかわす動き（軽の敵すべて）
    constexpr float DodgeLookAheadSeconds = 0.8f; // 何秒先までの弾の進路を見るか
    constexpr float DodgeMargin = 20.0f;          // 弾との間にあけたい隙間（ドット）
    constexpr float DodgeReactionMin = 0.1f;      // 弾を見直す間隔（秒）の最小と最大。この間でばらつく
    constexpr float DodgeReactionMax = 0.35f;
    constexpr float DodgeMissChance = 0.3f;       // 見直したときに、弾を見落とす確率

    // 中・重の型
    constexpr float GunshipKeepDistance = 560.0f; // 基本型：コアからこの距離の円を旋回する
    constexpr float GunshipShotInterval = 1.0f;   // 基本型：弾を撃つ間隔（重は3発を扇形に）
    constexpr int HeavyShotCount = 3;
    constexpr float HeavyShotSpreadDegrees = 12.0f;
    constexpr float TorpedoKeepDistance = 600.0f; // 雷撃型
    constexpr float TorpedoInterval = 4.0f;       // 雷撃型：敵の誘導弾を撃つ間隔（重は3発を扇形に）
    constexpr float CarrierKeepDistance = 640.0f; // 空母型
    constexpr float CarrierInterval = 6.0f;       // 空母型：軽の敵を生み出す間隔
    constexpr int CarrierLaunchCount = 3;         // 空母型（中）が1回に生み出す数と、同時に出ていられる数
    constexpr int CarrierMaxChildren = 9;
    constexpr int HeavyCarrierLaunchCount = 5;    // 空母型（重）
    constexpr int HeavyCarrierMaxChildren = 15;
    constexpr int RamDamage = 10;                 // 中・重がコアにぶつかったときのダメージ（ぶつかった敵は消える）

    // 敵の誘導弾（雷撃型が撃つ）。敵の弾と同じ扱い（HP1）で、コアを追いかける
    constexpr float EnemyMissileSpeed = 550.0f;
    constexpr float EnemyMissileTurnDegrees = 120.0f;
    constexpr float EnemyMissileLifeSeconds = 6.0f;
    constexpr float EnemyMissileRadius = 5.0f;
    constexpr int EnemyMissileDamage = 15;

    // 敵の弾が自分の攻撃で壊されたときの小さな爆発。自分の誘導弾の爆発と同じく、周りの敵本体（軽だけが倒れる）・敵の弾を
    // 巻き込み（連鎖する）、自分の誘導弾は誘爆させない
    constexpr float EnemyBulletBlastRadius = 18.0f;
    constexpr float EnemyBulletBlastSeconds = 0.12f;

    // マズルフラッシュ（撃った瞬間に、弾が出た位置が光る。見た目だけ）。半径はフィールド上の大きさ、時間は消えるまで
    constexpr float RapidFlashRadius = 5.0f;
    constexpr float RapidFlashSeconds = 0.04f;   // 撃つ間隔（最短0.05秒）より短くして、連射でも点滅して見えるように
    constexpr float BeamFlashRadius = 9.0f;
    constexpr float BeamFlashSeconds = 0.08f;
    constexpr float MissileFlashRadius = 7.0f;
    constexpr float MissileFlashSeconds = 0.10f;
    constexpr float ShellFlashRadius = 16.0f;
    constexpr float ShellFlashSeconds = 0.12f;

    // 煙の軌跡（自分の誘導弾と、敵の弾すべて。見た目だけ）。進んだ距離ごとに、尾の位置へ煙の粒を置く
    constexpr float SmokeSpacing = 30.0f;        // 煙の粒を置く間隔（進んだ距離）
    constexpr float SmokeSeconds = 0.6f;         // 煙の粒が薄くなって消えるまでの時間
    constexpr float SmokeRadiusStart = 3.0f;     // 置いたときの半径
    constexpr float SmokeRadiusEnd = 10.0f;      // 消えるときの半径
    constexpr int SmokeAlpha = 120;              // 置いたときの濃さ（0〜255）
    constexpr int SmokeMax = 4000;               // 煙の粒の数の上限（超える間は新しく置かない。描く処理が重くなりすぎないように）

    // 敵本体が消えるとき（倒れたとき・コアに体当たりしたとき）の爆発。見た目だけ（何も巻き込まない）
    constexpr float LightBlastRadius = 20.0f;   // 軽の爆発の半径
    constexpr float MediumBlastRadius = 30.0f;  // 中の爆発の半径
    constexpr float HeavyBlastRadius = 45.0f;   // 重の爆発の半径
    constexpr float EnemyBlastSeconds = 0.2f;

    // 現れ方。軽の群れと中の割合、中が艦隊になる確率は、最初の値から RampSeconds 秒かけて後の値へ変わる
    constexpr float RampSeconds = 120.0f;
    constexpr float LightGroupWeightStart = 6.0f;
    constexpr float LightGroupWeightEnd = 4.0f;
    constexpr float MediumWeightStart = 4.0f;
    constexpr float MediumWeightEnd = 6.0f;
    constexpr float FleetChanceStart = 0.1f;      // 中の基本型か雷撃型が現れるときに、艦隊になる確率
    constexpr float FleetChanceEnd = 0.3f;
    constexpr int FleetEscorts = 3;               // 艦隊の、旗艦（重）以外の中の数
    constexpr float GroupSpread = 18.0f;          // まとまって現れるときの、1体ごとの間隔（ドット）

    // 大艦隊：大艦隊がいない時間が一定続くと現れ、旗艦（重）を全部倒すと報酬を選べる
    constexpr float GrandFleetInterval = 60.0f;
    constexpr float GrandFleetWarningSeconds = 3.0f;
    constexpr int GrandFleetFlagships = 2;
    constexpr int GrandFleetEscorts = 6;

    // ゲームオーバーのあと、やり直しの操作を受け付けるまでの時間（秒）
    constexpr float RestartDelay = 0.5f;
}
