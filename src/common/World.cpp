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

    const WeaponKind RapidBasicKind{ WeaponVariant::RapidBasic, 1 };

    // 開始時に、速射砲の基本型に加えて持つ武装の候補（エネルギー上限以下の単装。速射砲の基本型は除く）
    std::vector<WeaponKind> StartWeaponChoices() {
        std::vector<WeaponKind> choices;
        for (int v = 0; v < WeaponVariantCount; ++v) {
            const WeaponKind kind{ static_cast<WeaponVariant>(v), 1 };
            if (kind != RapidBasicKind && WeaponEnergy(kind) <= Config::StartEnergy) {
                choices.push_back(kind);
            }
        }
        return choices;
    }
}

Vec2 Beam::Tail() const {
    const float len = std::min(Config::BeamTailLength, traveled);
    return pos - vel.Normalized() * len;
}

World::World() : rng_(static_cast<unsigned int>(GetNowHiPerformanceCount())) {
    Reset();
}

void World::Reset() {
    core_.pos = Vec2(Config::ScreenWidth * 0.5f, Config::ScreenHeight * 0.5f);
    core_.radius = Config::CoreRadius;
    core_.maxHp = Config::CoreMaxHp;
    core_.hp = core_.maxHp;

    // 開始時は、速射砲に加えてランダムに選んだ武装を1つ持つ。
    // 最初はエネルギー上限に収まるように自動で積む（ランダムな武装を1つ、残ったエネルギーで速射砲を積めるだけ）
    weapons_.clear();
    const std::vector<WeaponKind> choices = StartWeaponChoices();
    const WeaponKind start = choices[std::uniform_int_distribution<size_t>(0, choices.size() - 1)(rng_)];
    ownedWeapons_ = { RapidBasicKind, start };
    for (auto& row : spareParts_) {
        for (int& n : row) n = 0;
    }
    energyCap_ = Config::StartEnergy;
    exp_ = 0;
    expNeed_ = Config::ExpFirstNeed;
    weapons_.push_back(CreateWeapon(start, 0.0f));
    int energyLeft = Config::StartEnergy - WeaponEnergy(start);
    int rapidCount = 0;
    while (energyLeft >= WeaponEnergy(RapidBasicKind)) {
        // 撃つタイミングが重ならないように、少しずつずらす
        weapons_.push_back(CreateWeapon(RapidBasicKind, Config::RapidGunInterval * 0.5f * rapidCount));
        energyLeft -= WeaponEnergy(RapidBasicKind);
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
    pickups_.clear();
    messages_.clear();
    rewardChoices_.clear();
    bossTimer_ = Config::BossInterval;
    kills_ = 0;
    bossKills_ = 0;
    nextId_ = 1;
    elapsed_ = 0.0f;
    spawnTimer_ = 0.0f;
    gameOver_ = false;
    gameOverSeconds_ = 0.0f;
}

int World::UsedEnergy() const {
    int used = 0;
    for (const auto& w : weapons_) {
        used += w->Energy();
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
    if (index >= weapons_.size()) {
        return;
    }
    // 付けていた強化パーツは手元に戻す
    const int family = static_cast<int>(FamilyOf(weapons_[index]->Kind()));
    for (PartKind part : weapons_[index]->Parts()) {
        ++spareParts_[family][static_cast<int>(part)];
    }
    weapons_.erase(weapons_.begin() + static_cast<std::ptrdiff_t>(index));
}

int World::SpareParts(WeaponFamily family, PartKind part) const {
    return spareParts_[static_cast<int>(family)][static_cast<int>(part)];
}

bool World::CanAttachPart(size_t weaponIndex, PartKind part) const {
    if (weaponIndex >= weapons_.size()) {
        return false;
    }
    const Weapon& w = *weapons_[weaponIndex];
    return SpareParts(FamilyOf(w.Kind()), part) > 0 && UsedEnergy() + w.NextPartEnergy() <= energyCap_;
}

void World::AttachPart(size_t weaponIndex, PartKind part) {
    if (!CanAttachPart(weaponIndex, part)) {
        return;
    }
    Weapon& w = *weapons_[weaponIndex];
    --spareParts_[static_cast<int>(FamilyOf(w.Kind()))][static_cast<int>(part)];
    w.AddPart(part);
}

void World::DetachPart(size_t weaponIndex, PartKind part) {
    if (weaponIndex >= weapons_.size()) {
        return;
    }
    Weapon& w = *weapons_[weaponIndex];
    if (w.RemovePart(part)) {
        ++spareParts_[static_cast<int>(FamilyOf(w.Kind()))][static_cast<int>(part)];
    }
}

void World::Collect(const Pickup& pickup) {
    switch (pickup.kind) {
    case PickupKind::Energy: {
        ++energyCap_;
        AddMessage("エネルギー上限が1増えた（上限 " + std::to_string(energyCap_) + "）", pickup.kind);
        break;
    }
    case PickupKind::Weapon:
        if (!Owns(pickup.weapon)) {
            ownedWeapons_.push_back(pickup.weapon);
            AddMessage(WeaponName(pickup.weapon) + " を手に入れた", pickup.kind);
        } else {
            // 同じ武装を続けて拾ったときは、その系統の強化パーツにする
            Pickup part = pickup;
            part.kind = PickupKind::Part;
            const auto& parts = FamilyParts(pickup.family);
            part.part = parts[std::uniform_int_distribution<size_t>(0, parts.size() - 1)(rng_)];
            Collect(part);
        }
        break;
    case PickupKind::Part:
        ++spareParts_[static_cast<int>(pickup.family)][static_cast<int>(pickup.part)];
        AddMessage(std::string(FamilyName(pickup.family)) + "の強化パーツ「" + PartName(pickup.part) + "」を手に入れた",
                   pickup.kind);
        break;
    }
}

void World::AddMessage(const std::string& text, PickupKind kind) {
    // 画面の上に並べるのは新しいものから4つまで
    constexpr size_t MaxMessages = 4;
    if (messages_.size() >= MaxMessages) {
        messages_.erase(messages_.begin());
    }
    Message m;
    m.text = text;
    m.kind = kind;
    m.life = Config::MessageSeconds;
    messages_.push_back(m);
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

Target World::FindNearestAny(const Vec2& from, float maxRange) const {
    Target best;
    float bestDistSq = maxRange * maxRange;
    for (const Enemy& e : enemies_) {
        const float d = (e.pos - from).LengthSq();
        if (e.alive && d <= bestDistSq) {
            best.kind = TargetKind::Enemy;
            best.id = e.id;
            best.pos = e.pos;
            best.vel = e.vel;
            bestDistSq = d;
        }
    }
    for (const Bullet& b : enemyBullets_) {
        const float d = (b.pos - from).LengthSq();
        if (b.alive && d <= bestDistSq) {
            best.kind = TargetKind::EnemyBullet;
            best.id = b.id;
            best.pos = b.pos;
            best.vel = b.vel;
            bestDistSq = d;
        }
    }
    return best;
}

Target World::FindHighestHpTarget(const Vec2& from) const {
    Target best;
    float bestHp = 0.0f;
    for (const Enemy& e : enemies_) {
        if (e.alive && (best.kind == TargetKind::None || e.hp > bestHp)) {
            best.kind = TargetKind::Enemy;
            best.id = e.id;
            best.pos = e.pos;
            best.vel = e.vel;
            bestHp = e.hp;
        }
    }
    if (best.kind != TargetKind::None) {
        return best;
    }
    // 敵の弾はどれもHP1なので、最も近いものを選ぶ
    return FindNearestTarget(from);
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

void World::FireEnemyBullet(const Vec2& pos, const Vec2& vel, float radius, float damage) {
    Bullet b;
    b.id = nextId_++;
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
    // 強敵を倒して報酬を選ぶ間は、時間を止める
    for (int i = 0; i < steps && !gameOver_ && !HasPendingReward(); ++i) {
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

    // 強敵は、強敵がいない時間が一定続くと現れる（1体いる間は次は現れない）
    if (FindBoss() == nullptr) {
        bossTimer_ -= dt;
        if (bossTimer_ <= 0.0f) {
            SpawnBoss();
        }
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
    MovePickups(dt);
    AgeMessages(dt);
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
    PlaceAtScreenEdge(e);
    enemies_.push_back(e);
}

void World::SpawnBoss() {
    Enemy e;
    e.id = nextId_++;
    e.type = EnemyType::Boss;
    e.radius = Config::BossRadius;
    e.speed = Config::BossSpeed;
    e.maxHp = Config::BossHp * (1.0f + elapsed_ / Config::EnemyHpGrowthSeconds);
    e.hp = e.maxHp;
    e.shotTimer = Config::BossShotInterval;
    PlaceAtScreenEdge(e);
    enemies_.push_back(e);
    bossTimer_ = Config::BossInterval;
}

void World::PlaceAtScreenEdge(Enemy& e) {
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
}

const Enemy* World::FindBoss() const {
    for (const Enemy& e : enemies_) {
        if (e.alive && e.type == EnemyType::Boss) {
            return &e;
        }
    }
    return nullptr;
}

bool World::IsBossWarning() const {
    return FindBoss() == nullptr && bossTimer_ <= Config::BossWarningSeconds;
}

void World::MakeRewardChoices() {
    // 候補の出方：まだ持っていない武装があれば1つは必ず武装。
    // 残りは、持っている武装の系統の強化パーツと、エネルギー上限解放パーツから選ぶ（同じものは並べない）
    rewardChoices_.clear();
    std::vector<WeaponKind> unowned;
    for (const WeaponKind& kind : WeaponCandidates()) {
        if (!Owns(kind)) {
            unowned.push_back(kind);
        }
    }
    if (!unowned.empty()) {
        Pickup p;
        p.kind = PickupKind::Weapon;
        p.weapon = unowned[std::uniform_int_distribution<size_t>(0, unowned.size() - 1)(rng_)];
        p.family = FamilyOf(p.weapon);
        rewardChoices_.push_back(p);
    }
    std::vector<Pickup> pool;
    Pickup energy;
    energy.kind = PickupKind::Energy;
    pool.push_back(energy);
    std::vector<WeaponFamily> families;
    for (WeaponKind kind : ownedWeapons_) {
        const WeaponFamily family = FamilyOf(kind);
        if (std::find(families.begin(), families.end(), family) != families.end()) continue;
        families.push_back(family);
        for (PartKind part : FamilyParts(family)) {
            Pickup p;
            p.kind = PickupKind::Part;
            p.family = family;
            p.part = part;
            pool.push_back(p);
        }
    }
    std::shuffle(pool.begin(), pool.end(), rng_);
    for (size_t i = 0; i < pool.size() && rewardChoices_.size() < 3; ++i) {
        rewardChoices_.push_back(pool[i]);
    }
}

void World::ChooseReward(size_t index) {
    if (index >= rewardChoices_.size()) {
        return;
    }
    const Pickup chosen = rewardChoices_[index];
    rewardChoices_.clear();
    Collect(chosen);
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
    for (Missile& m : missiles_) {
        const float maxTurn = m.turnDegrees * Pi / 180.0f * dt;
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
                target = m.targetsBullets ? FindNearestAny(m.pos, 1.0e9f) : FindNearestTarget(m.pos);
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

void World::MovePickups(float dt) {
    for (Pickup& p : pickups_) {
        p.age += dt;
        if (p.age < Config::PickupWaitSeconds) {
            continue;  // 落ちた場所に少しとどまる
        }
        // コアへ吸い寄せられ、だんだん速くなる
        p.speed = std::min(Config::PickupMaxSpeed, p.speed + Config::PickupAcceleration * dt);
        const Vec2 toCore = core_.pos - p.pos;
        const float dist = toCore.Length();
        const float step = p.speed * dt;
        if (dist <= core_.radius + Config::PickupRadius || dist <= step) {
            p.alive = false;
            Collect(p);
        } else {
            p.pos += toCore * (step / dist);
        }
    }
}

void World::AgeMessages(float dt) {
    for (Message& m : messages_) {
        m.life -= dt;
    }
    messages_.erase(std::remove_if(messages_.begin(), messages_.end(), [](const Message& m) { return m.life <= 0.0f; }),
                    messages_.end());
}

void World::DropItems(const Enemy& e) {
    // 見えない経験値がたまったら、エネルギー上限解放パーツを落とす
    exp_ += (e.type == EnemyType::Evader) ? Config::EvaderExp : 1;
    while (exp_ >= expNeed_) {
        exp_ -= expNeed_;
        expNeed_ += Config::ExpNeedGrowth;
        Pickup p;
        p.kind = PickupKind::Energy;
        p.pos = e.pos + Vec2(RandomRange(-8.0f, 8.0f), RandomRange(-8.0f, 8.0f));
        p.speed = Config::PickupStartSpeed;
        pickups_.push_back(p);
    }

    // 一定の確率で、武装か強化パーツを落とす。武装をランダムに1つ選び、持っていなければその武装、持っていれば強化パーツにする
    if (RandomRange(0.0f, 1.0f) >= Config::DropChance) {
        return;
    }
    Pickup p;
    const std::vector<WeaponKind> candidates = WeaponCandidates();
    p.weapon = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(rng_)];
    p.family = FamilyOf(p.weapon);
    if (!Owns(p.weapon)) {
        p.kind = PickupKind::Weapon;
    } else {
        p.kind = PickupKind::Part;
        const auto& parts = FamilyParts(p.family);
        p.part = parts[std::uniform_int_distribution<size_t>(0, parts.size() - 1)(rng_)];
    }
    p.pos = e.pos + Vec2(RandomRange(-8.0f, 8.0f), RandomRange(-8.0f, 8.0f));
    p.speed = Config::PickupStartSpeed;
    pickups_.push_back(p);
}

bool World::Owns(const WeaponKind& kind) const {
    return std::find(ownedWeapons_.begin(), ownedWeapons_.end(), kind) != ownedWeapons_.end();
}

std::vector<WeaponKind> World::WeaponCandidates() const {
    // すべての単装に、持っている武装の1つ上の連装段階を加える（連装型は順番にしか手に入らない）
    std::vector<WeaponKind> candidates;
    for (int v = 0; v < WeaponVariantCount; ++v) {
        candidates.push_back(WeaponKind{ static_cast<WeaponVariant>(v), 1 });
    }
    for (const WeaponKind& owned : ownedWeapons_) {
        const WeaponKind next = NextBarrelStep(owned);
        if (next.barrels > 0 && std::find(candidates.begin(), candidates.end(), next) == candidates.end()) {
            candidates.push_back(next);
        }
    }
    return candidates;
}

void World::DamageEnemy(Enemy& e, float damage) {
    if (!e.alive) {
        return;
    }
    e.hp -= damage;
    if (e.hp <= 0.0f) {
        e.alive = false;
        if (e.type == EnemyType::Boss) {
            // 強敵は落とし物を落とさず、代わりに報酬を選べる
            ++bossKills_;
            MakeRewardChoices();
        } else {
            ++kills_;
            DropItems(e);
        }
    }
}

bool World::HitWithBullet(Bullet& b, int targetId) {
    // 貫徹力強化を付けた速射砲の弾は、確率で貫いて先へ進む（威力は下がる）
    if (b.pierceChance > 0.0f && RandomRange(0.0f, 1.0f) < b.pierceChance) {
        b.damage *= Config::RapidPierceDecay;
        b.lastHitId = targetId;
        return true;
    }
    b.alive = false;
    return false;
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
    AddExplosion(m.pos, m.blastRadius, Config::MissileExplosionDamage, Config::MissileExplosionSeconds);
}

void World::BurstShell(Shell& s) {
    if (!s.alive) return;
    s.alive = false;
    // その場に残る大きな爆炎
    AddExplosion(s.pos, s.flameRadius, Config::ShellFlameDamage * s.damageScale, s.flameSeconds);
    // 進んでいた方向の前方へ、円錐形に破片をまく
    const float baseAngle = std::atan2(s.vel.y, s.vel.x);
    const float spread = Config::ShellFragmentSpreadDegrees * Pi / 180.0f;
    for (int i = 0; i < s.fragmentCount; ++i) {
        const float angle = baseAngle + RandomRange(-spread, spread);
        Bullet f;
        f.pos = s.pos;
        f.vel = Vec2::FromAngle(angle) * (Config::ShellFragmentSpeed * RandomRange(0.8f, 1.2f));
        f.radius = Config::FragmentRadius;
        f.damage = Config::ShellFragmentDamage * s.damageScale;
        f.life = Config::ShellFragmentSeconds;
        playerBullets_.push_back(f);
    }
}

void World::HandleCollisions() {
    // 速射砲の弾・破片：敵の弾とぶつかると敵の弾は消える。敵本体に当たると威力を与える。
    // 弾はふつうそこで消えるが、貫徹力強化があれば確率で貫いて先へ進む
    for (Bullet& pb : playerBullets_) {
        if (!pb.alive) continue;
        for (Bullet& eb : enemyBullets_) {
            if (eb.alive && Overlaps(pb.pos, pb.radius, eb.pos, eb.radius)) {
                eb.alive = false;
                if (!HitWithBullet(pb, eb.id)) break;
            }
        }
        if (!pb.alive) continue;
        for (Enemy& e : enemies_) {
            if (e.alive && e.id != pb.lastHitId && Overlaps(pb.pos, pb.radius, e.pos, e.radius)) {
                DamageEnemy(e, pb.damage);
                if (!HitWithBullet(pb, e.id)) break;
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
                b.damage *= b.decay;
            }
        }
        for (Enemy& e : enemies_) {
            if (!e.alive || Contains(b.hitIds, e.id)) continue;
            const float r = e.radius + Config::BeamHalfWidth;
            if (DistanceSqToSegment(e.pos, tail, b.pos) <= r * r) {
                b.hitIds.push_back(e.id);
                DamageEnemy(e, b.damage);
                b.damage *= b.decay;
            }
        }
    }

    // 誘導弾：敵本体や敵の弾に当たると、消えて小さな爆発を起こす
    for (Missile& m : missiles_) {
        if (!m.alive) continue;
        for (Enemy& e : enemies_) {
            if (e.alive && Overlaps(m.pos, Config::MissileRadius, e.pos, e.radius)) {
                DamageEnemy(e, m.damage);
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
                DamageEnemy(e, Config::ShellDirectDamage * s.damageScale);
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

    // 敵本体がコアにぶつかる（体当たり）。ぶつかった敵は消える。強敵は消えず、少し間をおいて何度でもダメージを与える
    for (Enemy& e : enemies_) {
        if (e.alive && Overlaps(e.pos, e.radius, core_.pos, core_.radius)) {
            if (e.type == EnemyType::Boss) {
                if (e.ramCooldown <= 0.0f) {
                    core_.hp -= Config::BossRamDamage;
                    e.ramCooldown = Config::BossRamCooldown;
                }
            } else {
                e.alive = false;
                core_.hp -= Config::ChargerRamDamage;
            }
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
    EraseDead(pickups_);
}
