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

    // 撃つ間隔を数える部分は、どの武装も同じ
    class TimedWeapon : public Weapon {
    public:
        TimedWeapon(float interval, float firstDelay) : interval_(interval), cooldown_(firstDelay) {}

        void Update(World& world, float dt) override {
            cooldown_ -= dt;
            if (cooldown_ > 0.0f) {
                return;
            }
            if (Fire(world)) {
                cooldown_ += interval_;
            } else {
                cooldown_ = 0.0f;  // 狙うものが現れたらすぐ撃てるようにしておく
            }
        }

    protected:
        // 撃てたら true を返す（狙うものがいなければ false）
        virtual bool Fire(World& world) = 0;

    private:
        float interval_;
        float cooldown_;
    };

    // 速射砲（基本型）。最も近い敵を先読みして狙い、ほんの少しばらけた弾を撃つ
    class RapidGun : public TimedWeapon {
    public:
        explicit RapidGun(float firstDelay) : TimedWeapon(Config::RapidGunInterval, firstDelay) {}
        WeaponKind Kind() const override { return WeaponKind::RapidGun; }

    protected:
        bool Fire(World& world) override {
            const Vec2 origin = world.GetCore().pos;
            const Target target = world.FindNearestTarget(origin);
            if (target.kind == TargetKind::None) {
                return false;
            }
            const Vec2 toTarget = LeadTarget(origin, target.pos, target.vel, Config::RapidGunBulletSpeed) - origin;
            const float spread = Config::RapidGunSpreadDegrees * Pi / 180.0f;
            const float angle = std::atan2(toTarget.y, toTarget.x) + world.RandomRange(-spread, spread);
            const Vec2 dir = Vec2::FromAngle(angle);
            world.FirePlayerBullet(origin + dir * Config::CoreRadius, dir * Config::RapidGunBulletSpeed,
                                   Config::PlayerBulletRadius, Config::RapidGunDamage);
            return true;
        }
    };

    // ビーム砲（基本型）。最も近い敵を先読みして狙い、敵を貫くビームを撃つ
    class BeamGun : public TimedWeapon {
    public:
        explicit BeamGun(float firstDelay) : TimedWeapon(Config::BeamInterval, firstDelay) {}
        WeaponKind Kind() const override { return WeaponKind::Beam; }

    protected:
        bool Fire(World& world) override {
            const Vec2 origin = world.GetCore().pos;
            const Target target = world.FindNearestTarget(origin);
            if (target.kind == TargetKind::None) {
                return false;
            }
            const Vec2 dir = (LeadTarget(origin, target.pos, target.vel, Config::BeamSpeed) - origin).Normalized();
            world.FireBeam(origin + dir * Config::CoreRadius, dir * Config::BeamSpeed, Config::BeamDamage);
            return true;
        }
    };

    // 誘導兵器（基本型）。最も近い敵の方向へ撃ち出し、追いかけさせる
    class MissileLauncher : public TimedWeapon {
    public:
        explicit MissileLauncher(float firstDelay) : TimedWeapon(Config::MissileInterval, firstDelay) {}
        WeaponKind Kind() const override { return WeaponKind::Missile; }

    protected:
        bool Fire(World& world) override {
            const Vec2 origin = world.GetCore().pos;
            const Target target = world.FindNearestTarget(origin);
            if (target.kind == TargetKind::None) {
                return false;
            }
            const Vec2 toTarget = target.pos - origin;
            const float heading = std::atan2(toTarget.y, toTarget.x);
            world.FireMissile(origin + Vec2::FromAngle(heading) * Config::CoreRadius, heading, target);
            return true;
        }
    };

    // 炸裂弾（基本型）。敵が多い場所の、中心の少し手前で炸裂させる
    class ShellCannon : public TimedWeapon {
    public:
        explicit ShellCannon(float firstDelay) : TimedWeapon(Config::ShellInterval, firstDelay) {}
        WeaponKind Kind() const override { return WeaponKind::Shell; }

    protected:
        bool Fire(World& world) override {
            const Vec2 origin = world.GetCore().pos;
            Vec2 center;
            if (!world.FindCrowdCenter(center)) {
                // 敵本体がいなければ、最も近い敵の弾のところで炸裂させる
                const Target target = world.FindNearestTarget(origin);
                if (target.kind == TargetKind::None) {
                    return false;
                }
                center = target.pos;
            }
            const Vec2 toCenter = center - origin;
            const Vec2 dir = toCenter.Normalized();
            if (dir.LengthSq() <= 0.0f) {
                return false;
            }
            const float distance = std::max(Config::CoreRadius + 10.0f, toCenter.Length() - Config::ShellShortDistance);
            world.FireShell(origin + dir * Config::CoreRadius, dir * Config::ShellSpeed, distance - Config::CoreRadius);
            return true;
        }
    };
}

const char* WeaponName(WeaponKind kind) {
    switch (kind) {
    case WeaponKind::RapidGun: return "速射砲 基本型";
    case WeaponKind::Beam: return "ビーム砲 基本型";
    case WeaponKind::Missile: return "誘導兵器 基本型";
    case WeaponKind::Shell: return "炸裂弾 基本型";
    }
    return "";
}

int WeaponEnergy(WeaponKind kind) {
    switch (kind) {
    case WeaponKind::RapidGun: return Config::RapidGunEnergy;
    case WeaponKind::Beam: return Config::BeamEnergy;
    case WeaponKind::Missile: return Config::MissileEnergy;
    case WeaponKind::Shell: return Config::ShellEnergy;
    }
    return 0;
}

std::unique_ptr<Weapon> CreateWeapon(WeaponKind kind, float firstDelay) {
    switch (kind) {
    case WeaponKind::RapidGun: return std::make_unique<RapidGun>(firstDelay);
    case WeaponKind::Beam: return std::make_unique<BeamGun>(firstDelay);
    case WeaponKind::Missile: return std::make_unique<MissileLauncher>(firstDelay);
    case WeaponKind::Shell: return std::make_unique<ShellCannon>(firstDelay);
    }
    return nullptr;
}
