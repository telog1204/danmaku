#include "pch.h"
#include "BuildScreen.h"
#include <cstdio>
#include <DxLib.h>
#include "Config.h"
#include "World.h"

namespace {
    // 画面の配置（ゲームの画面 800×600 の上の位置）
    constexpr int ButtonX = 680;   // 右上のボタン（戦闘画面の「組み込み」、組み込み画面の「戻る」）
    constexpr int ButtonY = 10;
    constexpr int ButtonW = 110;
    constexpr int ButtonH = 40;
    constexpr int LeftX = 20;      // 左の列「持っている武装」
    constexpr int RightX = 410;    // 右の列「積んでいる武装」
    constexpr int RowW = 370;
    constexpr int RowH = 40;
    constexpr int RowTop = 102;
    constexpr int RowStep = 46;    // 行の高さ＋すき間
    constexpr float DeniedSeconds = 0.4f;

    bool Inside(int x, int y, int left, int top, int w, int h) {
        return x >= left && x < left + w && y >= top && y < top + h;
    }

    // 何行目を押したか（行がなければ −1）
    int HitRow(int x, int y, int columnX, size_t rowCount) {
        for (size_t i = 0; i < rowCount; ++i) {
            const int top = RowTop + static_cast<int>(i) * RowStep;
            if (top + RowH > Config::ScreenHeight) break;
            if (Inside(x, y, columnX, top, RowW, RowH)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void DrawButton(const char* label) {
        DrawBox(ButtonX, ButtonY, ButtonX + ButtonW, ButtonY + ButtonH, GetColor(18, 50, 74), TRUE);
        DrawBox(ButtonX, ButtonY, ButtonX + ButtonW, ButtonY + ButtonH, GetColor(127, 208, 255), FALSE);
        const int w = GetDrawStringWidth(label, static_cast<int>(std::strlen(label)));
        DrawString(ButtonX + (ButtonW - w) / 2, ButtonY + (ButtonH - 16) / 2, label, GetColor(223, 244, 255));
    }

    // 1行（武装の名前と、右端に使うエネルギー）
    void DrawRow(int left, int top, unsigned int fill, unsigned int border, unsigned int textColor,
                 unsigned int energyColor, WeaponKind kind) {
        DrawBox(left, top, left + RowW, top + RowH, fill, TRUE);
        DrawBox(left, top, left + RowW, top + RowH, border, FALSE);
        DrawString(left + 12, top + (RowH - 16) / 2, WeaponName(kind), textColor);
        char energy[16];
        std::snprintf(energy, sizeof(energy), "E %d", WeaponEnergy(kind));
        const int w = GetDrawStringWidth(energy, static_cast<int>(std::strlen(energy)));
        DrawString(left + RowW - 12 - w, top + (RowH - 16) / 2, energy, energyColor);
    }
}

void BuildScreen::Open() {
    deniedRow_ = -1;
    deniedTimer_ = 0.0f;
}

bool BuildScreen::Update(World& world, bool pressed, int x, int y, float dt) {
    if (deniedTimer_ > 0.0f) {
        deniedTimer_ -= dt;
        if (deniedTimer_ <= 0.0f) {
            deniedRow_ = -1;
        }
    }
    if (!pressed) {
        return false;
    }
    if (Inside(x, y, ButtonX, ButtonY, ButtonW, ButtonH)) {
        return true;  // 「戻る」
    }

    // 左の列：持っている武装を1つ積む。エネルギーが足りなければ、その行を赤く光らせる
    const auto& owned = world.OwnedWeapons();
    const int left = HitRow(x, y, LeftX, owned.size());
    if (left >= 0) {
        const WeaponKind kind = owned[static_cast<size_t>(left)];
        if (world.CanEquip(kind)) {
            world.Equip(kind);
            deniedRow_ = -1;
            deniedTimer_ = 0.0f;
        } else {
            deniedRow_ = left;
            deniedTimer_ = DeniedSeconds;
        }
        return false;
    }

    // 右の列：積んでいる武装を外す
    const int right = HitRow(x, y, RightX, world.Weapons().size());
    if (right >= 0) {
        world.Unequip(static_cast<size_t>(right));
    }
    return false;
}

void BuildScreen::Draw(const World& world) const {
    DrawBox(0, 0, Config::ScreenWidth, Config::ScreenHeight, GetColor(10, 14, 24), TRUE);

    // 上の帯：題名・エネルギー・戻る
    DrawString(LeftX, 20, "組み込み", GetColor(255, 255, 255));
    const int used = world.UsedEnergy();
    const int cap = world.EnergyCap();
    DrawFormatString(170, 14, GetColor(255, 215, 106), "エネルギー　使用 %d / 上限 %d", used, cap);
    const int gaugeX = 170;
    const int gaugeY = 36;
    const int gaugeW = 360;
    DrawBox(gaugeX, gaugeY, gaugeX + gaugeW, gaugeY + 12, GetColor(58, 52, 32), TRUE);
    if (cap > 0) {
        DrawBox(gaugeX, gaugeY, gaugeX + gaugeW * std::min(used, cap) / cap, gaugeY + 12, GetColor(255, 215, 106), TRUE);
    }
    DrawButton("戻る");
    DrawLine(0, 62, Config::ScreenWidth, 62, GetColor(44, 56, 80));
    DrawLine(399, 76, 399, Config::ScreenHeight - 24, GetColor(44, 56, 80));

    // 左の列：持っている武装
    DrawString(LeftX, 78, "持っている武装（タップで積む）", GetColor(154, 170, 187));
    const auto& owned = world.OwnedWeapons();
    for (size_t i = 0; i < owned.size(); ++i) {
        const int top = RowTop + static_cast<int>(i) * RowStep;
        if (top + RowH > Config::ScreenHeight) break;
        if (static_cast<int>(i) == deniedRow_) {
            DrawRow(LeftX, top, GetColor(90, 20, 20), GetColor(255, 80, 80), GetColor(255, 255, 255),
                    GetColor(255, 255, 255), owned[i]);
            DrawString(LeftX + 12, top + RowH + 4, "エネルギーが足りません", GetColor(255, 128, 128));
        } else {
            DrawRow(LeftX, top, GetColor(27, 43, 61), GetColor(59, 95, 128), GetColor(228, 241, 255),
                    GetColor(255, 215, 106), owned[i]);
        }
    }

    // 右の列：積んでいる武装（1つずつ1行）
    DrawString(RightX, 78, "積んでいる武装（タップで外す）", GetColor(154, 170, 187));
    const auto& weapons = world.Weapons();
    for (size_t i = 0; i < weapons.size(); ++i) {
        const int top = RowTop + static_cast<int>(i) * RowStep;
        if (top + RowH > Config::ScreenHeight) break;
        DrawRow(RightX, top, GetColor(34, 51, 31), GetColor(79, 125, 69), GetColor(234, 255, 228),
                GetColor(255, 215, 106), weapons[i]->Kind());
    }
}

bool BuildScreen::HitOpenButton(int x, int y) {
    return Inside(x, y, ButtonX, ButtonY, ButtonW, ButtonH);
}

void BuildScreen::DrawOpenButton() {
    DrawButton("組み込み");
}
