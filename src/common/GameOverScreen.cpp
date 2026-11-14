#include "pch.h"
#include "GameOverScreen.h"
#include <cstdio>
#include <DxLib.h>
#include "Config.h"
#include "TitleScreen.h"
#include "World.h"

namespace {
    constexpr int ButtonY = 370;
    constexpr int ButtonW = 180;
    constexpr int ButtonH = 50;
    constexpr int RetryX = 200;
    constexpr int TitleX = 420;

    bool Inside(int x, int y, int left, int top, int w, int h) {
        return x >= left && x < left + w && y >= top && y < top + h;
    }

    void DrawCentered(int y, const char* text, unsigned int color) {
        const int w = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
        DrawString((Config::ScreenWidth - w) / 2, y, text, color);
    }

    void DrawButton(int x, const char* label, bool enabled) {
        const unsigned int border = enabled ? GetColor(127, 208, 255) : GetColor(60, 80, 100);
        const unsigned int text = enabled ? GetColor(223, 244, 255) : GetColor(100, 110, 120);
        DrawBox(x, ButtonY, x + ButtonW, ButtonY + ButtonH, GetColor(18, 50, 74), TRUE);
        DrawBox(x, ButtonY, x + ButtonW, ButtonY + ButtonH, border, FALSE);
        const int w = GetDrawStringWidth(label, static_cast<int>(std::strlen(label)));
        DrawString(x + (ButtonW - w) / 2, ButtonY + (ButtonH - 16) / 2, label, text);
    }
}

void GameOverScreen::Open(bool newRecord) {
    newRecord_ = newRecord;
    pressing_ = false;
}

GameOverScreen::Choice GameOverScreen::Update(const World& world, const InputState& input) {
    const bool ready = world.GameOverSeconds() >= Config::RestartDelay;
    if (input.primaryDown && !pressing_) {
        pressing_ = true;
        pressX_ = input.x;
        pressY_ = input.y;
        return Choice::None;
    }
    if (!input.primaryDown && pressing_) {
        // 指を離したときに、押した場所のボタンを選ぶ（タッチでは離すと位置が分からなくなるので、押した位置を使う）
        pressing_ = false;
        if (!ready) {
            return Choice::None;  // ゲームオーバーの直後は、うっかり押すのを防ぐため押せない
        }
        if (Inside(pressX_, pressY_, RetryX, ButtonY, ButtonW, ButtonH)) return Choice::Retry;
        if (Inside(pressX_, pressY_, TitleX, ButtonY, ButtonW, ButtonH)) return Choice::Title;
    }
    return Choice::None;
}

void GameOverScreen::Draw(const World& world) const {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
    DrawBox(0, 0, Config::ScreenWidth, Config::ScreenHeight, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    const char* title = "GAME OVER";
    const int w = GetDrawStringWidthToHandle(title, static_cast<int>(std::strlen(title)), LargeFont());
    DrawStringToHandle((Config::ScreenWidth - w) / 2, 110, title, GetColor(255, 96, 96), LargeFont());

    char line[64];
    std::snprintf(line, sizeof(line), "生存時間　%.1f 秒", world.ElapsedSeconds());
    DrawCentered(185, line, GetColor(255, 255, 255));
    std::snprintf(line, sizeof(line), "倒した敵　%d 体", world.Kills());
    DrawCentered(215, line, GetColor(255, 255, 255));
    std::snprintf(line, sizeof(line), "倒した強敵　%d 体", world.BossKills());
    DrawCentered(245, line, GetColor(255, 255, 255));
    if (newRecord_) {
        DrawCentered(295, "★ 新記録！ ★", GetColor(255, 224, 96));
    }

    const bool ready = world.GameOverSeconds() >= Config::RestartDelay;
    DrawButton(RetryX, "もう一度", ready);
    DrawButton(TitleX, "タイトルへ", ready);
}
