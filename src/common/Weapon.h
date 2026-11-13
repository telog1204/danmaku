#pragma once

class World;

// 速射砲（基本型）。敵本体を優先して最も近いものを狙い、ほんの少しばらけた弾を連射する。
// 敵本体がいないときだけ、最も近い敵の弾を狙う
class RapidGun {
public:
    void Reset();
    void Update(World& world, float dt);

private:
    float cooldown_ = 0.0f;
};
