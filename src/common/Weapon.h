#pragma once
#include <memory>
#include <string>
#include <vector>

class World;

// 武装の系統。強化パーツは、元になった系統の武装にだけ付けられる
enum class WeaponFamily {
    RapidGun,  // 速射砲
    Beam,      // ビーム砲
    Missile,   // 誘導兵器
    Shell,     // 炸裂弾
};
constexpr int WeaponFamilyCount = 4;

// 武装の派生版（系統ごとの撃ち方の違い）
enum class WeaponVariant {
    RapidBasic,      // 速射砲 基本型
    RapidBarrage,    // 速射砲 弾幕型
    RapidAttack,     // 速射砲 攻撃型
    RapidClose,      // 速射砲 近接防御型
    BeamBasic,       // ビーム砲 基本型
    BeamAttack,      // ビーム砲 攻撃型
    BeamDecisive,    // ビーム砲 決戦型
    MissileBasic,    // 誘導兵器 基本型
    MissileAttack,   // 誘導兵器 攻撃型
    MissileClose,    // 誘導兵器 近接防御型
    ShellBasic,      // 炸裂弾 基本型
    ShellDecisive,   // 炸裂弾 決戦型
};
constexpr int WeaponVariantCount = 12;

// 武装の種類：派生版と、連装の門数（1なら単装）の組。組み込み画面や落とし物でも、この組で見分ける
struct WeaponKind {
    WeaponVariant variant = WeaponVariant::RapidBasic;
    int barrels = 1;

    bool operator==(const WeaponKind& o) const { return variant == o.variant && barrels == o.barrels; }
    bool operator!=(const WeaponKind& o) const { return !(*this == o); }
};

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

WeaponFamily FamilyOf(WeaponVariant variant);
WeaponFamily FamilyOf(const WeaponKind& kind);
const std::vector<PartKind>& FamilyParts(WeaponFamily family);
const char* FamilyName(WeaponFamily family);
const char* PartName(PartKind part);
const char* PartDescription(WeaponFamily family, PartKind part);

// 連装の段階。系統ごとの門数の並び（速射砲 1〜4、ビーム砲と炸裂弾 1〜3、誘導兵器 1・2・4・8）
const std::vector<int>& BarrelSteps(WeaponFamily family);
// 1つ上の連装段階（もうなければ barrels が 0 の組を返す）
WeaponKind NextBarrelStep(const WeaponKind& kind);

// 武装の名前（「速射砲 弾幕型（2連装）」など）・説明・使うエネルギー（強化パーツを除く）
std::string WeaponName(const WeaponKind& kind);
std::string WeaponDescription(const WeaponKind& kind);
int WeaponEnergy(const WeaponKind& kind);

// 武装に共通する形。コアが積んだ武装を順に Update し、撃つ時間になったら World に弾を出す
class Weapon {
public:
    explicit Weapon(const WeaponKind& kind) : kind_(kind) {}
    virtual ~Weapon() = default;
    const WeaponKind& Kind() const { return kind_; }
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
    WeaponKind kind_;
    std::vector<PartKind> parts_;
};

// 武装を作る。撃つタイミングが重ならないように、最初の待ち時間を firstDelay にする
std::unique_ptr<Weapon> CreateWeapon(const WeaponKind& kind, float firstDelay);
