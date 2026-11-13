#include "pch.h"
#include "EnemyAI.h"
#include "Config.h"
#include "World.h"

// 敵は航空機のように動く。毎フレーム「行きたい向き」を決め、機首をそちらへ少しずつ回して、機首の向きへ進む。
// 向きを回せる速さと、速さを変えられる量には上限があるので、急に止まったり向きを変えたりできない

namespace {
    constexpr float Pi = 3.14159265f;

    float DegreesToRadians(float degrees) { return degrees * Pi / 180.0f; }

    // 角度の差を −π〜π に収める
    float WrapAngle(float a) {
        while (a > Pi) a -= 2.0f * Pi;
        while (a < -Pi) a += 2.0f * Pi;
        return a;
    }

    // コアから keepDistance の距離の円に沿って回るための向き（遠ければ内側へ、近すぎれば外側へ寄せる）
    Vec2 OrbitDirection(const Enemy& e, const Vec2& corePos, float keepDistance) {
        const Vec2 toCore = corePos - e.pos;
        const float dist = toCore.Length();
        const Vec2 inward = toCore.Normalized();
        const float radial = std::max(-1.0f, std::min(1.0f, (dist - keepDistance) / 50.0f));
        const Vec2 tangent = inward.Perpendicular() * e.orbitDir;
        return (inward * radial + tangent * 0.6f).Normalized();
    }

    // コアに向けて弾を撃つ（撃てる距離にいるときだけ）。撃つ向きは機首の向きとは関係ない
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
            // 自分も動いているので、弾から見た自分の動きで考える
            const Vec2 relVel = b.vel - e.vel;
            const float relSpeedSq = relVel.LengthSq();
            if (relSpeedSq <= 0.0f) continue;
            const Vec2 rel = e.pos - b.pos;
            // 弾が最も近づくまでの時間
            const float t = rel.Dot(relVel) / relSpeedSq;
            if (t <= 0.0f || t > Config::EvaderLookAheadSeconds) continue;
            // 最も近づいたときの、弾から見た自分の位置
            const Vec2 closest = rel - relVel * t;
            const float limit = e.radius + b.radius + Config::EvaderDodgeMargin;
            const float dist = closest.Length();
            if (dist >= limit) continue;
            // 進路の真上にいるときは、進路に対して横へ逃げる
            const Vec2 away = dist > 0.001f ? closest * (1.0f / dist) : relVel.Perpendicular().Normalized();
            // 早く届く弾、進路の中心に近い弾ほど強く避ける
            const float weight = (1.0f - t / Config::EvaderLookAheadSeconds) * (1.0f - dist / limit);
            push += away * weight;
        }
        return push.Normalized();
    }

    // 一度画面に入った敵は、端に近づいたら行きたい向きを内側へ寄せる（近いほど強く）
    Vec2 AvoidEdges(const Enemy& e, const Vec2& desired) {
        if (!e.entered) {
            return desired;
        }
        const float m = Config::EnemyEdgeMargin;
        Vec2 inward;
        if (e.pos.x < m) inward.x += (m - e.pos.x) / m;
        if (e.pos.x > Config::ScreenWidth - m) inward.x -= (e.pos.x - (Config::ScreenWidth - m)) / m;
        if (e.pos.y < m) inward.y += (m - e.pos.y) / m;
        if (e.pos.y > Config::ScreenHeight - m) inward.y -= (e.pos.y - (Config::ScreenHeight - m)) / m;
        if (inward.LengthSq() <= 0.0f) {
            return desired;
        }
        return (desired + inward * 3.0f).Normalized();
    }

    // 機首を行きたい向きへ回し（1秒間に turnDegrees まで）、速さを目標の速さへ近づける
    void Steer(Enemy& e, const Vec2& desired, float turnDegrees, float targetSpeed, float dt) {
        if (desired.LengthSq() > 0.0f) {
            const float diff = WrapAngle(std::atan2(desired.y, desired.x) - e.heading);
            const float maxTurn = DegreesToRadians(turnDegrees) * dt;
            e.heading = WrapAngle(e.heading + std::max(-maxTurn, std::min(maxTurn, diff)));
        }
        const float maxAccel = Config::EnemyAcceleration * dt;
        e.speed += std::max(-maxAccel, std::min(maxAccel, targetSpeed - e.speed));
        e.vel = Vec2::FromAngle(e.heading) * e.speed;
    }

    void UpdateCharger(Enemy& e, World& world, float dt) {
        const Vec2 desired = AvoidEdges(e, (world.GetCore().pos - e.pos).Normalized());
        Steer(e, desired, Config::ChargerTurnDegrees, Config::ChargerSpeed, dt);
    }

    void UpdateShooter(Enemy& e, World& world, float dt) {
        const Vec2 desired = AvoidEdges(e, OrbitDirection(e, world.GetCore().pos, Config::ShooterKeepDistance));
        Steer(e, desired, Config::ShooterTurnDegrees, Config::ShooterSpeed, dt);
        ShootAtCore(e, world, dt, Config::ShooterShotInterval, Config::ShooterKeepDistance + 80.0f);
    }

    void UpdateEvader(Enemy& e, World& world, float dt) {
        // 弾を見直すのは、ばらつきのある間隔ごと。たまに見落とす
        e.reactionTimer -= dt;
        if (e.reactionTimer <= 0.0f) {
            e.reactionTimer = world.RandomRange(Config::EvaderReactionMin, Config::EvaderReactionMax);
            if (world.RandomRange(0.0f, 1.0f) < Config::EvaderMissChance) {
                e.dodgeDir = Vec2();
            } else {
                e.dodgeDir = DodgeDirection(e, world);
            }
        }

        Vec2 desired;
        float targetSpeed = Config::EvaderSpeed;
        if (e.dodgeDir.LengthSq() > 0.0f) {
            // 逃げたい側へ機体を傾けて、真横へ向かって旋回しながら加速する
            const Vec2 forward = Vec2::FromAngle(e.heading);
            const Vec2 left = forward.Perpendicular();
            desired = (e.dodgeDir.Dot(left) >= 0.0f) ? left : left * -1.0f;
            targetSpeed = Config::EvaderDodgeSpeed;
        } else {
            desired = OrbitDirection(e, world.GetCore().pos, Config::EvaderKeepDistance);
        }
        Steer(e, AvoidEdges(e, desired), Config::EvaderTurnDegrees, targetSpeed, dt);
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
        UpdateCharger(e, world, dt);
        break;
    case EnemyType::Shooter:
        UpdateShooter(e, world, dt);
        break;
    case EnemyType::Evader:
        UpdateEvader(e, world, dt);
        break;
    }

    e.pos += e.vel * dt;

    if (!e.entered && IsInsideScreen(e.pos, e.radius)) {
        e.entered = true;
    }
}
