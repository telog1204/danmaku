#include "pch.h"
#include <DxLib.h>
#include "Config.h"
#include "GameLoop.h"
#include "Platform_android.h"

namespace {
    // 端末の画面の大きさ（横向き）。取れなかったときはゲームの画面の大きさにする
    void GetLandscapeDisplaySize(int& width, int& height) {
        width = 0;
        height = 0;
        GetAndroidDisplayResolution(&width, &height);
        if (width < height) {
            const int t = width;
            width = height;
            height = t;
        }
        if (width <= 0 || height <= 0) {
            width = Config::ScreenWidth;
            height = Config::ScreenHeight;
        }
    }
}

// Android 版の始まり（DxLib が NativeActivity から呼ぶ）
int android_main(void) {
    // ソースはUTF-8で書いているので、DxLibにもUTF-8として扱わせる
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
    SetOutApplicationLogValidFlag(FALSE);

    // 画面は端末の解像度にし、ゲームの画面（Config::ScreenWidth × Config::ScreenHeight）を拡大して表示する（Platform_android.cpp）
    int width = 0;
    int height = 0;
    GetLandscapeDisplaySize(width, height);
    SetGraphMode(width, height, 32);
    SetBackgroundColor(Config::BackgroundR, Config::BackgroundG, Config::BackgroundB);

    if (DxLib_Init() == -1) {
        return -1;
    }

    // 初期化の前に解像度が取れなかった場合に備えて、初期化のあとにもう一度確かめる
    int screenW = 0;
    int screenH = 0;
    GetDrawScreenSize(&screenW, &screenH);
    GetLandscapeDisplaySize(width, height);
    if (width != screenW || height != screenH) {
        SetGraphMode(width, height, 32);
    }
    SetDrawScreen(DX_SCREEN_BACK);
    AndroidPlatform::SetUpScreen();

    RunGame();

    DxLib_End();
    return 0;
}
