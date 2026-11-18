#pragma once
#include "Vec2.h"

// 狙う相手の種類
enum class TargetKind {
    None,
    Enemy,
    EnemyBullet,
};

// 武装がロックオンするときの、狙う相手の優先の仕方
enum class TargetPriority {
    Any,             // 敵本体と敵の弾を区別しない（誘導兵器）
    LightBulletsThenMedium, // 軽の敵と敵の弾 → 中 の順に、いる区分から選ぶ（速射砲。重は狙わない）
    MediumFirst,     // 中・重 → 軽 → 敵の弾 の順に、いる区分から選ぶ（ビーム砲）
};

// 武装や誘導弾が狙う相手（kind と id で相手を見分ける。pos と vel はそのときの位置と速さ）
struct Target {
    TargetKind kind = TargetKind::None;
    int id = 0;
    Vec2 pos;
    Vec2 vel;
    bool light = false;  // 軽の敵か
    bool medium = false; // 中の敵か

    bool SameAs(const Target& o) const { return kind != TargetKind::None && kind == o.kind && id == o.id; }
};
