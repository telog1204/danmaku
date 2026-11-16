#pragma once
#include "Vec2.h"

// 狙う相手の種類
enum class TargetKind {
    None,
    Enemy,
    EnemyBullet,
};

// 武装や誘導弾が狙う相手（kind と id で相手を見分ける。pos と vel はそのときの位置と速さ）
struct Target {
    TargetKind kind = TargetKind::None;
    int id = 0;
    Vec2 pos;
    Vec2 vel;
    bool light = false;  // 軽の敵か（速射砲は、軽の敵と敵の弾だけを狙う）

    bool SameAs(const Target& o) const { return kind != TargetKind::None && kind == o.kind && id == o.id; }
};
