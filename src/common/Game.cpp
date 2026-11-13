#include "pch.h"
#include "Game.h"
#include <DxLib.h>
#include "Config.h"
#include "Platform.h"

void Game::Update(double dtSeconds) {
    // 入力は Platform::PollInput() で読む（マウスとタッチを同じ形で受け取れる）
    const InputState input = Platform::PollInput();
    (void)input;
    (void)dtSeconds;
}

void Game::Draw() {
}
