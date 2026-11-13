#include "pch.h"
#include "Weapon.h"
#include "Config.h"
#include "World.h"

namespace {
    constexpr float Pi = 3.14159265f;

    // 動いている相手に弾を当てるために、弾が届くまでの時間を求めて、その時の相手の位置を返す（先読みの狙い）
    Vec2 LeadTarget(const Vec2& origin, const Vec2& targetPos, const Vec2& targetVel, float bulletSpeed) {
        const Vec2 rel = targetPos - origin;
        // |rel + targetVel * t| = bulletSpeed * t を t について解く
        const float a = targetVel.LengthSq() - bulletSpeed * bulletSpeed;
        const float b = 2.0f * rel.Dot(targetVel);
        const float c = rel.LengthSq();
        float t = -1.0f;
        if (std::fabs(a) < 1e-4f) {
            if (std::fabs(b) > 1e-4f) t = -c / b;
        } else {
            const float disc = b * b - 4.0f * a * c;
            if (disc >= 0.0f) {
                const float sq = std::sqrt(disc);
                const float t1 = (-b - sq) / (2.0f * a);
                const float t2 = (-b + sq) / (2.0f * a);
                if (t1 > 0.0f && (t2 <= 0.0f || t1 < t2)) t = t1;
                else if (t2 > 0.0f) t = t2;
            }
        }
        if (t <= 0.0f) {
            return targetPos;  // 追いつけないときは今の位置を狙う
        }
        return targetPos + targetVel * t;
    }
}

void RapidGun::Reset() {
    cooldown_ = 0.0f;
}

void RapidGun::Update(World& world, float dt) {
    cooldown_ -= dt;
    if (cooldown_ > 0.0f) {
        return;
    }

    const Vec2 origin = world.GetCore().pos;

    // 敵本体を優先して、最も近いものを狙う（相手の動きを先読みして、当たる位置へ撃つ）
    const Vec2* target = nullptr;
    const Vec2* targetVel = nullptr;
    float bestDistSq = 0.0f;
    for (const Enemy& e : world.Enemies()) {
        const float d = (e.pos - origin).LengthSq();
        if (target == nullptr || d < bestDistSq) {
            target = &e.pos;
            targetVel = &e.vel;
            bestDistSq = d;
        }
    }
    // 敵本体がいないときだけ、最も近い敵の弾を狙う
    if (target == nullptr) {
        for (const Bullet& b : world.EnemyBullets()) {
            const float d = (b.pos - origin).LengthSq();
            if (target == nullptr || d < bestDistSq) {
                target = &b.pos;
                targetVel = &b.vel;
                bestDistSq = d;
            }
        }
    }
    if (target == nullptr) {
        cooldown_ = 0.0f;  // 狙うものが現れたらすぐ撃てるようにしておく
        return;
    }

    const Vec2 aimPos = LeadTarget(origin, *target, *targetVel, Config::RapidGunBulletSpeed);
    const Vec2 toTarget = aimPos - origin;
    const float spread = Config::RapidGunSpreadDegrees * Pi / 180.0f;
    const float angle = std::atan2(toTarget.y, toTarget.x) + world.RandomRange(-spread, spread);
    const Vec2 dir = Vec2::FromAngle(angle);
    world.FirePlayerBullet(origin + dir * Config::CoreRadius, dir * Config::RapidGunBulletSpeed,
                           Config::PlayerBulletRadius, Config::RapidGunDamage);
    cooldown_ += Config::RapidGunInterval;
}
