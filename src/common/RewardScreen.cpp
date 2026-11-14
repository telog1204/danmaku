#include "pch.h"
#include "RewardScreen.h"
#include <DxLib.h>
#include "Config.h"
#include "World.h"

namespace {
    // カードの配置（ゲームの画面 800×600 の上の位置）
    constexpr int CardTop = 150;
    constexpr int CardW = 230;
    constexpr int CardH = 300;
    constexpr int CardX[3] = { 25, 285, 545 };
    constexpr int Padding = 16;

    bool Inside(int x, int y, int left, int top, int w, int h) {
        return x >= left && x < left + w && y >= top && y < top + h;
    }

    int TextWidth(const std::string& text) {
        return GetDrawStringWidth(text.c_str(), static_cast<int>(text.size()));
    }

    void DrawCentered(int y, const char* text, unsigned int color) {
        DrawString((Config::ScreenWidth - TextWidth(text)) / 2, y, text, color);
    }

    // UTF-8 の1文字の長さ（バイト数）
    size_t CharBytes(unsigned char c) {
        if (c < 0x80) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        return 4;
    }

    // 幅 width に収まるように折り返して描く。描いたあとの次の行の y を返す
    int DrawWrapped(int x, int y, int width, const std::string& text, unsigned int color) {
        constexpr int LineH = 22;
        std::string line;
        for (size_t i = 0; i < text.size();) {
            const size_t n = CharBytes(static_cast<unsigned char>(text[i]));
            const std::string ch = text.substr(i, n);
            if (!line.empty() && TextWidth(line + ch) > width) {
                DrawString(x, y, line.c_str(), color);
                y += LineH;
                line.clear();
            }
            line += ch;
            i += n;
        }
        if (!line.empty()) {
            DrawString(x, y, line.c_str(), color);
            y += LineH;
        }
        return y;
    }
}

void RewardScreen::Open() {
    pressing_ = false;
    ignoreUntilRelease_ = true;
}

bool RewardScreen::Update(World& world, const InputState& input) {
    if (ignoreUntilRelease_) {
        // 強敵を倒した瞬間に押していた指で、うっかり選ばないようにする
        if (!input.primaryDown) {
            ignoreUntilRelease_ = false;
        }
        return false;
    }
    if (input.primaryDown && !pressing_) {
        pressing_ = true;
        pressX_ = input.x;
        pressY_ = input.y;
        return false;
    }
    if (!input.primaryDown && pressing_) {
        // 指を離したときに、押した場所のカードを選ぶ（タッチでは離すと位置が分からなくなるので、押した位置を使う）
        pressing_ = false;
        const size_t count = world.RewardChoices().size();
        for (size_t i = 0; i < count && i < 3; ++i) {
            if (Inside(pressX_, pressY_, CardX[i], CardTop, CardW, CardH)) {
                world.ChooseReward(i);
                return true;
            }
        }
    }
    return false;
}

void RewardScreen::Draw(const World& world) const {
    DrawBox(0, 0, Config::ScreenWidth, Config::ScreenHeight, GetColor(6, 8, 14), TRUE);
    DrawCentered(54, "強敵撃破！", GetColor(255, 233, 160));
    DrawCentered(96, "報酬を1つ選んでください", GetColor(154, 170, 187));

    const auto& choices = world.RewardChoices();
    for (size_t i = 0; i < choices.size() && i < 3; ++i) {
        const Pickup& p = choices[i];
        const int x = CardX[i];
        unsigned int fill = 0;
        unsigned int border = 0;
        std::string kindLabel;
        std::string name;
        std::string desc;
        switch (p.kind) {
        case PickupKind::Weapon:
            fill = GetColor(27, 43, 61);
            border = GetColor(127, 208, 255);
            kindLabel = "武装";
            name = WeaponName(p.weapon);
            desc = std::string(WeaponDescription(p.weapon)) + "（エネルギー " + std::to_string(WeaponEnergy(p.weapon)) + "）";
            break;
        case PickupKind::Part:
            fill = GetColor(46, 36, 64);
            border = GetColor(199, 155, 255);
            kindLabel = std::string("強化パーツ（") + FamilyName(p.family) + "）";
            name = PartName(p.part);
            desc = PartDescription(p.family, p.part);
            break;
        case PickupKind::Energy:
            fill = GetColor(58, 52, 32);
            border = GetColor(255, 215, 106);
            kindLabel = "エネルギー上限解放パーツ";
            name = "エネルギー上限 +1";
            desc = "エネルギーの上限が1増える。今の上限 " + std::to_string(world.EnergyCap()) + " → " +
                   std::to_string(world.EnergyCap() + 1);
            break;
        }
        DrawBox(x, CardTop, x + CardW, CardTop + CardH, fill, TRUE);
        DrawBox(x, CardTop, x + CardW, CardTop + CardH, border, FALSE);
        DrawBox(x + 1, CardTop + 1, x + CardW - 1, CardTop + CardH - 1, border, FALSE);
        int y = CardTop + Padding;
        y = DrawWrapped(x + Padding, y, CardW - Padding * 2, kindLabel, border);
        y = DrawWrapped(x + Padding, y + 6, CardW - Padding * 2, name, GetColor(255, 255, 255));
        DrawWrapped(x + Padding, y + 12, CardW - Padding * 2, desc, GetColor(201, 212, 226));
    }
    DrawCentered(480, "タップした報酬が手に入り、戦闘を続きから再開します", GetColor(120, 136, 136));
}
