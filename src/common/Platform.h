#pragma once
#include <string>

// OSごとに違う部分（入力・ファイルの場所・画面への表示）を、ゲーム本体から同じ形で使うための窓口。
// 実装は src/windows/Platform_windows.cpp と src/android/Platform_android.cpp にあり、
// ビルドする側（Visual Studio のプロジェクト、Android の CMakeLists.txt）がどちらか一方だけを含める。

// 1フレーム分の入力。座標はゲームの画面（Config::ScreenWidth × Config::ScreenHeight）上の位置
struct InputState {
    int x = -1;                  // 指やマウスの位置。指が触れていないとき（タッチ）は画面の外になる
    int y = -1;
    bool primaryDown = false;    // 主な操作が続いている（Windows：左ボタン、Android：指1本）
    bool secondaryDown = false;  // 2つ目の操作が続いている（Windows：右ボタン、Android：指2本）
};

namespace Platform {
    // 今のフレームの入力を読む（1フレームに1回呼ぶ）
    InputState PollInput();

    // セーブデータを書き込むファイルのパス（Windows：作業フォルダ、Android：アプリ専用の内部ストレージ）
    std::string SaveFilePath(const char* fileName);

    // 1フレームの描画の前後に呼ぶ。描き終わったら画面に表示する
    void BeginDraw();
    void EndDraw();
}
