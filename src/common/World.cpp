#include "pch.h"
#include "World.h"
#include <DxLib.h>
#include "Config.h"
#include "EnemyAI.h"

namespace {
    bool Overlaps(const Vec2& a, float ra, const Vec2& b, float rb) {
        const float r = ra + rb;
        return (a - b).LengthSq() <= r * r;
    }

    // 画面の外へこれだけ出た弾は消す
    constexpr float BulletCullMargin = 40.0f;

    bool IsFarOutside(const Vec2& p) {
        return p.x < -BulletCullMargin || p.y < -BulletCullMargin ||
               p.x > Config::ScreenWidth + BulletCullMargin || p.y > Config::ScreenHeight + BulletCullMargin;
    }

    template <typename T>
    void EraseDead(std::vector<T>& items) {
        items.erase(std::remove_if(items.begin(), items.end(), [](const T& item) { return !item.alive; }), items.end());
    }
}

World::World() : rng_(static_cast<unsigned int>(GetNowHiPerformanceCount())) {
    Reset();
}

void World::Reset() {
    core_.pos = Vec2(Config::ScreenWidth * 0.5f, Config::ScreenHeight * 0.5f);
    core_.radius = Config::CoreRadius;
    core_.maxHp = Config::CoreMaxHp;
    core_.hp = core_.maxHp;
    rapidGun_.Reset();
    enemies_.clear();
    playerBullets_.clear();
    enemyBullets_.clear();
    elapsed_ = 0.0f;
    spawnTimer_ = 0.0f;
    gameOver_ = false;
    gameOverSeconds_ = 0.0f;
}

float World::RandomRange(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_);
}

void World::FirePlayerBullet(const Vec2& pos, const Vec2& vel, float radius, int damage) {
    Bullet b;
    b.pos = pos;
    b.vel = vel;
    b.radius = radius;
    b.damage = damage;
    playerBullets_.push_back(b);
}

void World::FireEnemyBullet(const Vec2& pos, const Vec2& vel, float radius, int damage) {
    Bullet b;
    b.pos = pos;
    b.vel = vel;
    b.radius = radius;
    b.damage = damage;
    enemyBullets_.push_back(b);
}

void World::Update(float dtSeconds) {
    if (gameOver_) {
        gameOverSeconds_ += dtSeconds;
        return;
    }
    // 速い弾が敵をすり抜けないように、細かく分けて進める
    const int steps = std::max(1, static_cast<int>(std::ceil(dtSeconds / Config::StepSeconds)));
    const float dt = dtSeconds / steps;
    for (int i = 0; i < steps && !gameOver_; ++i) {
        Step(dt);
    }
}

void World::Step(float dt) {
    elapsed_ += dt;

    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0.0f) {
        SpawnEnemy();
        spawnTimer_ = std::max(Config::SpawnIntervalMin,
                               Config::SpawnIntervalStart - elapsed_ * Config::SpawnIntervalShrinkPerSecond);
    }

    rapidGun_.Update(*this, dt);

    // 敵が撃つと enemyBullets_ が増えるだけで enemies_ は変わらないので、参照のまま回してよい
    for (Enemy& e : enemies_) {
        UpdateEnemy(e, *this, dt);
    }

    MoveBullets(playerBullets_, dt);
    MoveBullets(enemyBullets_, dt);
    HandleCollisions();
    RemoveDead();

    if (core_.hp <= 0) {
        core_.hp = 0;
        gameOver_ = true;
        gameOverSeconds_ = 0.0f;
    }
}

void World::SpawnEnemy() {
    Enemy e;
    const int kind = std::uniform_int_distribution<int>(0, 2)(rng_);
    int baseHp = 0;
    switch (kind) {
    case 0:
        e.type = EnemyType::Charger;
        e.radius = Config::ChargerRadius;
        baseHp = Config::ChargerHp;
        break;
    case 1:
        e.type = EnemyType::Shooter;
        e.radius = Config::ShooterRadius;
        baseHp = Config::ShooterHp;
        e.shotTimer = RandomRange(0.5f, Config::ShooterShotInterval);
        break;
    default:
        e.type = EnemyType::Evader;
        e.radius = Config::EvaderRadius;
        baseHp = Config::EvaderHp;
        e.shotTimer = RandomRange(0.5f, Config::EvaderShotInterval);
        break;
    }
    // 時間が経つほどHPが増える
    const float hpScale = 1.0f + elapsed_ / Config::EnemyHpGrowthSeconds;
    e.maxHp = std::max(1, static_cast<int>(std::lround(baseHp * hpScale)));
    e.hp = e.maxHp;
    e.orbitDir = (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) ? 1.0f : -1.0f;

    // 画面の四辺のどこかの、少し外側に置く
    const float w = static_cast<float>(Config::ScreenWidth);
    const float h = static_cast<float>(Config::ScreenHeight);
    const float m = Config::SpawnMargin;
    const float t = RandomRange(0.0f, 2.0f * (w + h));
    if (t < w) {
        e.pos = Vec2(t, -m);
    } else if (t < w + h) {
        e.pos = Vec2(w + m, t - w);
    } else if (t < 2.0f * w + h) {
        e.pos = Vec2(t - (w + h), h + m);
    } else {
        e.pos = Vec2(-m, t - (2.0f * w + h));
    }
    enemies_.push_back(e);
}

void World::MoveBullets(std::vector<Bullet>& bullets, float dt) {
    for (Bullet& b : bullets) {
        b.pos += b.vel * dt;
        if (IsFarOutside(b.pos)) {
            b.alive = false;
        }
    }
}

void World::HandleCollisions() {
    // 自分の弾と敵の弾がぶつかると、両方消える
    for (Bullet& pb : playerBullets_) {
        if (!pb.alive) continue;
        for (Bullet& eb : enemyBullets_) {
            if (eb.alive && Overlaps(pb.pos, pb.radius, eb.pos, eb.radius)) {
                pb.alive = false;
                eb.alive = false;
                break;
            }
        }
    }

    // 自分の弾が敵本体に当たる（速射砲は貫かない）
    for (Bullet& pb : playerBullets_) {
        if (!pb.alive) continue;
        for (Enemy& e : enemies_) {
            if (e.alive && Overlaps(pb.pos, pb.radius, e.pos, e.radius)) {
                pb.alive = false;
                e.hp -= pb.damage;
                if (e.hp <= 0) {
                    e.alive = false;
                }
                break;
            }
        }
    }

    // 敵の弾がコアに当たる
    for (Bullet& eb : enemyBullets_) {
        if (eb.alive && Overlaps(eb.pos, eb.radius, core_.pos, core_.radius)) {
            eb.alive = false;
            core_.hp -= eb.damage;
        }
    }

    // 敵本体がコアにぶつかる（体当たり）。ぶつかった敵は消える
    for (Enemy& e : enemies_) {
        if (e.alive && Overlaps(e.pos, e.radius, core_.pos, core_.radius)) {
            e.alive = false;
            core_.hp -= Config::ChargerRamDamage;
        }
    }
}

void World::RemoveDead() {
    EraseDead(enemies_);
    EraseDead(playerBullets_);
    EraseDead(enemyBullets_);
}
