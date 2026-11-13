#pragma once
#include "World.h"

// ゲーム本体。RunGame（GameLoop.cpp）が毎フレーム Update と Draw を呼ぶ。両OSで共通
class Game {
public:
    // 入力を読み、ゲームの時間を dtSeconds（現実の秒）だけ進める
    void Update(double dtSeconds);

    // ゲームの画面（Config::ScreenWidth × Config::ScreenHeight）に描く
    void Draw();

private:
    World world_;
    bool prevPrimaryDown_ = false;    // 前のフレームで主な操作が続いていたか（押した瞬間を見分けるため）
    bool prevSecondaryDown_ = false;  // 前のフレームで2つ目の操作が続いていたか
};
