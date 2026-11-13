#pragma once
#include <memory>
#include <random>
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
    bool alive = true;
};

// ビーム。先端（pos）から後ろへ尾が伸び、尾を含めた線全体に当たり判定がある。貫くたびに威力が下がる
struct Beam {
    Vec2 pos;
    Vec2 vel;
    float traveled = 0.0f;   // 撃ってから進んだ距離（尾は、これより長くは伸びない）
    float damage = 0.0f;
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
    bool alive = true;
};

// 炸裂弾。狙った位置まで飛ぶか、途中で敵に当たると炸裂する
struct Shell {
    Vec2 pos;
    Vec2 vel;
    float distanceLeft = 0.0f;  // 炸裂する位置までの残りの距離
    bool alive = true;
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

    // 開始時に持つ武装を次のものに切り替えて、最初からやり直す（組み込み画面ができるまでの試し用）
    void ResetWithNextStartWeapon();

    // ゲームの時間を dtSeconds 秒進める（ゲームオーバーのあいだは進めない）
    void Update(float dtSeconds);

    bool IsGameOver() const { return gameOver_; }
    float ElapsedSeconds() const { return elapsed_; }
    float GameOverSeconds() const { return gameOverSeconds_; }

    const Core& GetCore() const { return core_; }
    const std::vector<Enemy>& Enemies() const { return enemies_; }
    const std::vector<Bullet>& PlayerBullets() const { return playerBullets_; }
    const std::vector<Bullet>& EnemyBullets() const { return enemyBullets_; }
    const std::vector<Beam>& Beams() const { return beams_; }
    const std::vector<Missile>& Missiles() const { return missiles_; }
    const std::vector<Shell>& Shells() const { return shells_; }
    const std::vector<Explosion>& Explosions() const { return explosions_; }
    const std::vector<std::unique_ptr<Weapon>>& Weapons() const { return weapons_; }

    // 組み込み画面から使うもの
    const std::vector<WeaponKind>& OwnedWeapons() const { return ownedWeapons_; }
    int EnergyCap() const { return energyCap_; }
    int UsedEnergy() const;
    bool CanEquip(WeaponKind kind) const;  // エネルギーが足りるか
    void Equip(WeaponKind kind);           // 1つ積む（足りなければ何もしない）
    void Unequip(size_t index);            // 積んでいる武装の index 番目を外す

    // 武装や敵の動きから使うもの
    std::mt19937& Rng() { return rng_; }
    float RandomRange(float min, float max);

    // 狙う相手を探す。敵本体を優先して from に最も近いものを選び、本体がいないときだけ最も近い敵の弾を選ぶ
    Target FindNearestTarget(const Vec2& from) const;
    // 番号で相手を探す（もういなければ kind が None になる）
    Target FindTargetById(TargetKind kind, int id) const;
    // 敵が多い場所の中心（敵本体がいなければ false）
    bool FindCrowdCenter(Vec2& center) const;

    void FirePlayerBullet(const Vec2& pos, const Vec2& vel, float radius, float damage, float life = -1.0f);
    void FireEnemyBullet(const Vec2& pos, const Vec2& vel, float radius, float damage);
    void FireBeam(const Vec2& pos, const Vec2& vel, float damage);
    void FireMissile(const Vec2& pos, float heading, const Target& target);
    void FireShell(const Vec2& pos, const Vec2& vel, float distance);

private:
    void ResetWithStartWeapon(int startWeaponIndex);
    void Step(float dt);
    void SpawnEnemy();
    void MoveBullets(std::vector<Bullet>& bullets, float dt);
    void MoveBeams(float dt);
    void MoveMissiles(float dt);
    void MoveShells(float dt);
    void AgeExplosions(float dt);
    void HandleCollisions();
    void DamageEnemy(Enemy& e, float damage);
    void DetonateMissile(Missile& m);
    void BurstShell(Shell& s);
    void AddExplosion(const Vec2& pos, float radius, float damage, float seconds);
    void RemoveDead();

    Core core_;
    std::vector<std::unique_ptr<Weapon>> weapons_;  // 積んでいる武装
    std::vector<WeaponKind> ownedWeapons_;          // 持っている武装（種類ごとに1つ）
    int energyCap_ = 0;
    int startWeaponIndex_ = 0;
    std::vector<Enemy> enemies_;
    std::vector<Bullet> playerBullets_;
    std::vector<Bullet> enemyBullets_;
    std::vector<Beam> beams_;
    std::vector<Missile> missiles_;
    std::vector<Shell> shells_;
    std::vector<Explosion> explosions_;
    std::vector<Explosion> pendingExplosions_;  // 当たり判定の途中で起きた爆発（判定が終わってから加える）
    std::mt19937 rng_;
    int nextId_ = 1;
    float elapsed_ = 0.0f;
    float spawnTimer_ = 0.0f;
    bool gameOver_ = false;
    float gameOverSeconds_ = 0.0f;  // ゲームオーバーになってからの時間（やり直しの受け付けに使う）
};
