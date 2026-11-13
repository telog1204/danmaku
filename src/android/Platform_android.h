#pragma once

// Android だけで使う初期化。DxLib_Init のあと、RunGame の前に呼ぶ
namespace AndroidPlatform {
    // 端末の画面に、ゲームの画面（Config::ScreenWidth × Config::ScreenHeight）を縦横比を保ったまま最大に表示する準備をする
    void SetUpScreen();
}
