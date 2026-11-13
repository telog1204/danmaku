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
    const bool secondaryPressed = input.secondaryDown && !prevSecondaryDown_;
    prevPrimaryDown_ = input.primaryDown;
    prevSecondaryDown_ = input.secondaryDown;

    // 右クリック（2本指タップ）で、開始時の武装を切り替えて最初からやり直す（組み込み画面ができるまでの試し用）
    if (secondaryPressed) {
        world_.ResetWithNextStartWeapon();
        return;
    }

    // ゲームオーバーのあと少し待ってから、タップ（クリック）で最初からやり直す
    if (world_.IsGameOver() && world_.GameOverSeconds() >= Config::RestartDelay && pressed) {
        world_.Reset();
        return;
    }

    world_.Update(static_cast<float>(dtSeconds));
}

void Game::Draw() {
    DrawWorld(world_);
}
