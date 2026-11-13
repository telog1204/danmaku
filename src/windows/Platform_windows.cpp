#include "pch.h"
#include "Platform.h"
#include <DxLib.h>

// Windows：マウスで操作し、ウィンドウ（Config::ScreenWidth × Config::ScreenHeight）にそのまま描く

InputState Platform::PollInput() {
    InputState state;
    GetMousePoint(&state.x, &state.y);
    const int input = GetMouseInput();
    state.primaryDown = (input & MOUSE_INPUT_LEFT) != 0;
    state.secondaryDown = (input & MOUSE_INPUT_RIGHT) != 0;
    return state;
}

std::string Platform::SaveFilePath(const char* fileName) {
    // 実行ファイルを起動したフォルダ（作業フォルダ）に置く
    return fileName;
}

void Platform::BeginDraw() {
}

void Platform::EndDraw() {
    ScreenFlip();
}
