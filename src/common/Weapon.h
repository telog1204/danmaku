#pragma once
#include <memory>

class World;

// 武装の種類。組み込み画面で積み外しするときも、この種類で見分ける
enum class WeaponKind {
    RapidGun,  // 速射砲（基本型）
    Beam,      // ビーム砲（基本型）
    Missile,   // 誘導兵器（基本型）
    Shell,     // 炸裂弾（基本型）
};

// 武装に共通する形。コアが積んだ武装を順に Update し、撃つ時間になったら World に弾を出す
class Weapon {
public:
    virtual ~Weapon() = default;
    virtual WeaponKind Kind() const = 0;
    virtual void Update(World& world, float dt) = 0;
};

// 武装の名前（画面に出す）と、使うエネルギー
const char* WeaponName(WeaponKind kind);
int WeaponEnergy(WeaponKind kind);

// 武装を作る。撃つタイミングが重ならないように、最初の待ち時間を firstDelay にする
std::unique_ptr<Weapon> CreateWeapon(WeaponKind kind, float firstDelay);
