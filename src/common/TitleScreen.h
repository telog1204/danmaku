#pragma once
#include <random>
#include <vector>
#include "Records.h"
#include "Vec2.h"

// タイトル画面。タップ（クリック）で始める。背景では光る弾がゆっくり流れる
class TitleScreen {
public:
    TitleScreen();

    // 背景の弾を動かす
    void Update(float dt);

    void Draw(const Records& records) const;

private:
    struct Particle {
        Vec2 pos;
        Vec2 vel;
        float radius = 0.0f;
        int r = 0;
        int g = 0;
        int b = 0;
    };
    std::vector<Particle> particles_;
};

// 大きな文字を描くためのフォント（初めて使うときに作る）
int TitleFont();   // タイトル用（とても大きい）
int LargeFont();   // 見出し用（大きい）
