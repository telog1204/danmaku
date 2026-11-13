#include "pch.h"
#include "World.h"
#include <DxLib.h>
#include "Config.h"
#include "EnemyAI.h"

namespace {
    constexpr float Pi = 3.14159265f;

    bool Overlaps(const Vec2& a, float ra, const Vec2& b, float rb) {
        const float r = ra + rb;
        return (a - b).LengthSq() <= r * r;
    }

    // 点 p から線分 a-b までの距離の2乗
    float DistanceSqToSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
        const Vec2 ab = b - a;
        const float lenSq = ab.LengthSq();
        float t = lenSq > 0.0f ? (p - a).Dot(ab) / lenSq : 0.0f;
        t = std::max(0.0f, std::min(1.0f, t));
        return (p - (a + ab * t)).LengthSq();
    }

    // 画面の外へこれだけ出た弾は消す
    constexpr float CullMargin = 40.0f;

    bool IsFarOutside(const Vec2& p) {
        return p.x < -CullMargin || p.y < -CullMargin ||
               p.x > Config::ScreenWidth + CullMargin || p.y > Config::ScreenHeight + CullMargin;
    }

    bool Contains(const std::vector<int>& ids, int id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    template <typename T>
    void EraseDead(std::vector<T>& items) {
        items.erase(std::remove_if(items.begin(), items.end(), [](const T& item) { return !item.alive; }), items.end());
    }

    float WrapAngle(float a) {
        while (a > Pi) a -= 2.0f * Pi;
        while (a < -Pi) a += 2.0f * Pi;
        return a;
    }

    // 開始時に、速射砲に加えて持つ武装の候補（エネルギー上限以下のもの）
    const WeaponKind StartWeaponChoices[] = { WeaponKind::Beam, WeaponKind::Missile, WeaponKind::Shell };
    constexpr int StartWeaponChoiceCount = 3;
}

Vec2 Beam::Tail() const {
    const float len = std::min(Config::BeamTailLength, traveled);
    return pos - vel.Normalized() * len;
}

World::World() : rng_(static_cast<unsigned int>(GetNowHiPerformanceCount())) {
    Reset();
}

void World::Reset() {
    ResetWithStartWeapon(std::uniform_int_distribution<int>(0, StartWeaponChoiceCount - 1)(rng_));
}

void World::ResetWithNextStartWeapon() {
    ResetWithStartWeapon((startWeaponIndex_ + 1) % StartWeaponChoiceCount);
}

void World::ResetWithStartWeapon(int startWeaponIndex) {
    core_.pos = Vec2(Config::ScreenWidth * 0.5f, Config::ScreenHeight * 0.5f);
    core_.radius = Config::CoreRadius;
    core_.maxHp = Config::CoreMaxHp;
    core_.hp = core_.maxHp;

    // 組み込み画面ができるまでは、開始時の武装をエネルギー上限に収まるように自動で積む。
    // ランダムに選んだ武装を1つ積み、残ったエネルギーで速射砲を積めるだけ積む
    startWeaponIndex_ = startWeaponIndex;
    weapons_.clear();
    const WeaponKind start = StartWeaponChoices[startWeaponIndex_];
    ownedWeapons_ = { WeaponKind::RapidGun, start };
    energyCap_ = Config::StartEnergy;
    weapons_.push_back(CreateWeapon(start, 0.0f));
    int energyLeft = Config::StartEnergy - WeaponEnergy(start);
    int rapidCount = 0;
    while (energyLeft >= WeaponEnergy(WeaponKind::RapidGun)) {
        // 撃つタイミングが重ならないように、少しずつずらす
        weapons_.push_back(CreateWeapon(WeaponKind::RapidGun, Config::RapidGunInterval * 0.5f * rapidCount));
        energyLeft -= WeaponEnergy(WeaponKind::RapidGun);
        ++rapidCount;
    }

    enemies_.clear();
    playerBullets_.clear();
    enemyBullets_.clear();
    beams_.clear();
    missiles_.clear();
    shells_.clear();
    explosions_.clear();
    pendingExplosions_.clear();
    nextId_ = 1;
    elapsed_ = 0.0f;
    spawnTimer_ = 0.0f;
    gameOver_ = false;
    gameOverSeconds_ = 0.0f;
}

int World::UsedEnergy() const {
    int used = 0;
    for (const auto& w : weapons_) {
        used += WeaponEnergy(w->Kind());
    }
    return used;
}

bool World::CanEquip(WeaponKind kind) const {
    return UsedEnergy() + WeaponEnergy(kind) <= energyCap_;
}

void World::Equip(WeaponKind kind) {
    if (!CanEquip(kind)) {
        return;
    }
    // 撃つタイミングがほかの武装と重ならないように、最初に撃つまでの時間を少しずらす
    weapons_.push_back(CreateWeapon(kind, RandomRange(0.0f, 0.3f)));
}

void World::Unequip(size_t index) {
    if (index < weapons_.size()) {
        weapons_.erase(weapons_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

float World::RandomRange(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_);
}

Target World::FindNearestTarget(const Vec2& from) const {
    Target best;
    float bestDistSq = 0.0f;
    for (const Enemy& e : enemies_) {
        if (!e.alive) continue;
        const float d = (e.pos - from).LengthSq();
        if (best.kind == TargetKind::None || d < bestDistSq) {
            best.kind = TargetKind::Enemy;
            best.id = e.id;
            best.pos = e.pos;
            best.vel = e.vel;
            bestDistSq = d;
        }
    }
    if (best.kind != TargetKind::None) {
        return best;
    }
    for (const Bullet& b : enemyBullets_) {
        if (!b.alive) continue;
        const float d = (b.pos - from).LengthSq();
        if (best.kind == TargetKind::None || d < bestDistSq) {
            best.kind = TargetKind::EnemyBullet;
            best.id = b.id;
            best.pos = b.pos;
            best.vel = b.vel;
            bestDistSq = d;
        }
    }
    return best;
}

Target World::FindTargetById(TargetKind kind, int id) const {
    Target t;
    if (kind == TargetKind::Enemy) {
        for (const Enemy& e : enemies_) {
            if (e.alive && e.id == id) {
                t.kind = kind;
                t.id = id;
                t.pos = e.pos;
                t.vel = e.vel;
                break;
            }
        }
    } else if (kind == TargetKind::EnemyBullet) {
        for (const Bullet& b : enemyBullets_) {
            if (b.alive && b.id == id) {
                t.kind = kind;
                t.id = id;
                t.pos = b.pos;
                t.vel = b.vel;
                break;
            }
        }
    }
    return t;
}

bool World::FindCrowdCenter(Vec2& center) const {
    // 周り（CrowdRadius 以内）にいる敵本体が最も多い敵を探し、その周りの敵の平均の位置を中心とする
    int bestCount = 0;
    const float r2 = Config::CrowdRadius * Config::CrowdRadius;
    for (const Enemy& e : enemies_) {
        if (!e.alive) continue;
        int count = 0;
        Vec2 sum;
        for (const Enemy& o : enemies_) {
            if (o.alive && (o.pos - e.pos).LengthSq() <= r2) {
                ++count;
                sum += o.pos;
            }
        }
        if (count > bestCount) {
            bestCount = count;
            center = sum * (1.0f / count);
        }
    }
    return bestCount > 0;
}

void World::FirePlayerBullet(const Vec2& pos, const Vec2& vel, float radius, float damage, float life) {
    Bullet b;
    b.pos = pos;
    b.vel = vel;
    b.radius = radius;
    b.damage = damage;
    b.life = life;
    playerBullets_.push_back(b);
}

void World::FireEnemyBullet(const Vec2& pos, const Vec2& vel, float radius, float damage) {
    Bullet b;
    b.id = nextId_++;
    b.pos = pos;
    b.vel = vel;
    b.radius = radius;
    b.damage = damage;
    enemyBullets_.push_back(b);
}

void World::FireBeam(const Vec2& pos, const Vec2& vel, float damage) {
    Beam b;
    b.pos = pos;
    b.vel = vel;
    b.damage = damage;
    beams_.push_back(b);
}

void World::FireMissile(const Vec2& pos, float heading, const Target& target) {
    Missile m;
    m.pos = pos;
    m.heading = heading;
    m.targetKind = target.kind;
    m.targetId = target.id;
    missiles_.push_back(m);
}

void World::FireShell(const Vec2& pos, const Vec2& vel, float distance) {
    Shell s;
    s.pos = pos;
    s.vel = vel;
    s.distanceLeft = distance;
    shells_.push_back(s);
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

    for (auto& w : weapons_) {
        w->Update(*this, dt);
    }

    // 敵が撃つと enemyBullets_ が増えるだけで enemies_ は変わらないので、参照のまま回してよい
    for (Enemy& e : enemies_) {
        UpdateEnemy(e, *this, dt);
    }

    MoveBullets(playerBullets_, dt);
    MoveBullets(enemyBullets_, dt);
    MoveBeams(dt);
    MoveMissiles(dt);
    MoveShells(dt);
    AgeExplosions(dt);
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
    e.id = nextId_++;
    const int kind = std::uniform_int_distribution<int>(0, 2)(rng_);
    float baseHp = 0.0f;
    switch (kind) {
    case 0:
        e.type = EnemyType::Charger;
        e.radius = Config::ChargerRadius;
        e.speed = Config::ChargerSpeed;
        baseHp = static_cast<float>(Config::ChargerHp);
        break;
    case 1:
        e.type = EnemyType::Shooter;
        e.radius = Config::ShooterRadius;
        e.speed = Config::ShooterSpeed;
        baseHp = static_cast<float>(Config::ShooterHp);
        e.shotTimer = RandomRange(0.5f, Config::ShooterShotInterval);
        break;
    default:
        e.type = EnemyType::Evader;
        e.radius = Config::EvaderRadius;
        e.speed = Config::EvaderSpeed;
        baseHp = static_cast<float>(Config::EvaderHp);
        e.shotTimer = RandomRange(0.5f, Config::EvaderShotInterval);
        break;
    }
    // 時間が経つほどHPが増える
    e.maxHp = baseHp * (1.0f + elapsed_ / Config::EnemyHpGrowthSeconds);
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
    // 最初はコアの方へ機首を向けて現れる
    const Vec2 toCore = core_.pos - e.pos;
    e.heading = std::atan2(toCore.y, toCore.x);
    e.vel = Vec2::FromAngle(e.heading) * e.speed;
    enemies_.push_back(e);
}

void World::MoveBullets(std::vector<Bullet>& bullets, float dt) {
    for (Bullet& b : bullets) {
        b.pos += b.vel * dt;
        if (b.life >= 0.0f) {
            b.life -= dt;
            if (b.life <= 0.0f) {
                b.alive = false;
            }
        }
        if (IsFarOutside(b.pos)) {
            b.alive = false;
        }
    }
}

void World::MoveBeams(float dt) {
    for (Beam& b : beams_) {
        const Vec2 step = b.vel * dt;
        b.pos += step;
        b.traveled += step.Length();
        // 尾まで画面の外へ出たら消す
        if (IsFarOutside(b.Tail())) {
            b.alive = false;
        }
    }
}

void World::MoveMissiles(float dt) {
    const float maxTurn = Config::MissileTurnDegrees * Pi / 180.0f * dt;
    for (Missile& m : missiles_) {
        m.age += dt;
        if (m.age > Config::MissileLifeSeconds || IsFarOutside(m.pos)) {
            m.alive = false;
            continue;
        }
        // 撃ち出してしばらくはまっすぐ進み、そのあと狙った相手を追いかける
        if (m.age > Config::MissileStraightSeconds) {
            Target target = FindTargetById(m.targetKind, m.targetId);
            if (target.kind == TargetKind::None) {
                // 狙った相手がいなくなったら、別の相手を狙い直す
                target = FindNearestTarget(m.pos);
                m.targetKind = target.kind;
                m.targetId = target.id;
            }
            if (target.kind != TargetKind::None) {
                const Vec2 to = target.pos - m.pos;
                const float diff = WrapAngle(std::atan2(to.y, to.x) - m.heading);
                m.heading = WrapAngle(m.heading + std::max(-maxTurn, std::min(maxTurn, diff)));
            }
        }
        m.pos += Vec2::FromAngle(m.heading) * (Config::MissileSpeed * dt);
    }
}

void World::MoveShells(float dt) {
    for (Shell& s : shells_) {
        const Vec2 step = s.vel * dt;
        s.pos += step;
        s.distanceLeft -= step.Length();
        if (s.distanceLeft <= 0.0f) {
            BurstShell(s);
        } else if (IsFarOutside(s.pos)) {
            s.alive = false;
        }
    }
}

void World::AgeExplosions(float dt) {
    for (Explosion& x : explosions_) {
        x.life -= dt;
        if (x.life <= 0.0f) {
            x.alive = false;
        }
    }
}

void World::DamageEnemy(Enemy& e, float damage) {
    e.hp -= damage;
    if (e.hp <= 0.0f) {
        e.alive = false;
    }
}

void World::AddExplosion(const Vec2& pos, float radius, float damage, float seconds) {
    Explosion x;
    x.pos = pos;
    x.radius = radius;
    x.damage = damage;
    x.life = seconds;
    x.maxLife = seconds;
    pendingExplosions_.push_back(x);
}

void World::DetonateMissile(Missile& m) {
    if (!m.alive) return;
    m.alive = false;
    AddExplosion(m.pos, Config::MissileExplosionRadius, Config::MissileExplosionDamage, Config::MissileExplosionSeconds);
}

void World::BurstShell(Shell& s) {
    if (!s.alive) return;
    s.alive = false;
    // その場に残る大きな爆炎
    AddExplosion(s.pos, Config::ShellFlameRadius, Config::ShellFlameDamage, Config::ShellFlameSeconds);
    // 進んでいた方向の前方へ、円錐形に破片をまく
    const float baseAngle = std::atan2(s.vel.y, s.vel.x);
    const float spread = Config::ShellFragmentSpreadDegrees * Pi / 180.0f;
    for (int i = 0; i < Config::ShellFragmentCount; ++i) {
        const float angle = baseAngle + RandomRange(-spread, spread);
        const float speed = Config::ShellFragmentSpeed * RandomRange(0.8f, 1.2f);
        FirePlayerBullet(s.pos, Vec2::FromAngle(angle) * speed, Config::FragmentRadius,
                         Config::ShellFragmentDamage, Config::ShellFragmentSeconds);
    }
}

void World::HandleCollisions() {
    // 速射砲の弾・破片：敵の弾とぶつかると両方消える。敵本体に当たると消える（貫かない）
    for (Bullet& pb : playerBullets_) {
        if (!pb.alive) continue;
        for (Bullet& eb : enemyBullets_) {
            if (eb.alive && Overlaps(pb.pos, pb.radius, eb.pos, eb.radius)) {
                pb.alive = false;
                eb.alive = false;
                break;
            }
        }
        if (!pb.alive) continue;
        for (Enemy& e : enemies_) {
            if (e.alive && Overlaps(pb.pos, pb.radius, e.pos, e.radius)) {
                pb.alive = false;
                DamageEnemy(e, pb.damage);
                break;
            }
        }
    }

    // ビーム：尾を含めた線全体で、敵の弾と敵本体を貫く。貫くたびに威力が下がる
    for (Beam& b : beams_) {
        if (!b.alive) continue;
        const Vec2 tail = b.Tail();
        for (Bullet& eb : enemyBullets_) {
            if (!eb.alive) continue;
            const float r = eb.radius + Config::BeamHalfWidth;
            if (DistanceSqToSegment(eb.pos, tail, b.pos) <= r * r) {
                eb.alive = false;
                b.damage *= Config::BeamDecay;
            }
        }
        for (Enemy& e : enemies_) {
            if (!e.alive || Contains(b.hitIds, e.id)) continue;
            const float r = e.radius + Config::BeamHalfWidth;
            if (DistanceSqToSegment(e.pos, tail, b.pos) <= r * r) {
                b.hitIds.push_back(e.id);
                DamageEnemy(e, b.damage);
                b.damage *= Config::BeamDecay;
            }
        }
    }

    // 誘導弾：敵本体や敵の弾に当たると、消えて小さな爆発を起こす
    for (Missile& m : missiles_) {
        if (!m.alive) continue;
        for (Enemy& e : enemies_) {
            if (e.alive && Overlaps(m.pos, Config::MissileRadius, e.pos, e.radius)) {
                DamageEnemy(e, Config::MissileDamage);
                DetonateMissile(m);
                break;
            }
        }
        if (!m.alive) continue;
        for (Bullet& eb : enemyBullets_) {
            if (eb.alive && Overlaps(m.pos, Config::MissileRadius, eb.pos, eb.radius)) {
                eb.alive = false;
                DetonateMissile(m);
                break;
            }
        }
    }

    // 炸裂弾：途中で敵本体や敵の弾に当たると、その場で炸裂する（敵本体には直撃の威力）
    for (Shell& s : shells_) {
        if (!s.alive) continue;
        for (Enemy& e : enemies_) {
            if (e.alive && Overlaps(s.pos, Config::ShellRadius, e.pos, e.radius)) {
                DamageEnemy(e, Config::ShellDirectDamage);
                BurstShell(s);
                break;
            }
        }
        if (!s.alive) continue;
        for (Bullet& eb : enemyBullets_) {
            if (eb.alive && Overlaps(s.pos, Config::ShellRadius, eb.pos, eb.radius)) {
                eb.alive = false;
                BurstShell(s);
                break;
            }
        }
    }

    // 爆発・爆炎：残っているあいだ、触れた敵（1体につき1回）・敵の弾・誘導弾を巻き込む。
    // 巻き込まれた誘導弾が起こす爆発も、同じフレームのうちに次々と広がる（誘爆）
    // 残っている爆炎も毎回判定するので、新しい爆発がなくても1回は回す
    do {
        for (Explosion& x : pendingExplosions_) {
            explosions_.push_back(x);
        }
        pendingExplosions_.clear();
        for (Explosion& x : explosions_) {
            if (!x.alive) continue;
            for (Enemy& e : enemies_) {
                if (e.alive && !Contains(x.hitIds, e.id) && Overlaps(x.pos, x.radius, e.pos, e.radius)) {
                    x.hitIds.push_back(e.id);
                    DamageEnemy(e, x.damage);
                }
            }
            for (Bullet& eb : enemyBullets_) {
                if (eb.alive && Overlaps(x.pos, x.radius, eb.pos, eb.radius)) {
                    eb.alive = false;
                }
            }
            for (Missile& m : missiles_) {
                if (m.alive && Overlaps(x.pos, x.radius, m.pos, Config::MissileRadius)) {
                    DetonateMissile(m);
                }
            }
        }
    } while (!pendingExplosions_.empty());

    // 敵の弾がコアに当たる
    for (Bullet& eb : enemyBullets_) {
        if (eb.alive && Overlaps(eb.pos, eb.radius, core_.pos, core_.radius)) {
            eb.alive = false;
            core_.hp -= static_cast<int>(eb.damage);
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
    EraseDead(beams_);
    EraseDead(missiles_);
    EraseDead(shells_);
    EraseDead(explosions_);
}
