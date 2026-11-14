#pragma once
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Vec2.h"
#include "Weapon.h"

// 画面中央のコア。動かない
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
    float life = -1.0f;  // 残りの飛ぶ時間（秒）。負なら画面の外へ出るまで飛ぶ
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

// 狙う相手の種類
enum class TargetKind {
    None,
    Enemy,
    EnemyBullet,
};

// 誘導弾。撃ち出して一瞬まっすぐ進んでから、狙った相手を追いかける
struct Missile {
    Vec2 pos;
    float heading = 0.0f;
    float age = 0.0f;
    TargetKind targetKind = TargetKind::None;
    int targetId = 0;
    float damage = 0.0f;
    float turnDegrees = 0.0f;   // 1秒間に曲がれる角度
    float blastRadius = 0.0f;   // 消えるときの爆発の半径
    bool targetsBullets = false; // 狙った相手がいなくなったとき、敵の弾も含めて最も近いものを狙い直すか
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

// 倒した敵が落とすもの。落ちた場所に少しとどまってから、コアへ吸い寄せられて自動で拾われる
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
    Vec2 pos;
    float age = 0.0f;
    float speed = 0.0f;
    bool alive = true;
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

enum class EnemyType {
    Charger,  // 突撃型
    Shooter,  // 射撃型
    Evader,   // 回避型
    Boss,     // 強敵
};

struct Enemy {
    int id = 0;
    EnemyType type = EnemyType::Charger;
    Vec2 pos;
    Vec2 vel;
    float radius = 0.0f;
    float hp = 0.0f;
    float maxHp = 0.0f;
    bool alive = true;
    float heading = 0.0f;      // 機首の向き（ラジアン）。常にこの向きへ進む
    float speed = 0.0f;        // 今の速さ（ドット/秒）
    bool entered = false;      // 一度画面の中に入ったか（入ったあとは、画面の端で旋回して内側へ戻る）
    float shotTimer = 0.0f;    // 次に撃つまでの時間（秒）
    float orbitDir = 1.0f;     // コアの周りを回る向き（1：左回り、−1：右回り）
    float reactionTimer = 0.0f; // 回避型：次に弾を見直すまでの時間（秒）
    Vec2 dodgeDir;             // 回避型：当たりそうな弾から離れたい向き（当たりそうな弾がなければ長さ0）
    float ramCooldown = 0.0f;  // 強敵：次に体当たりのダメージを与えるまでの時間
};

// 武装が狙う相手
struct Target {
    TargetKind kind = TargetKind::None;
    int id = 0;
    Vec2 pos;
    Vec2 vel;
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
    int BossKills() const { return bossKills_; }  // 倒した強敵の数

    const Core& GetCore() const { return core_; }
    const std::vector<Enemy>& Enemies() const { return enemies_; }
    const std::vector<Bullet>& PlayerBullets() const { return playerBullets_; }
    const std::vector<Bullet>& EnemyBullets() const { return enemyBullets_; }
    const std::vector<Beam>& Beams() const { return beams_; }
    const std::vector<Missile>& Missiles() const { return missiles_; }
    const std::vector<Shell>& Shells() const { return shells_; }
    const std::vector<Explosion>& Explosions() const { return explosions_; }
    const std::vector<std::unique_ptr<Weapon>>& Weapons() const { return weapons_; }
    const std::vector<Pickup>& Pickups() const { return pickups_; }
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

    // 強敵
    const Enemy* FindBoss() const;               // 今いる強敵（いなければ nullptr）
    bool IsBossWarning() const;                  // もうすぐ強敵が現れる（「強敵接近」を出す）
    bool HasPendingReward() const { return !rewardChoices_.empty(); }  // 強敵を倒して、報酬を選ぶのを待っている
    const std::vector<Pickup>& RewardChoices() const { return rewardChoices_; }
    void ChooseReward(size_t index);             // 報酬を1つ選んで手に入れる
    void SpawnBoss();                            // 強敵を出す（時間で現れるほか、試験からも使う）

    // 武装や敵の動きから使うもの
    std::mt19937& Rng() { return rng_; }
    float RandomRange(float min, float max);

    // 狙う相手を探す。敵本体を優先して from に最も近いものを選び、本体がいないときだけ最も近い敵の弾を選ぶ
    Target FindNearestTarget(const Vec2& from) const;
    // 敵本体と敵の弾を区別せずに、from から maxRange 以内で最も近いものを探す（近接防御型）
    Target FindNearestAny(const Vec2& from, float maxRange) const;
    // HPが最も高い敵本体を探す（敵本体がいなければ、from に最も近い敵の弾）
    Target FindHighestHpTarget(const Vec2& from) const;
    // 番号で相手を探す（もういなければ kind が None になる）
    Target FindTargetById(TargetKind kind, int id) const;
    // 敵が多い場所の中心（敵本体がいなければ false）
    bool FindCrowdCenter(Vec2& center) const;

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
    void MovePickups(float dt);
    void AgeMessages(float dt);
    void DropItems(const Enemy& e);
    std::vector<WeaponKind> WeaponCandidates() const;  // 落ちる候補になる武装（すべての単装＋持っている武装の1つ上の連装）
    bool Owns(const WeaponKind& kind) const;
    void MakeRewardChoices();
    void PlaceAtScreenEdge(Enemy& e);
    void AddMessage(const std::string& text, PickupKind kind);
    void HandleCollisions();
    void DamageEnemy(Enemy& e, float damage);
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
    std::vector<Pickup> pickups_;
    std::vector<Message> messages_;
    std::mt19937 rng_;
    int nextId_ = 1;
    float elapsed_ = 0.0f;
    float spawnTimer_ = 0.0f;
    float bossTimer_ = 0.0f;
    int kills_ = 0;
    int bossKills_ = 0;  // 次の強敵が現れるまでの時間（強敵がいない間だけ減る）
    std::vector<Pickup> rewardChoices_;  // 強敵を倒したときの報酬の候補（選ぶまで空でない）
    bool gameOver_ = false;
    float gameOverSeconds_ = 0.0f;  // ゲームオーバーになってからの時間（やり直しの受け付けに使う）
};
