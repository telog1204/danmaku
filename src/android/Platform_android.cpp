#include "pch.h"
#include "Platform.h"
#include "Platform_android.h"
#include <DxLib.h>
#include "Config.h"

// Android：タッチで操作する。ゲームは Config::ScreenWidth × Config::ScreenHeight の画面に描き、それを端末の画面いっぱいに
// 縦横比を保って拡大して表示する（余った部分は背景色）。タッチの位置は逆の計算でゲームの画面の座標に戻す

namespace {
    int offscreen = -1;  // ゲームを描く画面（Config::ScreenWidth × Config::ScreenHeight）
    int viewX = 0;       // 端末の画面のうち、ゲームの画面を表示する範囲
    int viewY = 0;
    int viewW = Config::ScreenWidth;
    int viewH = Config::ScreenHeight;

    // 2本指で触れたあとは、指がすべて離れるまで1本指の操作（主な操作）を受け付けない。
    // 2本目の指より先に離した指が、タップとして扱われないようにするため
    bool waitAllReleased = false;
}

void AndroidPlatform::SetUpScreen() {
    int screenW = 0;
    int screenH = 0;
    GetDrawScreenSize(&screenW, &screenH);
    if (screenW <= 0 || screenH <= 0) {
        screenW = Config::ScreenWidth;
        screenH = Config::ScreenHeight;
    }
    // 横と縦のうち、先にいっぱいになる方に合わせる
    if (static_cast<long long>(screenW) * Config::ScreenHeight >= static_cast<long long>(screenH) * Config::ScreenWidth) {
        viewH = screenH;
        viewW = screenH * Config::ScreenWidth / Config::ScreenHeight;
    } else {
        viewW = screenW;
        viewH = screenW * Config::ScreenHeight / Config::ScreenWidth;
    }
    viewX = (screenW - viewW) / 2;
    viewY = (screenH - viewH) / 2;
    offscreen = MakeScreen(Config::ScreenWidth, Config::ScreenHeight, FALSE);
}

InputState Platform::PollInput() {
    InputState state;
    const int count = GetTouchInputNum();
    if (count <= 0) {
        // 指が触れていないときは、位置を画面の外にする（マウスを乗せたときのような表示が残らないように）
        waitAllReleased = false;
        return state;
    }
    int touchX = 0;
    int touchY = 0;
    GetTouchInput(0, &touchX, &touchY);
    state.x = (touchX - viewX) * Config::ScreenWidth / viewW;
    state.y = (touchY - viewY) * Config::ScreenHeight / viewH;
    if (count >= 2) {
        state.secondaryDown = true;
        waitAllReleased = true;
    } else if (!waitAllReleased) {
        state.primaryDown = true;
    }
    return state;
}

std::string Platform::SaveFilePath(const char* fileName) {
    // アプリ専用の内部ストレージに置く（アンインストールすると消える）
    TCHAR path[1024] = {};
    GetInternalDataPath(path, sizeof(path));
    if (path[0] == '\0') {
        return fileName;
    }
    return std::string(path) + "/" + fileName;
}

void Platform::BeginDraw() {
    if (offscreen != -1) {
        SetDrawScreen(offscreen);
    }
}

void Platform::EndDraw() {
    if (offscreen != -1) {
        SetDrawScreen(DX_SCREEN_BACK);
        ClearDrawScreen();
        SetDrawMode(DX_DRAWMODE_BILINEAR);
        DrawExtendGraph(viewX, viewY, viewX + viewW, viewY + viewH, offscreen, FALSE);
        SetDrawMode(DX_DRAWMODE_NEAREST);
    }
    ScreenFlip();
}
