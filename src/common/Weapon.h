#pragma once
#include <memory>
#include <vector>

class World;

// 武装の種類。組み込み画面で積み外しするときも、この種類で見分ける
enum class WeaponKind {
    RapidGun,  // 速射砲（基本型）
    Beam,      // ビーム砲（基本型）
    Missile,   // 誘導兵器（基本型）
    Shell,     // 炸裂弾（基本型）
};
constexpr int WeaponKindCount = 4;

// 武装の系統。強化パーツは、元になった系統の武装にだけ付けられる
enum class WeaponFamily {
    RapidGun,  // 速射砲
    Beam,      // ビーム砲
    Missile,   // 誘導兵器
    Shell,     // 炸裂弾
};
constexpr int WeaponFamilyCount = 4;

// 強化パーツの種類（どの系統で使えるかは FamilyParts で決まる）
enum class PartKind {
    Power,          // 威力強化
    Rate,           // 連射強化
    Focus,          // 収束（速射砲）
    Pierce,         // 貫徹力強化（速射砲・ビーム砲）
    Homing,         // 誘導力強化（誘導兵器）
    BlastRadius,    // 爆発範囲拡大（誘導兵器）
    FlameRadius,    // 爆炎範囲拡大（炸裂弾）
    FlameDuration,  // 爆炎持続延長（炸裂弾）
    Fragments,      // 破片増加（炸裂弾）
};
constexpr int PartKindCount = 9;

WeaponFamily FamilyOf(WeaponKind kind);
const std::vector<PartKind>& FamilyParts(WeaponFamily family);
const char* FamilyName(WeaponFamily family);
const char* PartName(PartKind part);
const char* PartDescription(WeaponFamily family, PartKind part);

// 武装の名前（画面に出す）と、本体が使うエネルギー
const char* WeaponName(WeaponKind kind);
int WeaponEnergy(WeaponKind kind);

// 武装に共通する形。コアが積んだ武装を順に Update し、撃つ時間になったら World に弾を出す
class Weapon {
public:
    virtual ~Weapon() = default;
    virtual WeaponKind Kind() const = 0;
    virtual void Update(World& world, float dt) = 0;

    // 付けている強化パーツ（付けた順）
    const std::vector<PartKind>& Parts() const { return parts_; }
    void AddPart(PartKind part) { parts_.push_back(part); }
    // 指定した種類の強化パーツを1つ外す（最後に付けたものから）。外せたら true
    bool RemovePart(PartKind part);
    int CountPart(PartKind part) const;

    // 本体と強化パーツを合わせた消費エネルギー（強化パーツは1個目 +1、2個目 +2、3個目 +3…）
    int Energy() const;
    // もう1つ強化パーツを付けたときに増えるエネルギー
    int NextPartEnergy() const { return static_cast<int>(parts_.size()) + 1; }

private:
    std::vector<PartKind> parts_;
};

// 武装を作る。撃つタイミングが重ならないように、最初の待ち時間を firstDelay にする
std::unique_ptr<Weapon> CreateWeapon(WeaponKind kind, float firstDelay);
