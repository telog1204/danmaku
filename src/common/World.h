#pragma once
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Target.h"
#include "Vec2.h"
#include "Weapon.h"

// フィールド中央のコア。動かない
struct Core {
    Vec2 pos;
    float radius = 0.0f;
    int hp = 0;
    int maxHp = 0;
};

// 丸い弾（速射砲の弾・炸裂弾の破片・敵の弾）
struct Bullet {
    int id = 0;
    Vec2 pos;
    Vec2 vel;
    float radius = 0.0f;
    float damage = 0.0f;
    float life = -1.0f;  // 残りの飛ぶ時間（秒）。負ならフィールドの外へ出るまで飛ぶ
    bool homing = false; // 敵の誘導弾：コアを追いかける
    float pierceChance = 0.0f; // 当たったときに貫く確率（速射砲の貫徹力強化）
    int lastHitId = 0;   // 最後に貫いた相手の番号（同じ相手に続けて当たらないように）
    bool alive = true;
};

// ビーム。先端（pos）から後ろへ尾が伸び、尾を含めた線全体に当たり判定がある。貫くたびに威力が下がる
struct Beam {
    Vec2 pos;
    Vec2 vel;
    float traveled = 0.0f;   // 撃ってから進んだ距離（尾は、これより長くは伸びない）
    float damage = 0.0f;
    float decay = 0.0f;      // 1つ貫くごとに、威力がこの割合になる
    std::vector<int> hitIds; // もう貫いた敵の番号（同じ敵に何度も当たらないように）
    bool alive = true;

    Vec2 Tail() const;
};

// 誘導弾。撃ち出してまっすぐ進んだあと（近接防御型はすぐに）、前方に見えている相手を自分で選んで追いかける。
// 相手の選び方は、撃った武装に関係なくすべて同じ（World::AcquireMissileTarget）。
// 後ろ側は見えないので、相手を追い越すと見失い、前方の相手を選び直す
struct Missile {
    Vec2 pos;
    float heading = 0.0f;
    float age = 0.0f;
    float straightSeconds = 0.0f; // 撃ち出してからまっすぐ進む時間（この間は相手を選ばない）
    TargetKind targetKind = TargetKind::None;
    int targetId = 0;
    float damage = 0.0f;
    float turnDegrees = 0.0f;   // 1秒間に曲がれる角度
    float blastRadius = 0.0f;   // 消えるときの爆発の半径
    bool alive = true;
};

// 炸裂弾。狙った位置まで飛ぶか、途中で敵に当たると炸裂する
struct Shell {
    Vec2 pos;
    Vec2 vel;
    float distanceLeft = 0.0f;  // 炸裂する位置までの残りの距離
    float damageScale = 1.0f;   // 直撃・爆炎・破片の威力の倍率
    float flameRadius = 0.0f;
    float flameSeconds = 0.0f;
    int fragmentCount = 0;
    bool alive = true;
};

// 手に入るもの。敵を倒した瞬間（と大艦隊の報酬）に手に入る
enum class PickupKind {
    Energy,  // エネルギー上限解放パーツ
    Weapon,  // 武装
    Part,    // 強化パーツ
};

struct Pickup {
    PickupKind kind = PickupKind::Energy;
    WeaponKind weapon;                             // 武装のとき
    WeaponFamily family = WeaponFamily::RapidGun;  // 武装・強化パーツの系統
    PartKind part = PartKind::Power;               // 強化パーツのとき
};

// 拾ったものを画面の上に出す知らせ
struct Message {
    std::string text;
    PickupKind kind = PickupKind::Energy;
    float life = 0.0f;
};

// 爆発・爆炎。残っているあいだ、触れた敵（1体につき1回）・敵の弾・誘導弾を巻き込む
struct Explosion {
    Vec2 pos;
    float radius = 0.0f;
    float damage = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    std::vector<int> hitIds;
    bool alive = true;
};

// 敵の区分。大きさ・HP・速さ・防御力などが決まる
enum class EnemyClass {
    Light,   // 軽：どの攻撃でも1発で倒せる。速く、まとまって現れ、弾をかわす
    Medium,  // 中：遅く、防御力がある
    Heavy,   // 重：とても遅く、硬い。艦隊の旗艦としてだけ現れる
};

// 敵の型。動き方と攻撃の仕方が決まる（軽は離脱型・自爆型、中・重は基本型・雷撃型・空母型）
enum class EnemyType {
    Strafer,   // 離脱型（軽）：コアへ向かいながら撃ち、近づくと離れて、また向かってくる
    Kamikaze,  // 自爆型（軽）：コアへ体当たりする
    Gunship,   // 基本型（中・重）：遠めの円を旋回し、弾を撃ち続ける
    Torpedo,   // 雷撃型（中・重）：遠めの円を旋回し、敵の誘導弾を撃つ
    Carrier,   // 空母型（中・重）：遠めの円を旋回し、軽の敵を生み出す
};

struct Enemy {
    int id = 0;
    EnemyClass cls = EnemyClass::Medium;
    EnemyType type = EnemyType::Gunship;
    float defense = 0.0f;      // 防御力（受けるダメージ＝攻撃力−防御力。最低でも1割は通る）
    float cruiseSpeed = 0.0f;  // ふだんの速さ
    float turnDegrees = 0.0f;  // 1秒間に曲がれる角度
    bool grandFlagship = false; // 大艦隊の旗艦（全部倒すと報酬を選べる）
    int parentId = 0;          // 空母型から生み出された軽の敵なら、その空母型の番号
    bool leaving = false;      // 離脱型：コアから離れている途中か
    Vec2 pos;
    Vec2 vel;
    float radius = 0.0f;
    float hp = 0.0f;
    float maxHp = 0.0f;
    bool alive = true;
    float heading = 0.0f;      // 機首の向き（ラジアン）。常にこの向きへ進む
    float speed = 0.0f;        // 今の速さ（ドット/秒）
    bool entered = false;      // 一度フィールドの中に入ったか（入ったあとは、フィールドの端で旋回して内側へ戻る）
    float shotTimer = 0.0f;    // 次に撃つまでの時間（秒）
    float orbitDir = 1.0f;     // コアの周りを回る向き（1：左回り、−1：右回り）
    float reactionTimer = 0.0f; // 軽：次に弾を見直すまでの時間（秒）
    Vec2 dodgeDir;             // 軽：当たりそうな弾から離れたい向き（当たりそうな弾がなければ長さ0）
};

// ゲームの中身（コア・敵・弾）をまとめて持ち、時間を進める
class World {
public:
    World();

    // 最初からやり直す。開始時に持つ武装（速射砲に加える1つ）はランダムに選ぶ
    void Reset();

    // ゲームの時間を dtSeconds 秒進める（ゲームオーバーのあいだは進めない）
    void Update(float dtSeconds);

    bool IsGameOver() const { return gameOver_; }
    float ElapsedSeconds() const { return elapsed_; }
    float GameOverSeconds() const { return gameOverSeconds_; }
    int Kills() const { return kills_; }          // 倒した敵の数（強敵と、体当たりで消えた敵は含めない）
    int FleetKills() const { return fleetKills_; }  // 倒した大艦隊の数（旗艦を全部倒した回数）

    const Core& GetCore() const { return core_; }
    const std::vector<Enemy>& Enemies() const { return enemies_; }
    const std::vector<Bullet>& PlayerBullets() const { return playerBullets_; }
    const std::vector<Bullet>& EnemyBullets() const { return enemyBullets_; }
    const std::vector<Beam>& Beams() const { return beams_; }
    const std::vector<Missile>& Missiles() const { return missiles_; }
    const std::vector<Shell>& Shells() const { return shells_; }
    const std::vector<Explosion>& Explosions() const { return explosions_; }
    const std::vector<std::unique_ptr<Weapon>>& Weapons() const { return weapons_; }
    const std::vector<Message>& Messages() const { return messages_; }

    // 組み込み画面から使うもの
    const std::vector<WeaponKind>& OwnedWeapons() const { return ownedWeapons_; }
    int EnergyCap() const { return energyCap_; }
    int UsedEnergy() const;
    bool CanEquip(WeaponKind kind) const;  // エネルギーが足りるか
    void Equip(WeaponKind kind);           // 1つ積む（足りなければ何もしない）
    void Unequip(size_t index);            // 積んでいる武装の index 番目を外す（付けていた強化パーツは手元に戻る）
    int SpareParts(WeaponFamily family, PartKind part) const;  // 持っている（付けていない）強化パーツの数
    bool CanAttachPart(size_t weaponIndex, PartKind part) const; // 強化パーツを持っていて、エネルギーが足りるか
    void AttachPart(size_t weaponIndex, PartKind part);
    void DetachPart(size_t weaponIndex, PartKind part);

    // 拾ったものを手に入れる（落とし物を拾ったときに呼ぶ。試験からも使う）
    void Collect(const Pickup& pickup);

    // 大艦隊
    bool IsGrandFleetActive() const;             // 大艦隊の旗艦が残っている
    bool IsGrandFleetWarning() const;            // もうすぐ大艦隊が現れる（「大艦隊接近」を出す）
    float GrandFleetHpRatio() const;             // 大艦隊の旗艦の残りHPの割合（いなければ0）
    bool HasPendingReward() const { return !rewardChoices_.empty(); }  // 大艦隊を倒して、報酬を選ぶのを待っている
    const std::vector<Pickup>& RewardChoices() const { return rewardChoices_; }
    void ChooseReward(size_t index);             // 報酬を1つ選んで手に入れる
    void SpawnGrandFleet();                      // 大艦隊を出す（時間で現れるほか、試験からも使う）

    // 敵を出す（現れ方の処理と、空母型・試験から使う）
    Enemy& SpawnEnemyAt(EnemyClass cls, EnemyType type, const Vec2& pos);
    void LaunchLightGroup(const Enemy& carrier, int count);  // 空母型が軽の敵を生み出す
    int CountChildren(int parentId) const;                   // 空母型から生み出されて、まだいる軽の敵の数
    void FireEnemyMissile(const Vec2& pos, float heading);   // 雷撃型が敵の誘導弾を撃つ

    // 武装や敵の動きから使うもの
    std::mt19937& Rng() { return rng_; }
    float RandomRange(float min, float max);

    // 狙う相手を探す。敵本体を優先して from に最も近いものを選び、本体がいないときだけ最も近い敵の弾を選ぶ
    Target FindNearestTarget(const Vec2& from) const;
    // 敵本体と敵の弾を区別せずに、from から maxRange 以内で最も近いものを探す（近接防御型）
    Target FindNearestAny(const Vec2& from, float maxRange) const;
    // 最大HPが最も高い敵本体を探す（同じなら from に近い方。敵本体がいなければ、from に最も近い敵の弾）。
    // exclude にある相手は除く（除くと敵本体がいなくなるときは、除かずに選ぶ）。
    // mediumFirst なら中・重から選び、中・重がいなければ軽から選ぶ（ビーム砲）。
    // keep（今狙っている相手）が、選んだ相手と同じ最大HPで同じ区分にいるなら、keep を選び続ける（狙いがふらつかないように）
    Target FindHighestHpTarget(const Vec2& from, const std::vector<Target>& exclude = {}, bool mediumFirst = false,
                               const Target& keep = Target()) const;
    // ビーム砲の攻撃型：from から撃ったときに、ビームの線の上に乗る相手が最も多い敵本体を探す
    // （敵本体は1、敵の弾は BeamPierceBulletWeight として数え、同じなら近い方）。中・重から選び、いなければ軽から選ぶ。
    // exclude にある相手は除く（除くと候補がなくなるときは、除かずに選ぶ）。敵本体がいなければ、最も近い敵の弾
    Target FindPierceTarget(const Vec2& from, const std::vector<Target>& exclude = {}) const;
    // from に近い順に、最大 count 個の相手を並べる（maxRange より遠いものは除く）。
    // bodiesFirst なら敵本体だけを並べ、本体がいないときだけ敵の弾を並べる。そうでなければ本体と弾を区別しない
    // accept を渡したときは、それが true を返す相手だけを並べる
    std::vector<Target> FindNearestCandidates(const Vec2& from, float maxRange, bool bodiesFirst, size_t count,
                                              const std::function<bool(const Target&)>& accept = nullptr) const;
    // ロックオンする相手を選ぶ。priority の順に区分を見て、相手がいる最初の区分の中から選ぶ。
    // keepAimFacing を渡したとき（砲塔を持つ武装が、狙っていた相手を失ったとき）は、まずその向きから
    // TurretKeepAimDegrees 以内にいる相手のうち角度の差が最も小さいもの（旋回せずに狙える相手）を選ぶ。
    // いなければ、近い順に3番目までの中からランダムに1つ選ぶ。
    // exclude にある相手は候補から外す（外すと候補がなくなるときは、外さずに選ぶ）。だれもいなければ kind が None
    Target PickLockOnTarget(const Vec2& from, float maxRange, TargetPriority priority, const std::vector<Target>& exclude = {},
                            const float* keepAimFacing = nullptr);
    // self と同じ種類（派生版が同じ。連装の段階は区別しない）のほかの武装が、今狙っている相手
    std::vector<Target> TargetsOfSameKind(const Weapon* self) const;
    // 番号で相手を探す（もういなければ kind が None になる）
    Target FindTargetById(TargetKind kind, int id) const;
    // 敵が多い場所の中心（敵本体がいなければ false）
    bool FindCrowdCenter(Vec2& center) const;
    // 軽の敵と敵の弾が多い場所の中心（どちらもいなければ false）。速射砲の攻撃型が使う。
    // 敵の弾は数が多く重くなるので、ゲームの時間で CrowdRefreshSeconds ごとに数え直し、その間は前の結果を使う
    bool FindLightCrowdCenter(Vec2& center);

    void AddPlayerBullet(const Bullet& b) { playerBullets_.push_back(b); }
    void FireEnemyBullet(const Vec2& pos, const Vec2& vel, float radius, float damage);
    void AddBeam(const Beam& b) { beams_.push_back(b); }
    void AddMissile(const Missile& m) { missiles_.push_back(m); }
    void AddShell(const Shell& s) { shells_.push_back(s); }

private:
    void Step(float dt);
    void SpawnEnemy();
    void MoveBullets(std::vector<Bullet>& bullets, float dt);
    void MoveBeams(float dt);
    void MoveMissiles(float dt);
    void MoveShells(float dt);
    void AgeExplosions(float dt);
    Target AcquireMissileTarget(const Missile& m);  // 誘導弾が、前方に見えている相手から狙う相手を選ぶ
    bool MissileCanSee(const Missile& m, const Vec2& pos) const;
    void AgeMessages(float dt);
    void DropItems(const Enemy& e);
    std::vector<WeaponKind> WeaponCandidates() const;  // 落ちる候補になる武装（すべての単装＋持っている武装の1つ上の連装）
    bool Owns(const WeaponKind& kind) const;
    void SortOwnedWeapons();  // 持っている武装を、系統 → 派生版 → 連装の順に並べる
    void MakeRewardChoices();
    void AddMessage(const std::string& text, PickupKind kind);
    void HandleCollisions();
    void DamageEnemy(Enemy& e, float damage);
    void DestroyEnemyBullet(Bullet& b);  // 敵の弾を自分の攻撃で壊す（小さな爆発を起こす）
    void MoveEnemyBullets(float dt);
    void SpawnGroupAtEdge(EnemyClass cls, const std::vector<EnemyType>& types);
    Vec2 RandomEdgePoint(Vec2& outward);
    void CheckGrandFleetDefeated();
    void DetonateMissile(Missile& m);
    void BurstShell(Shell& s);
    void AddExplosion(const Vec2& pos, float radius, float damage, float seconds);
    bool HitWithBullet(Bullet& b, int targetId);  // 速射砲の弾が当たったとき。貫いて進むなら true
    void RemoveDead();

    Core core_;
    std::vector<std::unique_ptr<Weapon>> weapons_;  // 積んでいる武装
    std::vector<WeaponKind> ownedWeapons_;          // 持っている武装（種類ごとに1つ）
    int spareParts_[WeaponFamilyCount][PartKindCount] = {};  // 持っている（付けていない）強化パーツの数
    int energyCap_ = 0;
    int exp_ = 0;      // 見えない経験値
    int expNeed_ = 0;  // 次のエネルギー上限解放パーツまでに必要な経験値
    std::vector<Enemy> enemies_;
    std::vector<Bullet> playerBullets_;
    std::vector<Bullet> enemyBullets_;
    std::vector<Beam> beams_;
    std::vector<Missile> missiles_;
    std::vector<Shell> shells_;
    std::vector<Explosion> explosions_;
    std::vector<Explosion> pendingExplosions_;  // 当たり判定の途中で起きた爆発（判定が終わってから加える）
    std::vector<Message> messages_;
    std::mt19937 rng_;
    int nextId_ = 1;
    float elapsed_ = 0.0f;
    // FindLightCrowdCenter の前の結果（数え直した時刻。負ならまだ数えていない）
    float lightCrowdTime_ = -1.0f;
    bool lightCrowdFound_ = false;
    Vec2 lightCrowdCenter_;
    float spawnTimer_ = 0.0f;
    float grandFleetTimer_ = 0.0f;  // 次の大艦隊が現れるまでの時間（大艦隊がいない間だけ減る）
    std::vector<Enemy> pendingEnemies_;  // 敵の動きの途中で生み出された敵（動きの処理が終わってから加える）
    int kills_ = 0;
    int fleetKills_ = 0;  // 次の強敵が現れるまでの時間（強敵がいない間だけ減る）
    std::vector<Pickup> rewardChoices_;  // 強敵を倒したときの報酬の候補（選ぶまで空でない）
    bool gameOver_ = false;
    float gameOverSeconds_ = 0.0f;  // ゲームオーバーになってからの時間（やり直しの受け付けに使う）
};
