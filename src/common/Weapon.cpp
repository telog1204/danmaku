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

    // 撃つ間隔を数える部分は、どの武装も同じ。連射強化を付けるほど間隔が短くなる
    class TimedWeapon : public Weapon {
    public:
        TimedWeapon(float interval, float firstDelay) : interval_(interval), cooldown_(firstDelay) {}

        void Update(World& world, float dt) override {
            cooldown_ -= dt;
            if (cooldown_ > 0.0f) {
                return;
            }
            if (Fire(world)) {
                cooldown_ += interval_ * std::pow(Config::PartRateMultiplier, static_cast<float>(CountPart(PartKind::Rate)));
            } else {
                cooldown_ = 0.0f;  // 狙うものが現れたらすぐ撃てるようにしておく
            }
        }

    protected:
        // 撃てたら true を返す（狙うものがいなければ false）
        virtual bool Fire(World& world) = 0;

        // 威力強化を反映した倍率
        float PowerScale(float ratePerPart) const {
            return 1.0f + ratePerPart * static_cast<float>(CountPart(PartKind::Power));
        }

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
            const float spread = Config::RapidGunSpreadDegrees * Pi / 180.0f *
                                 std::pow(Config::RapidFocusMultiplier, static_cast<float>(CountPart(PartKind::Focus)));
            const float angle = std::atan2(toTarget.y, toTarget.x) + world.RandomRange(-spread, spread);
            const Vec2 dir = Vec2::FromAngle(angle);
            Bullet b;
            b.pos = origin + dir * Config::CoreRadius;
            b.vel = dir * Config::RapidGunBulletSpeed;
            b.radius = Config::PlayerBulletRadius;
            b.damage = Config::RapidGunDamage * PowerScale(Config::PartPowerRate);
            b.pierceChance = std::min(1.0f, Config::RapidPierceChancePerPart * static_cast<float>(CountPart(PartKind::Pierce)));
            world.AddPlayerBullet(b);
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
            Beam b;
            b.pos = origin + dir * Config::CoreRadius;
            b.vel = dir * Config::BeamSpeed;
            b.damage = Config::BeamDamage * PowerScale(Config::PartPowerRate);
            b.decay = std::min(Config::BeamDecayMax,
                               Config::BeamDecay + Config::BeamPierceDecayPerPart * static_cast<float>(CountPart(PartKind::Pierce)));
            world.AddBeam(b);
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
            Missile m;
            m.heading = std::atan2(toTarget.y, toTarget.x);
            m.pos = origin + Vec2::FromAngle(m.heading) * Config::CoreRadius;
            m.targetKind = target.kind;
            m.targetId = target.id;
            m.damage = Config::MissileDamage * PowerScale(Config::PartPowerRate);
            m.turnDegrees = Config::MissileTurnDegrees *
                            (1.0f + Config::MissileHomingRate * static_cast<float>(CountPart(PartKind::Homing)));
            m.blastRadius = Config::MissileExplosionRadius *
                            (1.0f + Config::MissileBlastRate * static_cast<float>(CountPart(PartKind::BlastRadius)));
            world.AddMissile(m);
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
            Shell s;
            s.pos = origin + dir * Config::CoreRadius;
            s.vel = dir * Config::ShellSpeed;
            s.distanceLeft = distance - Config::CoreRadius;
            s.damageScale = PowerScale(Config::ShellPartPowerRate);
            s.flameRadius = Config::ShellFlameRadius *
                            (1.0f + Config::ShellFlameRadiusRate * static_cast<float>(CountPart(PartKind::FlameRadius)));
            s.flameSeconds = Config::ShellFlameSeconds *
                             (1.0f + Config::ShellFlameDurationRate * static_cast<float>(CountPart(PartKind::FlameDuration)));
            s.fragmentCount = Config::ShellFragmentCount + Config::ShellFragmentsPerPart * CountPart(PartKind::Fragments);
            world.AddShell(s);
            return true;
        }
    };
}

bool Weapon::RemovePart(PartKind part) {
    for (size_t i = parts_.size(); i > 0; --i) {
        if (parts_[i - 1] == part) {
            parts_.erase(parts_.begin() + static_cast<std::ptrdiff_t>(i - 1));
            return true;
        }
    }
    return false;
}

int Weapon::CountPart(PartKind part) const {
    return static_cast<int>(std::count(parts_.begin(), parts_.end(), part));
}

int Weapon::Energy() const {
    const int n = static_cast<int>(parts_.size());
    return WeaponEnergy(Kind()) + n * (n + 1) / 2;
}

WeaponFamily FamilyOf(WeaponKind kind) {
    switch (kind) {
    case WeaponKind::RapidGun: return WeaponFamily::RapidGun;
    case WeaponKind::Beam: return WeaponFamily::Beam;
    case WeaponKind::Missile: return WeaponFamily::Missile;
    case WeaponKind::Shell: return WeaponFamily::Shell;
    }
    return WeaponFamily::RapidGun;
}

const std::vector<PartKind>& FamilyParts(WeaponFamily family) {
    static const std::vector<PartKind> rapid = { PartKind::Power, PartKind::Rate, PartKind::Focus, PartKind::Pierce };
    static const std::vector<PartKind> beam = { PartKind::Power, PartKind::Rate, PartKind::Pierce };
    static const std::vector<PartKind> missile = { PartKind::Power, PartKind::Rate, PartKind::Homing, PartKind::BlastRadius };
    static const std::vector<PartKind> shell = { PartKind::Power, PartKind::Rate, PartKind::FlameRadius,
                                                 PartKind::FlameDuration, PartKind::Fragments };
    switch (family) {
    case WeaponFamily::RapidGun: return rapid;
    case WeaponFamily::Beam: return beam;
    case WeaponFamily::Missile: return missile;
    case WeaponFamily::Shell: return shell;
    }
    return rapid;
}

const char* FamilyName(WeaponFamily family) {
    switch (family) {
    case WeaponFamily::RapidGun: return "速射砲";
    case WeaponFamily::Beam: return "ビーム砲";
    case WeaponFamily::Missile: return "誘導兵器";
    case WeaponFamily::Shell: return "炸裂弾";
    }
    return "";
}

const char* PartName(PartKind part) {
    switch (part) {
    case PartKind::Power: return "威力強化";
    case PartKind::Rate: return "連射強化";
    case PartKind::Focus: return "収束";
    case PartKind::Pierce: return "貫徹力強化";
    case PartKind::Homing: return "誘導力強化";
    case PartKind::BlastRadius: return "爆発範囲拡大";
    case PartKind::FlameRadius: return "爆炎範囲拡大";
    case PartKind::FlameDuration: return "爆炎持続延長";
    case PartKind::Fragments: return "破片増加";
    }
    return "";
}

const char* PartDescription(WeaponFamily family, PartKind part) {
    switch (part) {
    case PartKind::Power:
        return family == WeaponFamily::Shell ? "直撃・爆炎・破片の威力が上がる" : "威力が上がる";
    case PartKind::Rate: return "撃つ間隔が短くなる";
    case PartKind::Focus: return "弾のばらつきが小さくなる";
    case PartKind::Pierce:
        return family == WeaponFamily::Beam ? "貫くたびに下がる威力の幅が小さくなる"
                                            : "当たったとき、確率で貫いて先へ進む";
    case PartKind::Homing: return "曲がる力が上がり、避けられにくくなる";
    case PartKind::BlastRadius: return "消えるときの爆発が大きくなる";
    case PartKind::FlameRadius: return "炸裂したときの爆炎が大きくなる";
    case PartKind::FlameDuration: return "爆炎がその場に残る時間が長くなる";
    case PartKind::Fragments: return "まき散らす破片の数が増える";
    }
    return "";
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
