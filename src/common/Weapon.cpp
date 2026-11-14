#include "pch.h"
#include "Weapon.h"
#include "Config.h"
#include "World.h"

namespace {
    constexpr float Pi = 3.14159265f;

    float DegreesToRadians(float degrees) { return degrees * Pi / 180.0f; }

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

    float AngleOf(const Vec2& v) { return std::atan2(v.y, v.x); }

    // 角度を −π〜π に収める
    float WrapAngle(float a) {
        while (a > Pi) a -= 2.0f * Pi;
        while (a < -Pi) a += 2.0f * Pi;
        return a;
    }

    // 射程の制限なし（距離に関係なく最も近いものを探す）
    constexpr float NoRange = 1.0e9f;

    // 連装型で砲を並べたときの、i 番目の砲の横へのずれ（中央が0）
    float BarrelOffset(int i, int barrels, float spacing) {
        return (static_cast<float>(i) - (barrels - 1) * 0.5f) * spacing;
    }

    // 撃つ間隔を数える部分は、どの武装も同じ。連射強化を付けるほど間隔が短くなる。
    // volleyGap が 0 より大きい武装は、撃つ時間が来るたびに「一斉射」として、1門ずつ volleyGap 秒ずつずらして門数分を続けて撃つ
    class TimedWeapon : public Weapon {
    public:
        TimedWeapon(const WeaponKind& kind, float interval, float firstDelay, float volleyGap = 0.0f)
            : Weapon(kind), interval_(interval), cooldown_(firstDelay), volleyGap_(volleyGap) {}

        void Update(World& world, float dt) override {
            Tick(dt);
            Think(world, dt);
            // 一斉射の残りを、時間をずらして撃つ（狙うものがいなくなったら、残りはやめる）
            if (pendingShots_ > 0) {
                volleyTimer_ -= dt;
                while (pendingShots_ > 0 && volleyTimer_ <= 0.0f) {
                    if (!FireShot(world, nextShot_)) {
                        pendingShots_ = 0;
                        break;
                    }
                    ++nextShot_;
                    --pendingShots_;
                    volleyTimer_ += volleyGap_;
                }
            }
            cooldown_ -= dt;
            if (cooldown_ > 0.0f) {
                return;
            }
            if (pendingShots_ > 0) {
                cooldown_ = 0.0f;  // 前の一斉射が終わるまで、次は始めない
                return;
            }
            if (FireShot(world, 0)) {
                cooldown_ += interval_ * std::pow(Config::PartRateMultiplier, static_cast<float>(CountPart(PartKind::Rate)));
                if (volleyGap_ > 0.0f && Barrels() > 1) {
                    pendingShots_ = Barrels() - 1;
                    nextShot_ = 1;
                    volleyTimer_ = volleyGap_;
                }
            } else {
                cooldown_ = 0.0f;  // 狙うものが現れたらすぐ撃てるようにしておく
            }
        }

    protected:
        // 1発（一斉射なら shot 番目の砲の1発）を撃つ。撃てたら true（狙うものがいなければ false）
        virtual bool FireShot(World& world, int shot) = 0;
        // 撃つかどうかに関係なく、毎回進める処理（扇形の往復など）
        virtual void Tick(float /*dt*/) {}
        // 撃つかどうかに関係なく、毎回 World を見て進める処理（砲の旋回など）
        virtual void Think(World& /*world*/, float /*dt*/) {}

        // 威力強化を反映した倍率
        float PowerScale(float ratePerPart) const {
            return 1.0f + ratePerPart * static_cast<float>(CountPart(PartKind::Power));
        }
        int Barrels() const { return Kind().barrels; }

    private:
        float interval_;
        float cooldown_;
        float volleyGap_;
        int pendingShots_ = 0;   // 一斉射で、まだ撃っていない砲の数
        int nextShot_ = 0;       // 次に撃つ砲の番号
        float volleyTimer_ = 0.0f;
    };

    // 速射砲の連装型で、横に並べる砲の数（4連装は2連装と同じ2門の並びで、発射速度を2倍にする）
    int RapidMounts(int barrels) {
        return barrels == 4 ? 2 : barrels;
    }

    // 速射砲。基本型は最も近い敵を先読みで狙う。弾幕型・攻撃型は扇形に往復しながら撃つ。
    // 近接防御型は、敵本体と敵の弾を区別せずに近くのものだけを、速い連射で狙う。
    // 連装型は、横に並べた砲から1門ずつ順番に撃つ（1発ずつの間隔を門数で割るので、1秒間の弾の数は門数倍）
    class RapidGun : public TimedWeapon {
    public:
        RapidGun(const WeaponKind& kind, float firstDelay)
            : TimedWeapon(kind,
                          (kind.variant == WeaponVariant::RapidClose ? Config::RapidCloseInterval : Config::RapidGunInterval) /
                              static_cast<float>(kind.barrels),
                          firstDelay) {}

        bool HasTurret() const override { return true; }
        float TurretAngle() const override { return facing_; }

    protected:
        void Tick(float dt) override {
            // −1〜1 を往復する（三角波）
            sweepPhase_ += sweepDir_ * DegreesToRadians(Config::RapidSweepSpeedDegrees) / DegreesToRadians(Config::RapidSweepDegrees) * dt;
            if (sweepPhase_ > 1.0f) { sweepPhase_ = 2.0f - sweepPhase_; sweepDir_ = -1.0f; }
            if (sweepPhase_ < -1.0f) { sweepPhase_ = -2.0f - sweepPhase_; sweepDir_ = 1.0f; }
        }

        // 毎回、狙う方向を決めて、そちらへ砲を旋回させる（1秒間に回れる角度には上限がある）
        void Think(World& world, float dt) override {
            float desired = 0.0f;
            Target tracked;
            hasTarget_ = DesiredAngle(world, desired, tracked);

            // 狙っていた相手がいなくなったかを見る（次の相手を狙うときの旋回の仕方を決めるため）
            if (trackedKind_ != TargetKind::None && (tracked.kind != trackedKind_ || tracked.id != trackedId_) &&
                world.FindTargetById(trackedKind_, trackedId_).kind == TargetKind::None) {
                lostTarget_ = true;
            }
            trackedKind_ = tracked.kind;
            trackedId_ = tracked.id;

            if (Kind().variant == WeaponVariant::RapidClose) {
                // 近接防御型：ふだんは待ち、射程内に何か入った瞬間にそちらへ向き直って撃ち始める。
                // 射程内に何もいなくなったら、撃つのをやめて待つ状態に戻る
                if (hasTarget_ && !engaged_) {
                    facing_ = desired;
                    lostTarget_ = false;
                    fastTurning_ = false;
                }
                engaged_ = hasTarget_;
            }
            if (!hasTarget_) {
                return;  // 狙うものがいなければ、向きはそのまま
            }

            // 狙っていた相手がいなくなって次の相手を狙うとき：角度の差が大きければ、撃つのをやめて速く旋回する
            if (lostTarget_) {
                lostTarget_ = false;
                if (std::fabs(WrapAngle(desired - facing_)) > DegreesToRadians(Config::RapidRetargetDegrees)) {
                    fastTurning_ = true;
                }
            }
            const float turnDegrees = fastTurning_ ? Config::RapidFastTurnDegrees : Config::RapidTurnDegrees;
            const float maxTurn = DegreesToRadians(turnDegrees) * dt;
            facing_ = WrapAngle(facing_ + std::max(-maxTurn, std::min(maxTurn, WrapAngle(desired - facing_))));
            if (fastTurning_ && std::fabs(WrapAngle(desired - facing_)) <= DegreesToRadians(Config::RapidResumeDegrees)) {
                fastTurning_ = false;  // 向き直ったら、撃つのを再開してふだんの旋回に戻る
            }
        }

        // 弾は、狙う方向ではなく今向いている方向へ出る（旋回中も撃ち続ける）
        bool FireShot(World& world, int /*shot*/) override {
            if (!hasTarget_ || fastTurning_) {
                return false;  // 狙うものがいないときと、速く旋回している間は撃たない
            }
            const Vec2 origin = world.GetCore().pos;
            // ほんの少しばらけて飛ぶ（収束を付けるほど小さくなる）
            const float spread = DegreesToRadians(Config::RapidGunSpreadDegrees) *
                                 std::pow(Config::RapidFocusMultiplier, static_cast<float>(CountPart(PartKind::Focus)));
            const Vec2 side = Vec2::FromAngle(facing_).Perpendicular();
            // 並べた砲から1門ずつ順番に撃つ
            const int mounts = RapidMounts(Barrels());
            const int mount = mountIndex_ % mounts;
            mountIndex_ = (mountIndex_ + 1) % mounts;
            const Vec2 dir = Vec2::FromAngle(facing_ + world.RandomRange(-spread, spread));
            Bullet b;
            b.pos = origin + dir * Config::CoreRadius + side * BarrelOffset(mount, mounts, Config::RapidBarrelSpacing);
            b.vel = dir * Config::RapidGunBulletSpeed;
            b.radius = Config::PlayerBulletRadius;
            b.damage = Config::RapidGunDamage * PowerScale(Config::PartPowerRate);
            b.pierceChance = std::min(1.0f, Config::RapidPierceChancePerPart * static_cast<float>(CountPart(PartKind::Pierce)));
            world.AddPlayerBullet(b);
            return true;
        }

    private:
        // 狙う方向を求める。狙うものがいなければ false。
        // tracked には狙っている相手を入れる（攻撃型のように特定の相手を狙わないときは None のまま）
        bool DesiredAngle(const World& world, float& angle, Target& tracked) const {
            const Vec2 origin = world.GetCore().pos;
            switch (Kind().variant) {
            case WeaponVariant::RapidBarrage:
            case WeaponVariant::RapidAttack: {
                // 中心の向き（弾幕型は敵本体と敵の弾を区別せずに最も近いもの、攻撃型は敵が多い方向）を中心に、扇形に往復する
                Vec2 center;
                const bool crowd = Kind().variant == WeaponVariant::RapidAttack && world.FindCrowdCenter(center);
                if (!crowd) {
                    const Target target = (Kind().variant == WeaponVariant::RapidBarrage) ? world.FindNearestAny(origin, NoRange)
                                                                                           : world.FindNearestTarget(origin);
                    if (target.kind == TargetKind::None) return false;
                    center = target.pos;
                    tracked = target;
                }
                angle = AngleOf(center - origin) + sweepPhase_ * DegreesToRadians(Config::RapidSweepDegrees);
                return true;
            }
            case WeaponVariant::RapidClose: {
                // 近接防御型：敵本体と敵の弾を区別せずに、射程内で最も近いものを先読みで狙う
                const Target target = world.FindNearestAny(origin, Config::RapidCloseRange);
                if (target.kind == TargetKind::None) return false;
                tracked = target;
                angle = AngleOf(LeadTarget(origin, target.pos, target.vel, Config::RapidGunBulletSpeed) - origin);
                return true;
            }
            default: {
                // 基本型：敵本体と敵の弾を区別せずに、最も近いものを先読みで狙う
                const Target target = world.FindNearestAny(origin, NoRange);
                if (target.kind == TargetKind::None) return false;
                tracked = target;
                angle = AngleOf(LeadTarget(origin, target.pos, target.vel, Config::RapidGunBulletSpeed) - origin);
                return true;
            }
            }
        }

        float facing_ = -Pi * 0.5f;  // 今向いている方向（積んだばかりのときは上向き）
        bool hasTarget_ = false;     // 今、狙うものがいるか
        bool engaged_ = false;       // 近接防御型：撃ち続けているか
        TargetKind trackedKind_ = TargetKind::None;  // 前の回に狙っていた相手
        int trackedId_ = 0;
        bool lostTarget_ = false;    // 狙っていた相手がいなくなった（次の相手を狙うときに旋回の仕方を決める）
        bool fastTurning_ = false;   // 撃つのをやめて速く旋回している
        float sweepPhase_ = 0.0f;
        float sweepDir_ = 1.0f;
        int mountIndex_ = 0;  // 次に撃つ砲
    };

    // ビーム砲。基本型は最も近い敵、攻撃型は敵が多い方向、決戦型はHPが最も高い敵を狙う。
    // 連装型は、横に並べた砲から1門ずつ素早く続けて撃つ（1門ごとに狙い直す）
    class BeamGun : public TimedWeapon {
    public:
        BeamGun(const WeaponKind& kind, float firstDelay)
            : TimedWeapon(kind,
                          Config::BeamInterval * (kind.variant == WeaponVariant::BeamDecisive ? Config::BeamDecisiveIntervalScale : 1.0f),
                          firstDelay, Config::BeamVolleyGap) {}

    protected:
        bool FireShot(World& world, int shot) override {
            const Vec2 origin = world.GetCore().pos;
            Vec2 aim;
            const bool crowd = Kind().variant == WeaponVariant::BeamAttack && world.FindCrowdCenter(aim);
            if (!crowd) {
                const Target target = (Kind().variant == WeaponVariant::BeamDecisive) ? world.FindHighestHpTarget(origin)
                                                                                         : world.FindNearestTarget(origin);
                if (target.kind == TargetKind::None) return false;
                aim = LeadTarget(origin, target.pos, target.vel, Config::BeamSpeed);
            }
            const Vec2 dir = (aim - origin).Normalized();
            if (dir.LengthSq() <= 0.0f) return false;
            const Vec2 side = dir.Perpendicular();
            const float scale = (Kind().variant == WeaponVariant::BeamDecisive) ? Config::BeamDecisiveDamageScale : 1.0f;
            {
                Beam b;
                b.pos = origin + dir * Config::CoreRadius + side * BarrelOffset(shot, Barrels(), Config::BeamBarrelSpacing);
                b.vel = dir * Config::BeamSpeed;
                b.damage = Config::BeamDamage * scale * PowerScale(Config::PartPowerRate);
                b.decay = std::min(Config::BeamDecayMax,
                                   Config::BeamDecay + Config::BeamPierceDecayPerPart * static_cast<float>(CountPart(PartKind::Pierce)));
                world.AddBeam(b);
            }
            return true;
        }
    };

    // 誘導兵器。基本型は最も近い敵、攻撃型は敵が多い場所（爆発が大きい）を追いかける。
    // 近接防御型は、敵本体か敵の弾が近くに来たときだけ撃つ
    // 連装型は、全門が同じ向きに、少しずつ時間をずらして撃ち出す（1門ごとに狙い直す）
    class MissileLauncher : public TimedWeapon {
    public:
        MissileLauncher(const WeaponKind& kind, float firstDelay)
            : TimedWeapon(kind, Config::MissileInterval, firstDelay, Config::MissileVolleyGap) {}

    protected:
        bool FireShot(World& world, int /*shot*/) override {
            const Vec2 origin = world.GetCore().pos;
            Target target;
            Vec2 center;
            switch (Kind().variant) {
            case WeaponVariant::MissileAttack:
                // 敵が多い場所の中心に最も近い敵を狙う
                target = world.FindCrowdCenter(center) ? world.FindNearestTarget(center) : world.FindNearestTarget(origin);
                break;
            case WeaponVariant::MissileClose:
                target = world.FindNearestAny(origin, Config::MissileCloseRange);
                break;
            default:
                // 基本型：敵本体と敵の弾を区別せずに、最も近いものを狙う
                target = world.FindNearestAny(origin, NoRange);
                break;
            }
            if (target.kind == TargetKind::None) {
                return false;
            }
            const float baseHeading = AngleOf(target.pos - origin);
            float blast = Config::MissileExplosionRadius *
                          (1.0f + Config::MissileBlastRate * static_cast<float>(CountPart(PartKind::BlastRadius)));
            if (Kind().variant == WeaponVariant::MissileAttack) {
                blast *= Config::MissileAttackBlastScale;
            }
            {
                Missile m;
                m.heading = baseHeading;
                m.pos = origin + Vec2::FromAngle(m.heading) * Config::CoreRadius;
                m.targetKind = target.kind;
                m.targetId = target.id;
                m.damage = Config::MissileDamage * PowerScale(Config::PartPowerRate);
                m.turnDegrees = Config::MissileTurnDegrees *
                                (1.0f + Config::MissileHomingRate * static_cast<float>(CountPart(PartKind::Homing)));
                m.blastRadius = blast;
                // 狙い直すときも、弾を含めて探すか（基本型・近接防御型）
                m.targetsBullets = Kind().variant != WeaponVariant::MissileAttack;
                world.AddMissile(m);
            }
            return true;
        }
    };

    // 炸裂弾。基本型は敵が多い場所の、中心の少し手前で炸裂させる。決戦型はHPが最も高い敵に直撃させる
    // 連装型は、1門ごとに向きを変えた扇形で、少しずつ時間をずらして続けて撃つ（扇形の中心は撃ち始めたときの狙い）
    class ShellCannon : public TimedWeapon {
    public:
        ShellCannon(const WeaponKind& kind, float firstDelay)
            : TimedWeapon(kind, Config::ShellInterval, firstDelay, Config::ShellVolleyGap) {}

    protected:
        bool FireShot(World& world, int shot) override {
            const Vec2 origin = world.GetCore().pos;
            if (shot > 0) {
                // 一斉射の2発目以降は、撃ち始めたときの向きと距離を使う
                Launch(world, origin, volleyAngle_, volleyDistance_, shot);
                return true;
            }
            Vec2 aim;
            float distance = 0.0f;
            if (Kind().variant == WeaponVariant::ShellDecisive) {
                const Target target = world.FindHighestHpTarget(origin);
                if (target.kind == TargetKind::None) return false;
                aim = LeadTarget(origin, target.pos, target.vel, Config::ShellSpeed);
                distance = Config::ShellDecisiveRange;  // 当たるまで飛ぶ
            } else {
                if (!world.FindCrowdCenter(aim)) {
                    // 敵本体がいなければ、最も近い敵の弾のところで炸裂させる
                    const Target target = world.FindNearestTarget(origin);
                    if (target.kind == TargetKind::None) return false;
                    aim = target.pos;
                }
                distance = std::max(Config::CoreRadius + 10.0f, (aim - origin).Length() - Config::ShellShortDistance);
            }
            const Vec2 toAim = aim - origin;
            if (toAim.LengthSq() <= 0.0f) return false;
            volleyAngle_ = AngleOf(toAim);
            volleyDistance_ = distance;
            Launch(world, origin, volleyAngle_, volleyDistance_, 0);
            return true;
        }

    private:
        // shot 番目の砲の1発を撃つ（扇形の中で、その砲の向きにずらす）
        void Launch(World& world, const Vec2& origin, float centerAngle, float distance, int shot) {
            const Vec2 dir = Vec2::FromAngle(centerAngle + DegreesToRadians(BarrelOffset(shot, Barrels(), Config::ShellFanDegrees)));
            {
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
            }
        }

        float volleyAngle_ = 0.0f;
        float volleyDistance_ = 0.0f;
    };

    int BaseEnergy(WeaponVariant v) {
        switch (v) {
        case WeaponVariant::RapidBasic: return Config::RapidGunEnergy;
        case WeaponVariant::RapidBarrage: return Config::RapidBarrageEnergy;
        case WeaponVariant::RapidAttack: return Config::RapidAttackEnergy;
        case WeaponVariant::RapidClose: return Config::RapidCloseEnergy;
        case WeaponVariant::BeamBasic: return Config::BeamEnergy;
        case WeaponVariant::BeamAttack: return Config::BeamAttackEnergy;
        case WeaponVariant::BeamDecisive: return Config::BeamDecisiveEnergy;
        case WeaponVariant::MissileBasic: return Config::MissileEnergy;
        case WeaponVariant::MissileAttack: return Config::MissileAttackEnergy;
        case WeaponVariant::MissileClose: return Config::MissileCloseEnergy;
        case WeaponVariant::ShellBasic: return Config::ShellEnergy;
        case WeaponVariant::ShellDecisive: return Config::ShellDecisiveEnergy;
        }
        return 0;
    }

    const char* VariantName(WeaponVariant v) {
        switch (v) {
        case WeaponVariant::RapidBasic: return "速射砲 基本型";
        case WeaponVariant::RapidBarrage: return "速射砲 弾幕型";
        case WeaponVariant::RapidAttack: return "速射砲 攻撃型";
        case WeaponVariant::RapidClose: return "速射砲 近接防御型";
        case WeaponVariant::BeamBasic: return "ビーム砲 基本型";
        case WeaponVariant::BeamAttack: return "ビーム砲 攻撃型";
        case WeaponVariant::BeamDecisive: return "ビーム砲 決戦型";
        case WeaponVariant::MissileBasic: return "誘導兵器 基本型";
        case WeaponVariant::MissileAttack: return "誘導兵器 攻撃型";
        case WeaponVariant::MissileClose: return "誘導兵器 近接防御型";
        case WeaponVariant::ShellBasic: return "炸裂弾 基本型";
        case WeaponVariant::ShellDecisive: return "炸裂弾 決戦型";
        }
        return "";
    }

    const char* VariantDescription(WeaponVariant v) {
        switch (v) {
        case WeaponVariant::RapidBasic: return "最も近い敵を狙い、小さく速い弾を連射する。";
        case WeaponVariant::RapidBarrage: return "最も近い敵の方向を中心に、扇形に往復しながら連射する。";
        case WeaponVariant::RapidAttack: return "敵が多い方向を中心に、扇形に往復しながら連射する。";
        case WeaponVariant::RapidClose: return "近くの敵と敵の弾を、とても速い連射で撃ち落とす。遠くには撃たない。";
        case WeaponVariant::BeamBasic: return "最も近い敵へ、敵を貫く速いビームを撃つ。貫くたびに威力が下がる。";
        case WeaponVariant::BeamAttack: return "敵が多い方向へ、敵を貫く速いビームを撃つ。";
        case WeaponVariant::BeamDecisive: return "HPが最も高い敵を狙い、威力の高いビームを撃つ。連射は遅め。";
        case WeaponVariant::MissileBasic: return "最も近い敵を追いかける弾を撃つ。当たると小さな爆発で周りを巻き込む。";
        case WeaponVariant::MissileAttack: return "敵が多い場所へ追いかける弾を撃つ。消えるときの爆発が大きい。";
        case WeaponVariant::MissileClose: return "近くに来た敵や敵の弾だけを狙って、追いかける弾を撃つ。";
        case WeaponVariant::ShellBasic: return "敵が多い場所の手前で炸裂し、残る爆炎と前方への破片で敵をまとめて倒す。";
        case WeaponVariant::ShellDecisive: return "HPが最も高い敵を狙い、威力の極めて高い直撃を当てる。";
        }
        return "";
    }
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
    return WeaponEnergy(kind_) + n * (n + 1) / 2;
}

WeaponFamily FamilyOf(WeaponVariant variant) {
    switch (variant) {
    case WeaponVariant::RapidBasic:
    case WeaponVariant::RapidBarrage:
    case WeaponVariant::RapidAttack:
    case WeaponVariant::RapidClose:
        return WeaponFamily::RapidGun;
    case WeaponVariant::BeamBasic:
    case WeaponVariant::BeamAttack:
    case WeaponVariant::BeamDecisive:
        return WeaponFamily::Beam;
    case WeaponVariant::MissileBasic:
    case WeaponVariant::MissileAttack:
    case WeaponVariant::MissileClose:
        return WeaponFamily::Missile;
    case WeaponVariant::ShellBasic:
    case WeaponVariant::ShellDecisive:
        return WeaponFamily::Shell;
    }
    return WeaponFamily::RapidGun;
}

WeaponFamily FamilyOf(const WeaponKind& kind) {
    return FamilyOf(kind.variant);
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

const std::vector<int>& BarrelSteps(WeaponFamily family) {
    static const std::vector<int> rapid = { 1, 2, 3, 4 };
    static const std::vector<int> three = { 1, 2, 3 };
    static const std::vector<int> missile = { 1, 2, 4, 8 };
    switch (family) {
    case WeaponFamily::RapidGun: return rapid;
    case WeaponFamily::Missile: return missile;
    default: return three;
    }
}

WeaponKind NextBarrelStep(const WeaponKind& kind) {
    const auto& steps = BarrelSteps(FamilyOf(kind));
    for (size_t i = 0; i + 1 < steps.size(); ++i) {
        if (steps[i] == kind.barrels) {
            return WeaponKind{ kind.variant, steps[i + 1] };
        }
    }
    return WeaponKind{ kind.variant, 0 };
}

std::string WeaponName(const WeaponKind& kind) {
    std::string name = VariantName(kind.variant);
    if (kind.barrels > 1) {
        name += "（" + std::to_string(kind.barrels) + "連装）";
    }
    return name;
}

std::string WeaponDescription(const WeaponKind& kind) {
    std::string desc = VariantDescription(kind.variant);
    if (kind.barrels > 1) {
        desc += std::to_string(kind.barrels) + "門で同時に撃つ。";
    }
    return desc;
}

int WeaponEnergy(const WeaponKind& kind) {
    // 元のエネルギー×門数×割合（四捨五入）にして、少し安くする。
    // ただし、連装の段階が1つ上がるごとに最低でも1は増やす（元のエネルギーが1のときは、門数分と同じになる）
    const int base = BaseEnergy(kind.variant);
    int energy = base;
    for (int barrels : BarrelSteps(FamilyOf(kind))) {
        if (barrels <= 1) continue;
        if (barrels > kind.barrels) break;
        const int scaled = static_cast<int>(std::lround(base * barrels * Config::MultiBarrelEnergyRate));
        energy = std::max(energy + 1, scaled);
    }
    return energy;
}

std::unique_ptr<Weapon> CreateWeapon(const WeaponKind& kind, float firstDelay) {
    switch (FamilyOf(kind)) {
    case WeaponFamily::RapidGun: return std::make_unique<RapidGun>(kind, firstDelay);
    case WeaponFamily::Beam: return std::make_unique<BeamGun>(kind, firstDelay);
    case WeaponFamily::Missile: return std::make_unique<MissileLauncher>(kind, firstDelay);
    case WeaponFamily::Shell: return std::make_unique<ShellCannon>(kind, firstDelay);
    }
    return nullptr;
}
