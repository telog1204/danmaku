#include "pch.h"
#include "EnemyAI.h"
#include "Config.h"
#include "World.h"

namespace {
    // コアから keepDistance の距離を保ちながら、コアの周りを回り込む速さ
    Vec2 KeepDistanceVelocity(const Enemy& e, const Vec2& corePos, float keepDistance, float speed) {
        const Vec2 toCore = corePos - e.pos;
        const float dist = toCore.Length();
        const Vec2 inward = toCore.Normalized();
        // 遠ければ近づき、近すぎれば離れる。ちょうどよい距離に近いほど、回り込む動きが中心になる
        const float radial = std::max(-1.0f, std::min(1.0f, (dist - keepDistance) / 50.0f));
        const Vec2 tangent = inward.Perpendicular() * e.orbitDir;
        return (inward * radial + tangent * 0.6f).Normalized() * speed;
    }

    // コアに向けて弾を撃つ（撃てる距離にいるときだけ）
    void ShootAtCore(Enemy& e, World& world, float dt, float interval, float maxDistance) {
        e.shotTimer -= dt;
        if (e.shotTimer > 0.0f) {
            return;
        }
        const Vec2 toCore = world.GetCore().pos - e.pos;
        if (toCore.Length() > maxDistance) {
            e.shotTimer = 0.0f;
            return;
        }
        const Vec2 dir = toCore.Normalized();
        world.FireEnemyBullet(e.pos + dir * e.radius, dir * Config::EnemyBulletSpeed,
                              Config::EnemyBulletRadius, Config::EnemyBulletDamage);
        e.shotTimer += interval;
    }

    // 飛んでくる自分の弾の進路を先読みし、当たりそうな弾から離れる向きを返す（当たりそうな弾がなければ長さ0）
    Vec2 DodgeDirection(const Enemy& e, const World& world) {
        Vec2 push;
        for (const Bullet& b : world.PlayerBullets()) {
            const float speedSq = b.vel.LengthSq();
            if (speedSq <= 0.0f) continue;
            const Vec2 rel = e.pos - b.pos;
            // 弾が最も近づくまでの時間
            const float t = rel.Dot(b.vel) / speedSq;
            if (t <= 0.0f || t > Config::EvaderLookAheadSeconds) continue;
            // 最も近づいたときの、弾から見た自分の位置
            const Vec2 closest = rel - b.vel * t;
            const float limit = e.radius + b.radius + Config::EvaderDodgeMargin;
            const float dist = closest.Length();
            if (dist >= limit) continue;
            // 進路の真上にいるときは、進路に対して横へ逃げる
            const Vec2 away = dist > 0.001f ? closest * (1.0f / dist) : b.vel.Perpendicular().Normalized();
            // 早く届く弾、進路の中心に近い弾ほど強く避ける
            const float weight = (1.0f - t / Config::EvaderLookAheadSeconds) * (1.0f - dist / limit);
            push += away * weight;
        }
        return push.Normalized();
    }

    void UpdateCharger(Enemy& e, World& world) {
        e.vel = (world.GetCore().pos - e.pos).Normalized() * Config::ChargerSpeed;
    }

    void UpdateShooter(Enemy& e, World& world, float dt) {
        e.vel = KeepDistanceVelocity(e, world.GetCore().pos, Config::ShooterKeepDistance, Config::ShooterSpeed);
        ShootAtCore(e, world, dt, Config::ShooterShotInterval, Config::ShooterKeepDistance + 80.0f);
    }

    void UpdateEvader(Enemy& e, World& world, float dt) {
        // 弾を見直すのは、ばらつきのある間隔ごと。たまに見落とす
        e.reactionTimer -= dt;
        if (e.reactionTimer <= 0.0f) {
            e.reactionTimer = world.RandomRange(Config::EvaderReactionMin, Config::EvaderReactionMax);
            if (world.RandomRange(0.0f, 1.0f) < Config::EvaderMissChance) {
                e.dodgeVel = Vec2();
            } else {
                e.dodgeVel = DodgeDirection(e, world) * (Config::EvaderSpeed * Config::EvaderDodgeSpeedRate);
            }
        }

        const Vec2 base = KeepDistanceVelocity(e, world.GetCore().pos, Config::EvaderKeepDistance, Config::EvaderSpeed);
        // 避けているあいだは、ふだんの動きを弱める
        e.vel = (e.dodgeVel.LengthSq() > 0.0f) ? base * 0.3f + e.dodgeVel : base;
        ShootAtCore(e, world, dt, Config::EvaderShotInterval, Config::EvaderKeepDistance + 100.0f);
    }

    bool IsInsideScreen(const Vec2& p, float margin) {
        return p.x >= margin && p.y >= margin &&
               p.x <= Config::ScreenWidth - margin && p.y <= Config::ScreenHeight - margin;
    }
}

void UpdateEnemy(Enemy& e, World& world, float dt) {
    switch (e.type) {
    case EnemyType::Charger:
        UpdateCharger(e, world);
        break;
    case EnemyType::Shooter:
        UpdateShooter(e, world, dt);
        break;
    case EnemyType::Evader:
        UpdateEvader(e, world, dt);
        break;
    }

    e.pos += e.vel * dt;

    // 一度画面に入った敵は、画面の外へ出ていかないようにする（避けた勢いで逃げ切らないように）
    if (!e.entered && IsInsideScreen(e.pos, e.radius)) {
        e.entered = true;
    }
    if (e.entered) {
        e.pos.x = std::max(e.radius, std::min(Config::ScreenWidth - e.radius, e.pos.x));
        e.pos.y = std::max(e.radius, std::min(Config::ScreenHeight - e.radius, e.pos.y));
    }
}
