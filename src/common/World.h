#pragma once
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

// 弾（自分の弾と敵の弾で共通）
struct Bullet {
    Vec2 pos;
    Vec2 vel;
    float radius = 0.0f;
    int damage = 0;
    bool alive = true;
};

enum class EnemyType {
    Charger,  // 突撃型
    Shooter,  // 射撃型
    Evader,   // 回避型
};

struct Enemy {
    EnemyType type = EnemyType::Charger;
    Vec2 pos;
    Vec2 vel;
    float radius = 0.0f;
    int hp = 0;
    int maxHp = 0;
    bool alive = true;
    float heading = 0.0f;      // 機首の向き（ラジアン）。常にこの向きへ進む
    float speed = 0.0f;        // 今の速さ（ドット/秒）
    bool entered = false;      // 一度画面の中に入ったか（入ったあとは、画面の端で旋回して内側へ戻る）
    float shotTimer = 0.0f;    // 次に撃つまでの時間（秒）
    float orbitDir = 1.0f;     // コアの周りを回る向き（1：左回り、−1：右回り）
    float reactionTimer = 0.0f; // 回避型：次に弾を見直すまでの時間（秒）
    Vec2 dodgeDir;             // 回避型：当たりそうな弾から離れたい向き（当たりそうな弾がなければ長さ0）
};

// ゲームの中身（コア・敵・弾）をまとめて持ち、時間を進める
class World {
public:
    World();

    // 最初からやり直す
    void Reset();

    // ゲームの時間を dtSeconds 秒進める（ゲームオーバーのあいだは進めない）
    void Update(float dtSeconds);

    bool IsGameOver() const { return gameOver_; }
    float ElapsedSeconds() const { return elapsed_; }
    float GameOverSeconds() const { return gameOverSeconds_; }

    const Core& GetCore() const { return core_; }
    const std::vector<Enemy>& Enemies() const { return enemies_; }
    const std::vector<Bullet>& PlayerBullets() const { return playerBullets_; }
    const std::vector<Bullet>& EnemyBullets() const { return enemyBullets_; }

    // 武装や敵の動きから使うもの
    std::mt19937& Rng() { return rng_; }
    float RandomRange(float min, float max);
    void FirePlayerBullet(const Vec2& pos, const Vec2& vel, float radius, int damage);
    void FireEnemyBullet(const Vec2& pos, const Vec2& vel, float radius, int damage);

private:
    void Step(float dt);
    void SpawnEnemy();
    void MoveBullets(std::vector<Bullet>& bullets, float dt);
    void HandleCollisions();
    void RemoveDead();

    Core core_;
    RapidGun rapidGun_;
    std::vector<Enemy> enemies_;
    std::vector<Bullet> playerBullets_;
    std::vector<Bullet> enemyBullets_;
    std::mt19937 rng_;
    float elapsed_ = 0.0f;
    float spawnTimer_ = 0.0f;
    bool gameOver_ = false;
    float gameOverSeconds_ = 0.0f;  // ゲームオーバーになってからの時間（やり直しの受け付けに使う）
};
