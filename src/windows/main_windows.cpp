#include "pch.h"
#include <DxLib.h>
#include "Config.h"
#include "GameLoop.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // ソースはUTF-8で書いているので、DxLibにもUTF-8として扱わせる
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
    SetOutApplicationLogValidFlag(FALSE);
    ChangeWindowMode(TRUE);
    SetGraphMode(Config::ScreenWidth, Config::ScreenHeight, 32);
    SetMainWindowText(Config::WindowTitle);
    SetBackgroundColor(Config::BackgroundR, Config::BackgroundG, Config::BackgroundB);

    if (DxLib_Init() == -1) {
        return -1;
    }
    SetDrawScreen(DX_SCREEN_BACK);

    RunGame();

    DxLib_End();
    return 0;
}
