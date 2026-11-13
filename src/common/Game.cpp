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
