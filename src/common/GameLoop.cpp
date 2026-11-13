#include "pch.h"
#include "GameLoop.h"
#include <DxLib.h>
#include "Config.h"
#include "Game.h"
#include "Platform.h"

void RunGame() {
    Game game;
    long long prevCount = GetNowHiPerformanceCount();
    while (ProcessMessage() == 0) {
        const long long count = GetNowHiPerformanceCount();
        double dtSeconds = (count - prevCount) / 1000000.0;
        prevCount = count;
        if (dtSeconds > Config::MaxFrameSeconds) {
            dtSeconds = Config::MaxFrameSeconds;
        }

        game.Update(dtSeconds);

        Platform::BeginDraw();
        ClearDrawScreen();
        game.Draw();
        Platform::EndDraw();
    }
}
