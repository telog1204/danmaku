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
            if (t <= 0.0f || t > Config::DodgeLookAheadSeconds) continue;
            // 最も近づいたときの、弾から見た自分の位置
            const Vec2 closest = rel - relVel * t;
            const float limit = e.radius + b.radius + Config::DodgeMargin;
            const float dist = closest.Length();
            if (dist >= limit) continue;
            // 進路の真上にいるときは、進路に対して横へ逃げる
            const Vec2 away = dist > 0.001f ? closest * (1.0f / dist) : relVel.Perpendicular().Normalized();
            // 早く届く弾、進路の中心に近い弾ほど強く避ける
            const float weight = (1.0f - t / Config::DodgeLookAheadSeconds) * (1.0f - dist / limit);
            push += away * weight;
        }
        return push.Normalized();
    }

    // 一度フィールドに入った敵は、端に近づいたら行きたい向きを内側へ寄せる（近いほど強く）。
    // まだ入っていない敵も、フィールドの外へ EnemyPreEntryLimit より離れたら、内側へ寄せる（外へ遠く出ないように）
    Vec2 AvoidEdges(const Enemy& e, const Vec2& desired) {
        const float m = Config::EnemyEdgeMargin;
        Vec2 inward;
        if (!e.entered) {
            const float l = Config::EnemyPreEntryLimit;
            if (e.pos.x < -l) inward.x += 1.0f + (-l - e.pos.x) / m;
            if (e.pos.x > Config::FieldWidth + l) inward.x -= 1.0f + (e.pos.x - (Config::FieldWidth + l)) / m;
            if (e.pos.y < -l) inward.y += 1.0f + (-l - e.pos.y) / m;
            if (e.pos.y > Config::FieldHeight + l) inward.y -= 1.0f + (e.pos.y - (Config::FieldHeight + l)) / m;
            if (inward.LengthSq() <= 0.0f) {
                return desired;
            }
            return (desired + inward * 3.0f).Normalized();
        }
        if (e.pos.x < m) inward.x += (m - e.pos.x) / m;
        if (e.pos.x > Config::FieldWidth - m) inward.x -= (e.pos.x - (Config::FieldWidth - m)) / m;
        if (e.pos.y < m) inward.y += (m - e.pos.y) / m;
        if (e.pos.y > Config::FieldHeight - m) inward.y -= (e.pos.y - (Config::FieldHeight - m)) / m;
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

    // 重の攻撃は、中より短い間隔で行う
    float AttackInterval(const Enemy& e, float mediumInterval) {
        return e.cls == EnemyClass::Heavy ? mediumInterval * Config::HeavyAttackIntervalRate : mediumInterval;
    }

    // 軽の敵：当たりそうな弾を見て、かわす向きを決める（見直すのはばらつきのある間隔ごと。たまに見落とす）
    void UpdateDodge(Enemy& e, World& world, float dt) {
        if (!e.entered) {
            e.dodgeDir = Vec2();  // フィールドに入るまでは避けずに、まっすぐ向かう（避けて外へ遠く出ないように）
            return;
        }
        e.reactionTimer -= dt;
        if (e.reactionTimer > 0.0f) {
            return;
        }
        e.reactionTimer = world.RandomRange(Config::DodgeReactionMin, Config::DodgeReactionMax);
        if (world.RandomRange(0.0f, 1.0f) < Config::DodgeMissChance) {
            e.dodgeDir = Vec2();
        } else {
            e.dodgeDir = DodgeDirection(e, world);
        }
    }

    // 軽の敵：かわす必要があれば、逃げたい側へ機体を傾けて真横へ旋回しながら加速する。かわす必要がなければ desired へ進む
    void SteerLight(Enemy& e, const Vec2& desired, float dt) {
        if (e.dodgeDir.LengthSq() > 0.0f) {
            const Vec2 left = Vec2::FromAngle(e.heading).Perpendicular();
            const Vec2 dodge = (e.dodgeDir.Dot(left) >= 0.0f) ? left : left * -1.0f;
            Steer(e, AvoidEdges(e, dodge), e.turnDegrees, Config::LightDodgeSpeed, dt);
        } else {
            Steer(e, AvoidEdges(e, desired), e.turnDegrees, e.cruiseSpeed, dt);
        }
    }

    // 離脱型（軽）：コアへ向かいながら弾を撃ち、近づいたら旋回して離れ、十分に離れたらまたコアへ向かう
    void UpdateStrafer(Enemy& e, World& world, float dt) {
        UpdateDodge(e, world, dt);
        const Vec2 toCore = world.GetCore().pos - e.pos;
        const float dist = toCore.Length();
        if (!e.leaving && dist < Config::StraferBreakDistance) {
            e.leaving = true;
        } else if (e.leaving && dist > Config::StraferReturnDistance) {
            e.leaving = false;
        }
        const Vec2 desired = e.leaving ? toCore.Normalized() * -1.0f : toCore.Normalized();
        SteerLight(e, desired, dt);
        if (!e.leaving) {
            ShootAtCore(e, world, dt, Config::StraferShotInterval, Config::StraferReturnDistance + 100.0f);
        }
    }

    // 自爆型（軽）：弾は撃たず、コアへ体当たりする
    void UpdateKamikaze(Enemy& e, World& world, float dt) {
        UpdateDodge(e, world, dt);
        SteerLight(e, (world.GetCore().pos - e.pos).Normalized(), dt);
    }

    // 中・重の型：コアから遠めの円を、ゆっくり旋回する
    void Orbit(Enemy& e, World& world, float keepDistance, float dt) {
        const Vec2 desired = AvoidEdges(e, OrbitDirection(e, world.GetCore().pos, keepDistance));
        Steer(e, desired, e.turnDegrees, e.cruiseSpeed, dt);
    }

    // 撃つ時間が来たら true（撃てる距離にいないときは待つ）
    bool AttackReady(Enemy& e, const World& world, float dt, float interval, float maxDistance) {
        e.shotTimer -= dt;
        if (e.shotTimer > 0.0f) {
            return false;
        }
        if ((world.GetCore().pos - e.pos).Length() > maxDistance) {
            e.shotTimer = 0.0f;
            return false;
        }
        e.shotTimer += interval;
        return true;
    }

    // コアの方向へ、count 発を扇形に（1発なら真っすぐ）
    float FanAngle(float center, int i, int count) {
        return center + DegreesToRadians(Config::HeavyShotSpreadDegrees) * (static_cast<float>(i) - (count - 1) * 0.5f);
    }

    // 基本型（中・重）：弾を撃ち続ける（重は3発を扇形に）
    void UpdateGunship(Enemy& e, World& world, float dt) {
        Orbit(e, world, Config::GunshipKeepDistance, dt);
        if (!AttackReady(e, world, dt, AttackInterval(e, Config::GunshipShotInterval), Config::GunshipKeepDistance + 120.0f)) {
            return;
        }
        const Vec2 toCore = world.GetCore().pos - e.pos;
        const float center = std::atan2(toCore.y, toCore.x);
        const int count = (e.cls == EnemyClass::Heavy) ? Config::HeavyShotCount : 1;
        for (int i = 0; i < count; ++i) {
            const Vec2 dir = Vec2::FromAngle(FanAngle(center, i, count));
            world.FireEnemyBullet(e.pos + dir * e.radius, dir * Config::EnemyBulletSpeed,
                                  Config::EnemyBulletRadius, Config::EnemyBulletDamage);
        }
    }

    // 雷撃型（中・重）：コアを追いかける敵の誘導弾を撃つ（重は3発を扇形に）
    void UpdateTorpedo(Enemy& e, World& world, float dt) {
        Orbit(e, world, Config::TorpedoKeepDistance, dt);
        if (!AttackReady(e, world, dt, AttackInterval(e, Config::TorpedoInterval), Config::TorpedoKeepDistance + 150.0f)) {
            return;
        }
        const Vec2 toCore = world.GetCore().pos - e.pos;
        const float center = std::atan2(toCore.y, toCore.x);
        const int count = (e.cls == EnemyClass::Heavy) ? Config::HeavyShotCount : 1;
        for (int i = 0; i < count; ++i) {
            const float angle = FanAngle(center, i, count);
            world.FireEnemyMissile(e.pos + Vec2::FromAngle(angle) * e.radius, angle);
        }
    }

    // 空母型（中・重）：軽の敵を生み出す（同時に出ていられる数に上限がある）
    void UpdateCarrier(Enemy& e, World& world, float dt) {
        Orbit(e, world, Config::CarrierKeepDistance, dt);
        if (!AttackReady(e, world, dt, AttackInterval(e, Config::CarrierInterval), 10000.0f)) {
            return;
        }
        const bool heavy = (e.cls == EnemyClass::Heavy);
        const int maxChildren = heavy ? Config::HeavyCarrierMaxChildren : Config::CarrierMaxChildren;
        const int launch = heavy ? Config::HeavyCarrierLaunchCount : Config::CarrierLaunchCount;
        const int room = maxChildren - world.CountChildren(e.id);
        if (room > 0) {
            world.LaunchLightGroup(e, std::min(launch, room));
        }
    }

    bool IsInsideField(const Vec2& p, float margin) {
        return p.x >= margin && p.y >= margin &&
               p.x <= Config::FieldWidth - margin && p.y <= Config::FieldHeight - margin;
    }
}

void UpdateEnemy(Enemy& e, World& world, float dt) {
    switch (e.type) {
    case EnemyType::Strafer:
        UpdateStrafer(e, world, dt);
        break;
    case EnemyType::Kamikaze:
        UpdateKamikaze(e, world, dt);
        break;
    case EnemyType::Gunship:
        UpdateGunship(e, world, dt);
        break;
    case EnemyType::Torpedo:
        UpdateTorpedo(e, world, dt);
        break;
    case EnemyType::Carrier:
        UpdateCarrier(e, world, dt);
        break;
    }

    e.pos += e.vel * dt;

    if (!e.entered && IsInsideField(e.pos, e.radius)) {
        e.entered = true;
    }
}
