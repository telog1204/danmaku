#pragma once
#include "BuildScreen.h"
#include "GameOverScreen.h"
#include "Records.h"
#include "RewardScreen.h"
#include "TitleScreen.h"
#include "World.h"

// ゲーム本体。RunGame（GameLoop.cpp）が毎フレーム Update と Draw を呼ぶ。両OSで共通
class Game {
public:
    Game();

    // 入力を読み、ゲームの時間を dtSeconds（現実の秒）だけ進める
    void Update(double dtSeconds);

    // ゲームの画面（Config::ScreenWidth × Config::ScreenHeight）に描く
    void Draw();

private:
    enum class Mode {
        Title,     // タイトル画面
        Battle,    // 戦闘中
        Build,     // 組み込み画面を開いている（時間が止まる）
        Reward,    // 強敵を倒して、報酬を選んでいる（時間が止まる）
        GameOver,  // ゲームオーバーの結果を出している
    };

    World world_;
    TitleScreen titleScreen_;
    BuildScreen buildScreen_;
    RewardScreen rewardScreen_;
    GameOverScreen gameOverScreen_;
    Records records_;
    Mode mode_ = Mode::Title;
    bool prevPrimaryDown_ = false;  // 前のフレームで主な操作が続いていたか（押した瞬間を見分けるため）
};
