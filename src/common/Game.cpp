#include "pch.h"
#include "Game.h"
#include <DxLib.h>
#include "Config.h"
#include "Platform.h"
#include "Renderer.h"

void Game::Update(double dtSeconds) {
    // 入力は Platform::PollInput() で読む（マウスとタッチを同じ形で受け取れる）
    const InputState input = Platform::PollInput();
    const bool pressed = input.primaryDown && !prevPrimaryDown_;
    prevPrimaryDown_ = input.primaryDown;
    const float dt = static_cast<float>(dtSeconds);

    // 組み込み画面を開いている間は、ゲームの時間を進めない
    if (mode_ == Mode::Build) {
        if (buildScreen_.Update(world_, input, dt)) {
            mode_ = Mode::Battle;
        }
        return;
    }

    if (world_.IsGameOver()) {
        // ゲームオーバーのあと少し待ってから、タップ（クリック）で最初からやり直す
        if (world_.GameOverSeconds() >= Config::RestartDelay && pressed) {
            world_.Reset();
            return;
        }
    } else if (pressed && BuildScreen::HitOpenButton(input.x, input.y)) {
        // 「組み込み」ボタンで組み込み画面を開き、時間を止める
        buildScreen_.Open();
        mode_ = Mode::Build;
        return;
    }

    world_.Update(dt);
}

void Game::Draw() {
    if (mode_ == Mode::Build) {
        buildScreen_.Draw(world_);
        return;
    }
    DrawWorld(world_);
    if (!world_.IsGameOver()) {
        BuildScreen::DrawOpenButton();
    }
}
