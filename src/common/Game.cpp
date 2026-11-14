#include "pch.h"
#include "Game.h"
#include <DxLib.h>
#include "Config.h"
#include "Platform.h"
#include "Renderer.h"

Game::Game() : records_(LoadRecords()) {
}

void Game::Update(double dtSeconds) {
    // 入力は Platform::PollInput() で読む（マウスとタッチを同じ形で受け取れる）
    const InputState input = Platform::PollInput();
    const bool pressed = input.primaryDown && !prevPrimaryDown_;
    prevPrimaryDown_ = input.primaryDown;
    const float dt = static_cast<float>(dtSeconds);

    switch (mode_) {
    case Mode::Title:
        // タップ（クリック）で、最初から始める
        titleScreen_.Update(dt);
        if (pressed) {
            world_.Reset();
            mode_ = Mode::Battle;
        }
        return;

    case Mode::Build:
        // 組み込み画面を開いている間は、ゲームの時間を進めない
        if (buildScreen_.Update(world_, input, dt)) {
            mode_ = Mode::Battle;
        }
        return;

    case Mode::Reward:
        // 報酬を選んでいる間も、ゲームの時間を進めない
        if (rewardScreen_.Update(world_, input)) {
            mode_ = Mode::Battle;
        }
        return;

    case Mode::GameOver:
        world_.Update(dt);  // ゲームオーバーになってからの時間を数える（ボタンを押せるまでの待ち時間）
        switch (gameOverScreen_.Update(world_, input)) {
        case GameOverScreen::Choice::Retry:
            world_.Reset();
            mode_ = Mode::Battle;
            break;
        case GameOverScreen::Choice::Title:
            mode_ = Mode::Title;
            break;
        case GameOverScreen::Choice::None:
            break;
        }
        return;

    case Mode::Battle:
        break;
    }

    // 戦闘中：「組み込み」ボタンで組み込み画面を開き、時間を止める
    if (pressed && BuildScreen::HitOpenButton(input.x, input.y)) {
        buildScreen_.Open();
        mode_ = Mode::Build;
        return;
    }

    world_.Update(dt);

    if (world_.IsGameOver()) {
        // 最高記録を更新して保存し、結果を出す
        const bool newRecord = UpdateRecords(records_, world_.ElapsedSeconds(), world_.Kills());
        SaveRecords(records_);
        gameOverScreen_.Open(newRecord);
        mode_ = Mode::GameOver;
    } else if (world_.HasPendingReward()) {
        // 強敵を倒したら、報酬を選ぶ画面を開く
        rewardScreen_.Open();
        mode_ = Mode::Reward;
    }
}

void Game::Draw() {
    switch (mode_) {
    case Mode::Title:
        titleScreen_.Draw(records_);
        return;
    case Mode::Build:
        buildScreen_.Draw(world_);
        return;
    case Mode::Reward:
        rewardScreen_.Draw(world_);
        return;
    case Mode::GameOver:
        DrawWorld(world_);
        gameOverScreen_.Draw(world_);
        return;
    case Mode::Battle:
        DrawWorld(world_);
        BuildScreen::DrawOpenButton();
        return;
    }
}
