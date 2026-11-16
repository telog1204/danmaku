#include "pch.h"
#include "World.h"
#include <DxLib.h>
#include "Config.h"
#include "CrashState.h"
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

    // フィールドの外へこれだけ出た弾は消す
    constexpr float CullMargin = 40.0f;

    bool IsFarOutside(const Vec2& p) {
        return p.x < -CullMargin || p.y < -CullMargin ||
               p.x > Config::FieldWidth + CullMargin || p.y > Config::FieldHeight + CullMargin;
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
    core_.pos = Vec2(Config::FieldWidth * 0.5f, Config::FieldHeight * 0.5f);
    core_.radius = Config::CoreRadius;
    core_.maxHp = Config::CoreMaxHp;
    core_.hp = core_.maxHp;

    // 開始時は、速射砲に加えてランダムに選んだ武装を1つ持つ。
    // 最初はエネルギー上限に収まるように自動で積む（ランダムな武装を1つ、残ったエネルギーで速射砲を積めるだけ）
    weapons_.clear();
    const std::vector<WeaponKind> choices = StartWeaponChoices();
    const WeaponKind start = choices[std::uniform_int_distribution<size_t>(0, choices.size() - 1)(rng_)];
    ownedWeapons_ = { RapidBasicKind, start };
    SortOwnedWeapons();
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
    messages_.clear();
    rewardChoices_.clear();
    pendingEnemies_.clear();
    grandFleetTimer_ = Config::GrandFleetInterval;
    kills_ = 0;
    fleetKills_ = 0;
    nextId_ = 1;
    elapsed_ = 0.0f;
    lightCrowdTime_ = -1.0f;
    lightCrowdFound_ = false;
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
        energyCap_ += Config::EnergyPerUnlock;
        AddMessage("エネルギー上限が" + std::to_string(Config::EnergyPerUnlock) + "増えた（上限 " + std::to_string(energyCap_) + "）",
                   pickup.kind);
        break;
    }
    case PickupKind::Weapon:
        if (!Owns(pickup.weapon)) {
            ownedWeapons_.push_back(pickup.weapon);
            SortOwnedWeapons();
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

std::vector<Target> World::FindNearestCandidates(const Vec2& from, float maxRange, bool bodiesFirst, size_t count,
                                                 const std::function<bool(const Target&)>& accept) const {
    std::vector<std::pair<float, Target>> found;
    const float rangeSq = maxRange * maxRange;
    for (const Enemy& e : enemies_) {
        const float d = (e.pos - from).LengthSq();
        if (e.alive && d <= rangeSq) {
            Target t;
            t.kind = TargetKind::Enemy;
            t.id = e.id;
            t.pos = e.pos;
            t.vel = e.vel;
            t.light = (e.cls == EnemyClass::Light);
            if (!accept || accept(t)) {
                found.emplace_back(d, t);
            }
        }
    }
    if (!bodiesFirst || found.empty()) {
        for (const Bullet& b : enemyBullets_) {
            const float d = (b.pos - from).LengthSq();
            if (b.alive && d <= rangeSq) {
                Target t;
                t.kind = TargetKind::EnemyBullet;
                t.id = b.id;
                t.pos = b.pos;
                t.vel = b.vel;
                if (!accept || accept(t)) {
                    found.emplace_back(d, t);
                }
            }
        }
    }
    const size_t n = std::min(count, found.size());
    std::partial_sort(found.begin(), found.begin() + static_cast<std::ptrdiff_t>(n), found.end(),
                      [](const std::pair<float, Target>& a, const std::pair<float, Target>& b) { return a.first < b.first; });
    std::vector<Target> result;
    for (size_t i = 0; i < n; ++i) {
        result.push_back(found[i].second);
    }
    return result;
}

namespace {
    bool Contains(const std::vector<Target>& list, const Target& t) {
        for (const Target& x : list) {
            if (x.SameAs(t)) return true;
        }
        return false;
    }

    using TargetFilter = std::function<bool(const Target&)>;

    // 狙う相手の優先の仕方を、上から順に見る区分の並びにする
    std::vector<TargetFilter> PriorityTiers(TargetPriority priority) {
        const auto isBullet = [](const Target& t) { return t.kind == TargetKind::EnemyBullet; };
        switch (priority) {
        case TargetPriority::LightAndBullets:
            return { [isBullet](const Target& t) { return isBullet(t) || t.light; } };
        case TargetPriority::MediumFirst:
            return { [](const Target& t) { return t.kind == TargetKind::Enemy && !t.light; },
                     [](const Target& t) { return t.kind == TargetKind::Enemy && t.light; },
                     isBullet };
        case TargetPriority::Any:
            break;
        }
        return { [](const Target&) { return true; } };
    }
}

Target World::PickLockOnTarget(const Vec2& from, float maxRange, TargetPriority priority, const std::vector<Target>& exclude,
                               const float* keepAimFacing) {
    for (const TargetFilter& tier : PriorityTiers(priority)) {
        // この区分の相手がいなければ、次の区分を見る
        const std::vector<Target> inTier = FindNearestCandidates(from, maxRange, false, static_cast<size_t>(-1), tier);
        if (inTier.empty()) continue;

        // 1回目は exclude の相手を外して選び、外すと候補がなくなるときは、ほかの武装と重なっても選ぶ
        for (int pass = 0; pass < 2; ++pass) {
            std::vector<Target> pool;
            for (const Target& t : inTier) {
                if (pass == 1 || !Contains(exclude, t)) pool.push_back(t);
            }
            if (pool.empty()) continue;
            if (keepAimFacing != nullptr) {
                // 旋回せずに狙える相手（今の向きから一定の角度以内で、角度の差が最も小さいもの）を優先する
                Target best;
                float bestDiff = Config::TurretKeepAimDegrees * Pi / 180.0f;
                for (const Target& t : pool) {
                    const Vec2 to = t.pos - from;
                    const float diff = std::fabs(WrapAngle(std::atan2(to.y, to.x) - *keepAimFacing));
                    if (diff <= bestDiff) {
                        best = t;
                        bestDiff = diff;
                    }
                }
                if (best.kind != TargetKind::None) return best;
            }
            // 近い順に3番目までの中からランダムに選ぶ（inTier は近い順に並んでいる）
            const size_t n = std::min(pool.size(), Config::LockOnCandidates);
            return pool[std::uniform_int_distribution<size_t>(0, n - 1)(rng_)];
        }
    }
    return Target();
}

std::vector<Target> World::TargetsOfSameKind(const Weapon* self) const {
    std::vector<Target> targets;
    for (const auto& w : weapons_) {
        // 派生版が同じなら、連装の段階が違っても同じ種類として扱う
        if (w.get() == self || w->Kind().variant != self->Kind().variant) continue;
        const Target t = w->CurrentTarget();
        if (t.kind != TargetKind::None) {
            targets.push_back(t);
        }
    }
    return targets;
}

namespace {
    Target TargetOf(const Enemy& e) {
        Target t;
        t.kind = TargetKind::Enemy;
        t.id = e.id;
        t.pos = e.pos;
        t.vel = e.vel;
        t.light = (e.cls == EnemyClass::Light);
        return t;
    }
}

Target World::FindHighestHpTarget(const Vec2& from, const std::vector<Target>& exclude, bool mediumFirst,
                                  const Target& keep) const {
    // 調べる区分：mediumFirst なら 中・重 → 軽、そうでなければ敵本体すべて
    for (int tier = 0; tier < (mediumFirst ? 2 : 1); ++tier) {
        const auto inTier = [mediumFirst, tier](const Enemy& e) {
            return !mediumFirst || ((e.cls == EnemyClass::Light) == (tier == 1));
        };
        // 1回目は exclude の相手を除いて探し、見つからなければ2回目は除かずに探す
        for (int pass = 0; pass < 2; ++pass) {
            const Enemy* best = nullptr;
            float bestDistSq = 0.0f;
            for (const Enemy& e : enemies_) {
                if (!e.alive || !inTier(e)) continue;
                if (pass == 0 && Contains(exclude, TargetOf(e))) continue;
                const float d = (e.pos - from).LengthSq();
                // 最大HPで比べる（削られても変わらないので、一度選んだ相手を倒すまで狙い続けやすい）。同じなら近い方
                if (best == nullptr || e.maxHp > best->maxHp || (e.maxHp == best->maxHp && d < bestDistSq)) {
                    best = &e;
                    bestDistSq = d;
                }
            }
            if (best == nullptr) continue;
            if (keep.kind == TargetKind::Enemy && keep.id != best->id) {
                for (const Enemy& e : enemies_) {
                    if (e.alive && e.id == keep.id && inTier(e) && e.maxHp == best->maxHp) {
                        return TargetOf(e);  // 今狙っている相手も最も高ければ、狙い続ける
                    }
                }
            }
            return TargetOf(*best);
        }
    }
    // 敵本体がいなければ、敵の弾はどれもHP1なので、最も近いものを選ぶ
    return FindNearestTarget(from);
}

Target World::FindPierceTarget(const Vec2& from, const std::vector<Target>& exclude) const {
    // ビームの線の上に乗る数を数える（コアから aim へ向かう線。手前も背後も含め、コアより後ろは含めない）
    const auto score = [this, &from](const Vec2& aim) {
        const Vec2 dir = (aim - from).Normalized();
        float total = 0.0f;
        const auto onLine = [&from, &dir](const Vec2& p, float radius) {
            const Vec2 rel = p - from;
            const float along = rel.Dot(dir);
            if (along < 0.0f) return false;
            const float r = radius + Config::BeamHalfWidth;
            return (rel - dir * along).LengthSq() <= r * r;
        };
        for (const Enemy& e : enemies_) {
            if (e.alive && onLine(e.pos, e.radius)) total += 1.0f;
        }
        for (const Bullet& b : enemyBullets_) {
            if (b.alive && onLine(b.pos, b.radius)) total += Config::BeamPierceBulletWeight;
        }
        return total;
    };
    // 中・重 → 軽 の順に、いる区分から選ぶ
    for (int tier = 0; tier < 2; ++tier) {
        for (int pass = 0; pass < 2; ++pass) {
            const Enemy* best = nullptr;
            float bestScore = 0.0f;
            float bestDistSq = 0.0f;
            for (const Enemy& e : enemies_) {
                if (!e.alive || (e.cls == EnemyClass::Light) != (tier == 1)) continue;
                if (pass == 0 && Contains(exclude, TargetOf(e))) continue;
                const float s = score(e.pos);
                const float d = (e.pos - from).LengthSq();
                if (best == nullptr || s > bestScore || (s == bestScore && d < bestDistSq)) {
                    best = &e;
                    bestScore = s;
                    bestDistSq = d;
                }
            }
            if (best != nullptr) return TargetOf(*best);
        }
    }
    // 敵本体がいなければ、最も近い敵の弾
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

bool World::FindLightCrowdCenter(Vec2& center) {
    if (lightCrowdTime_ < 0.0f || elapsed_ - lightCrowdTime_ >= Config::CrowdRefreshSeconds || elapsed_ < lightCrowdTime_) {
        lightCrowdTime_ = elapsed_;
        // 軽の敵と敵の弾の位置を集め、周り（CrowdRadius 以内）に最も多く集まっている所の平均の位置を中心とする
        std::vector<Vec2> points;
        for (const Enemy& e : enemies_) {
            if (e.alive && e.cls == EnemyClass::Light) points.push_back(e.pos);
        }
        for (const Bullet& b : enemyBullets_) {
            if (b.alive) points.push_back(b.pos);
        }
        const float r2 = Config::CrowdRadius * Config::CrowdRadius;
        int bestCount = 0;
        for (const Vec2& p : points) {
            int count = 0;
            Vec2 sum;
            for (const Vec2& o : points) {
                if ((o - p).LengthSq() <= r2) {
                    ++count;
                    sum += o;
                }
            }
            if (count > bestCount) {
                bestCount = count;
                lightCrowdCenter_ = sum * (1.0f / count);
            }
        }
        lightCrowdFound_ = bestCount > 0;
    }
    center = lightCrowdCenter_;
    return lightCrowdFound_;
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
    CrashState::SetElapsed(elapsed_);

    CrashState::SetPhase(CrashState::Phase::Spawn);
    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0.0f) {
        SpawnEnemy();
        spawnTimer_ = std::max(Config::SpawnIntervalMin,
                               Config::SpawnIntervalStart - elapsed_ * Config::SpawnIntervalShrinkPerSecond);
    }

    // 大艦隊は、大艦隊がいない時間が一定続くと現れる（旗艦が残っている間は次は現れない）
    if (!IsGrandFleetActive()) {
        grandFleetTimer_ -= dt;
        if (grandFleetTimer_ <= 0.0f) {
            SpawnGrandFleet();
        }
    }

    CrashState::SetPhase(CrashState::Phase::Weapons);
    for (auto& w : weapons_) {
        w->Update(*this, dt);
    }

    CrashState::SetPhase(CrashState::Phase::Enemies);
    // 敵の動きの途中で生み出された敵（空母型）は pendingEnemies_ にためておき、動きの処理が終わってから加える
    for (Enemy& e : enemies_) {
        UpdateEnemy(e, *this, dt);
    }
    for (const Enemy& e : pendingEnemies_) {
        enemies_.push_back(e);
    }
    pendingEnemies_.clear();

    CrashState::SetPhase(CrashState::Phase::MoveBullets);
    MoveBullets(playerBullets_, dt);
    MoveEnemyBullets(dt);
    CrashState::SetPhase(CrashState::Phase::MoveBeams);
    MoveBeams(dt);
    CrashState::SetPhase(CrashState::Phase::MoveMissiles);
    MoveMissiles(dt);
    CrashState::SetPhase(CrashState::Phase::MoveShells);
    MoveShells(dt);
    CrashState::SetPhase(CrashState::Phase::Explosions);
    AgeExplosions(dt);
    CrashState::SetPhase(CrashState::Phase::Collisions);
    HandleCollisions();
    CrashState::SetPhase(CrashState::Phase::Messages);
    AgeMessages(dt);
    CrashState::SetPhase(CrashState::Phase::Cleanup);
    RemoveDead();
    CrashState::SetCounts(static_cast<int>(enemies_.size()), static_cast<int>(enemyBullets_.size()),
                          static_cast<int>(playerBullets_.size()), static_cast<int>(missiles_.size()),
                          static_cast<int>(beams_.size()), static_cast<int>(shells_.size()),
                          static_cast<int>(explosions_.size()));

    if (core_.hp <= 0) {
        core_.hp = 0;
        gameOver_ = true;
        gameOverSeconds_ = 0.0f;
    }
}

namespace {
    float Lerp(float a, float b, float t) { return a + (b - a) * std::max(0.0f, std::min(1.0f, t)); }
}

void World::SpawnEnemy() {
    // 軽の群れか中を選ぶ（時間が経つほど中が増える）。中の基本型・雷撃型は、ある確率で艦隊になる
    const float t = elapsed_ / Config::RampSeconds;
    const float lightW = Lerp(Config::LightGroupWeightStart, Config::LightGroupWeightEnd, t);
    const float mediumW = Lerp(Config::MediumWeightStart, Config::MediumWeightEnd, t);
    if (RandomRange(0.0f, lightW + mediumW) < lightW) {
        // 軽の群れ：同じ型が3〜5体まとまって
        const EnemyType type = (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) ? EnemyType::Strafer : EnemyType::Kamikaze;
        const int count = std::uniform_int_distribution<int>(Config::LightGroupMin, Config::LightGroupMax)(rng_);
        SpawnGroupAtEdge(EnemyClass::Light, std::vector<EnemyType>(static_cast<size_t>(count), type));
        return;
    }
    static const EnemyType mediumTypes[] = { EnemyType::Gunship, EnemyType::Torpedo, EnemyType::Carrier };
    const EnemyType type = mediumTypes[std::uniform_int_distribution<int>(0, 2)(rng_)];
    if (type != EnemyType::Carrier && RandomRange(0.0f, 1.0f) < Lerp(Config::FleetChanceStart, Config::FleetChanceEnd, t)) {
        // 艦隊：旗艦（重）1体と、中の基本型・雷撃型3体
        std::vector<EnemyType> escorts;
        for (int i = 0; i < Config::FleetEscorts; ++i) {
            escorts.push_back((std::uniform_int_distribution<int>(0, 1)(rng_) == 0) ? EnemyType::Gunship : EnemyType::Torpedo);
        }
        Vec2 outward;
        const Vec2 base = RandomEdgePoint(outward);
        SpawnEnemyAt(EnemyClass::Heavy, mediumTypes[std::uniform_int_distribution<int>(0, 2)(rng_)], base);
        const Vec2 side = outward.Perpendicular();
        for (size_t i = 0; i < escorts.size(); ++i) {
            const float offset = (static_cast<float>(i) - (escorts.size() - 1) * 0.5f) * Config::GroupSpread * 2.0f;
            SpawnEnemyAt(EnemyClass::Medium, escorts[i], base + side * offset + outward * Config::GroupSpread * 1.5f);
        }
        return;
    }
    SpawnGroupAtEdge(EnemyClass::Medium, { type });
}

Vec2 World::RandomEdgePoint(Vec2& outward) {
    // フィールドの四辺のどこかの、少し外側の点（outward はフィールドの外へ向かう向き）
    const float w = static_cast<float>(Config::FieldWidth);
    const float h = static_cast<float>(Config::FieldHeight);
    const float m = Config::SpawnMargin;
    const float t = RandomRange(0.0f, 2.0f * (w + h));
    if (t < w) {
        outward = Vec2(0.0f, -1.0f);
        return Vec2(t, -m);
    }
    if (t < w + h) {
        outward = Vec2(1.0f, 0.0f);
        return Vec2(w + m, t - w);
    }
    if (t < 2.0f * w + h) {
        outward = Vec2(0.0f, 1.0f);
        return Vec2(t - (w + h), h + m);
    }
    outward = Vec2(-1.0f, 0.0f);
    return Vec2(-m, t - (2.0f * w + h));
}

void World::SpawnGroupAtEdge(EnemyClass cls, const std::vector<EnemyType>& types) {
    // 1か所からまとまって現れる（辺に沿って少しずつずらし、外側へ少し重ねる）
    Vec2 outward;
    const Vec2 base = RandomEdgePoint(outward);
    const Vec2 side = outward.Perpendicular();
    for (size_t i = 0; i < types.size(); ++i) {
        const float offset = (static_cast<float>(i) - (types.size() - 1) * 0.5f) * Config::GroupSpread;
        const Vec2 jitter = outward * RandomRange(0.0f, Config::GroupSpread);
        SpawnEnemyAt(cls, types[i], base + side * offset + jitter);
    }
}

Enemy& World::SpawnEnemyAt(EnemyClass cls, EnemyType type, const Vec2& pos) {
    Enemy e;
    e.id = nextId_++;
    e.cls = cls;
    e.type = type;
    e.pos = pos;
    // 時間が経つほどHPが増える（軽は増えない）
    const float hpScale = 1.0f + elapsed_ / Config::EnemyHpGrowthSeconds;
    switch (cls) {
    case EnemyClass::Light:
        e.radius = Config::LightRadius;
        e.cruiseSpeed = Config::LightSpeed;
        e.turnDegrees = Config::LightTurnDegrees;
        e.maxHp = 1.0f;
        e.defense = 0.0f;
        break;
    case EnemyClass::Medium:
        e.radius = Config::MediumRadius;
        e.cruiseSpeed = Config::MediumSpeed;
        e.turnDegrees = Config::MediumTurnDegrees;
        e.maxHp = Config::MediumHp * hpScale;
        e.defense = Config::MediumDefense;
        break;
    case EnemyClass::Heavy:
        e.radius = Config::HeavyRadius;
        e.cruiseSpeed = Config::HeavySpeed;
        e.turnDegrees = Config::HeavyTurnDegrees;
        e.maxHp = Config::HeavyHp * hpScale;
        e.defense = Config::HeavyDefense;
        break;
    }
    e.hp = e.maxHp;
    e.speed = e.cruiseSpeed;
    e.orbitDir = (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) ? 1.0f : -1.0f;
    e.shotTimer = RandomRange(0.5f, 1.5f);
    // 最初はコアの方へ機首を向けて現れる
    const Vec2 toCore = core_.pos - e.pos;
    e.heading = std::atan2(toCore.y, toCore.x);
    e.vel = Vec2::FromAngle(e.heading) * e.speed;
    pendingEnemies_.push_back(e);
    return pendingEnemies_.back();
}

void World::LaunchLightGroup(const Enemy& carrier, int count) {
    // 空母型の位置から、同じ型の軽の敵をまとめて生み出す
    const EnemyType type = (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) ? EnemyType::Strafer : EnemyType::Kamikaze;
    for (int i = 0; i < count; ++i) {
        const Vec2 offset = Vec2::FromAngle(RandomRange(0.0f, 6.2831853f)) * RandomRange(0.0f, carrier.radius);
        Enemy& e = SpawnEnemyAt(EnemyClass::Light, type, carrier.pos + offset);
        e.parentId = carrier.id;
        e.entered = carrier.entered;
    }
}

int World::CountChildren(int parentId) const {
    int n = 0;
    for (const Enemy& e : enemies_) {
        if (e.alive && e.parentId == parentId) ++n;
    }
    for (const Enemy& e : pendingEnemies_) {
        if (e.alive && e.parentId == parentId) ++n;
    }
    return n;
}

void World::FireEnemyMissile(const Vec2& pos, float heading) {
    Bullet b;
    b.id = nextId_++;
    b.pos = pos;
    b.vel = Vec2::FromAngle(heading) * Config::EnemyMissileSpeed;
    b.radius = Config::EnemyMissileRadius;
    b.damage = static_cast<float>(Config::EnemyMissileDamage);
    b.life = Config::EnemyMissileLifeSeconds;
    b.homing = true;
    enemyBullets_.push_back(b);
}

void World::SpawnGrandFleet() {
    // 大艦隊：旗艦（重）と、中の基本型・雷撃型が、1つの辺からまとまって現れる
    static const EnemyType heavyTypes[] = { EnemyType::Gunship, EnemyType::Torpedo, EnemyType::Carrier };
    Vec2 outward;
    const Vec2 base = RandomEdgePoint(outward);
    const Vec2 side = outward.Perpendicular();
    for (int i = 0; i < Config::GrandFleetFlagships; ++i) {
        const float offset = (static_cast<float>(i) - (Config::GrandFleetFlagships - 1) * 0.5f) * Config::GroupSpread * 3.0f;
        Enemy& e = SpawnEnemyAt(EnemyClass::Heavy, heavyTypes[std::uniform_int_distribution<int>(0, 2)(rng_)],
                                base + side * offset);
        e.grandFlagship = true;
    }
    for (int i = 0; i < Config::GrandFleetEscorts; ++i) {
        const float offset = (static_cast<float>(i) - (Config::GrandFleetEscorts - 1) * 0.5f) * Config::GroupSpread * 2.0f;
        const EnemyType type = (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) ? EnemyType::Gunship : EnemyType::Torpedo;
        SpawnEnemyAt(EnemyClass::Medium, type, base + side * offset + outward * Config::GroupSpread * 2.0f);
    }
    grandFleetTimer_ = Config::GrandFleetInterval;
}

bool World::IsGrandFleetActive() const {
    for (const Enemy& e : enemies_) {
        if (e.alive && e.grandFlagship) return true;
    }
    for (const Enemy& e : pendingEnemies_) {
        if (e.alive && e.grandFlagship) return true;
    }
    return false;
}

bool World::IsGrandFleetWarning() const {
    return !IsGrandFleetActive() && grandFleetTimer_ <= Config::GrandFleetWarningSeconds;
}

float World::GrandFleetHpRatio() const {
    float hp = 0.0f;
    float maxHp = 0.0f;
    for (const Enemy& e : enemies_) {
        if (e.grandFlagship) {
            hp += e.alive ? std::max(0.0f, e.hp) : 0.0f;
            maxHp += e.maxHp;
        }
    }
    return maxHp > 0.0f ? hp / maxHp : 0.0f;
}

void World::CheckGrandFleetDefeated() {
    // 旗艦が全部いなくなったら、報酬を選べる
    if (!IsGrandFleetActive()) {
        ++fleetKills_;
        MakeRewardChoices();
    }
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

void World::MoveEnemyBullets(float dt) {
    // 敵の誘導弾は、コアのほうへ曲がりながら飛ぶ（ほかの敵の弾はまっすぐ）
    const float maxTurn = Config::EnemyMissileTurnDegrees * Pi / 180.0f * dt;
    for (Bullet& b : enemyBullets_) {
        if (!b.homing) continue;
        const float heading = std::atan2(b.vel.y, b.vel.x);
        const Vec2 to = core_.pos - b.pos;
        const float diff = WrapAngle(std::atan2(to.y, to.x) - heading);
        b.vel = Vec2::FromAngle(heading + std::max(-maxTurn, std::min(maxTurn, diff))) * Config::EnemyMissileSpeed;
    }
    MoveBullets(enemyBullets_, dt);
}

void World::MoveBeams(float dt) {
    for (Beam& b : beams_) {
        const Vec2 step = b.vel * dt;
        b.pos += step;
        b.traveled += step.Length();
        // 尾までフィールドの外へ出たら消す
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
        // 撃ち出してしばらくはまっすぐ進み（近接防御型はすぐに）、そのあと前方に見えている相手を選んで追いかける
        if (m.age >= m.straightSeconds) {
            Target target = FindTargetById(m.targetKind, m.targetId);
            if (target.kind != TargetKind::None && !MissileCanSee(m, target.pos)) {
                target = Target();  // 追い越して後ろ側に回ったら見失う
            }
            if (target.kind == TargetKind::None) {
                // 狙う相手がいない・見失ったときは、前方に見えている相手から選び直す（いなければまっすぐ飛ぶ）
                target = AcquireMissileTarget(m);
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

bool World::MissileCanSee(const Missile& m, const Vec2& pos) const {
    const Vec2 to = pos - m.pos;
    if (to.LengthSq() <= 0.0f) return true;
    const float diff = WrapAngle(std::atan2(to.y, to.x) - m.heading);
    return std::fabs(diff) <= Config::MissileViewDegrees * 0.5f * Pi / 180.0f;
}

Target World::AcquireMissileTarget(const Missile& m) {
    // 撃った武装に関係なく、すべての誘導弾で同じ選び方をする。
    // 前方に見えている相手（敵本体と敵の弾を区別しない）を近い順に並べ、近い順3体のうち、
    // まだ上限の数の誘導弾に追いかけられていない相手からランダムに選ぶ。
    // 3体ともふさがっていれば、近い順に1体ずつ探す範囲を広げる。見つからなければ選ばない（まっすぐ進み、あとで探し直す）
    const std::vector<Target> visible = FindNearestCandidates(
        m.pos, 1.0e9f, false, static_cast<size_t>(-1), [this, &m](const Target& t) { return MissileCanSee(m, t.pos); });
    const auto isOpen = [this, &m](const Target& t) {
        int chasing = 0;
        for (const Missile& o : missiles_) {
            if (&o != &m && o.alive && o.targetKind == t.kind && o.targetId == t.id) {
                ++chasing;
            }
        }
        return chasing < Config::MissileTargetCap;
    };
    std::vector<Target> open;
    for (size_t i = 0; i < visible.size(); ++i) {
        if (isOpen(visible[i])) {
            open.push_back(visible[i]);
        }
        // 近い順3体まで見て、空いている相手が1体でもいれば、その中から選ぶ。いなければ範囲を1体ずつ広げる
        if (i + 1 >= Config::LockOnCandidates && !open.empty()) {
            break;
        }
    }
    if (open.empty()) {
        return Target();
    }
    return open[std::uniform_int_distribution<size_t>(0, open.size() - 1)(rng_)];
}

void World::AgeMessages(float dt) {
    for (Message& m : messages_) {
        m.life -= dt;
    }
    messages_.erase(std::remove_if(messages_.begin(), messages_.end(), [](const Message& m) { return m.life <= 0.0f; }),
                    messages_.end());
}

void World::DropItems(const Enemy& e) {
    // 倒した瞬間に手に入る（落とし物を弾と見間違えないように、フィールドには出さない）
    // 見えない経験値がたまったら、エネルギー上限解放パーツが手に入る
    switch (e.cls) {
    case EnemyClass::Light: exp_ += Config::LightExp; break;
    case EnemyClass::Medium: exp_ += Config::MediumExp; break;
    case EnemyClass::Heavy: exp_ += Config::HeavyExp; break;
    }
    while (exp_ >= expNeed_) {
        exp_ -= expNeed_;
        expNeed_ += Config::ExpNeedGrowth;
        Pickup p;
        p.kind = PickupKind::Energy;
        Collect(p);
    }

    // 一定の確率で、武装か強化パーツが手に入る。武装をランダムに1つ選び、持っていなければその武装、持っていれば強化パーツにする
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
    Collect(p);
}

void World::SortOwnedWeapons() {
    // 系統 → 派生版 → 連装の順（派生版の並びは系統ごとにまとまっている）
    std::sort(ownedWeapons_.begin(), ownedWeapons_.end(), [](const WeaponKind& a, const WeaponKind& b) {
        if (a.variant != b.variant) return static_cast<int>(a.variant) < static_cast<int>(b.variant);
        return a.barrels < b.barrels;
    });
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
    // 受けるダメージ＝攻撃力−防御力（最低でも攻撃力の1割は通る）。軽はどの攻撃でも1発で倒れる
    const float dealt = std::max(damage * Config::MinDamageRate, damage - e.defense);
    e.hp -= (e.cls == EnemyClass::Light) ? e.hp : dealt;
    if (e.hp <= 0.0f) {
        e.alive = false;
        if (e.grandFlagship) {
            // 大艦隊の旗艦は落とし物を落とさず、全部倒すと報酬を選べる
            CheckGrandFleetDefeated();
        } else {
            ++kills_;
            DropItems(e);
        }
    }
}

void World::DestroyEnemyBullet(Bullet& b) {
    if (!b.alive) {
        return;
    }
    // 自分の攻撃で壊された敵の弾は、小さな爆発を起こして、周りの敵・敵の弾・誘導弾を巻き込む（連鎖する）
    b.alive = false;
    AddExplosion(b.pos, Config::EnemyBulletBlastRadius, Config::EnemyBulletBlastDamage, Config::EnemyBulletBlastSeconds);
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
                DestroyEnemyBullet(eb);
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
                DestroyEnemyBullet(eb);
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
                DestroyEnemyBullet(eb);
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
                DestroyEnemyBullet(eb);
                BurstShell(s);
                break;
            }
        }
    }

    // 爆発・爆炎：残っているあいだ、触れた敵（1体につき1回）・敵の弾・誘導弾を巻き込む。
    // 巻き込まれた誘導弾が起こす爆発も、同じフレームのうちに次々と広がる（誘爆）
    // 残っている爆炎も毎回判定するので、新しい爆発がなくても1回は回す
    // 1回目はすべての爆発を、2回目からは新しく起きた爆発だけを判定する（連鎖が長く続いても重くならないように）
    size_t firstNew = 0;
    do {
        for (Explosion& x : pendingExplosions_) {
            explosions_.push_back(x);
        }
        pendingExplosions_.clear();
        const size_t end = explosions_.size();
        for (size_t xi = firstNew; xi < end; ++xi) {
            Explosion& x = explosions_[xi];
            if (!x.alive) continue;
            for (Enemy& e : enemies_) {
                if (e.alive && !Contains(x.hitIds, e.id) && Overlaps(x.pos, x.radius, e.pos, e.radius)) {
                    x.hitIds.push_back(e.id);
                    DamageEnemy(e, x.damage);
                }
            }
            for (Bullet& eb : enemyBullets_) {
                if (eb.alive && Overlaps(x.pos, x.radius, eb.pos, eb.radius)) {
                    DestroyEnemyBullet(eb);
                }
            }
            for (Missile& m : missiles_) {
                // まっすぐ進んでいるあいだ（撃ち出した直後）は巻き込まれない
                if (m.alive && m.age >= m.straightSeconds && Overlaps(x.pos, x.radius, m.pos, Config::MissileRadius)) {
                    DetonateMissile(m);
                }
            }
        }
        firstNew = end;
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
            core_.hp -= (e.type == EnemyType::Kamikaze) ? Config::KamikazeRamDamage : Config::RamDamage;
            if (e.grandFlagship) {
                CheckGrandFleetDefeated();
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
}
