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
    constexpr int ColumnX[2] = { LeftX, RightX };
    constexpr int RowW = 370;
    constexpr int RowH = 40;
    constexpr int RowStep = 46;    // 行の高さ＋すき間
    constexpr int ListTop = 100;   // 一覧を表示する範囲（この範囲の中でスクロールする）
    constexpr int ListBottom = 590;
    constexpr int LineH = 20;      // 文字の行の高さ
    constexpr int DragThreshold = 8;  // 指をこれより大きく動かしたらスクロールとして扱う
    constexpr float DeniedSeconds = 0.4f;

    // 武装の詳しい画面の配置
    constexpr int PanelX = 100;
    constexpr int PanelY = 50;
    constexpr int PanelW = 600;
    constexpr int PanelH = 520;
    constexpr int CloseX = 576;
    constexpr int CloseY = 66;
    constexpr int CloseW = 104;
    constexpr int CloseH = 40;
    constexpr int PartLeftX = 124;   // 付けている強化パーツ
    constexpr int PartRightX = 416;  // 付けられる強化パーツ
    constexpr int PartW = 260;
    constexpr int PartTop = 160;
    constexpr int DescTop = 404;     // 強化パーツの説明
    constexpr int RemoveX = 124;     // この武装を外す
    constexpr int RemoveY = 522;
    constexpr int RemoveW = 180;
    constexpr int RemoveH = 40;

    bool Inside(int x, int y, int left, int top, int w, int h) {
        return x >= left && x < left + w && y >= top && y < top + h;
    }

    int TextWidth(const char* text) {
        return GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
    }

    void DrawButton(int x, int y, int w, int h, const char* label, unsigned int fill, unsigned int border, unsigned int text) {
        DrawBox(x, y, x + w, y + h, fill, TRUE);
        DrawBox(x, y, x + w, y + h, border, FALSE);
        DrawString(x + (w - TextWidth(label)) / 2, y + (h - 16) / 2, label, text);
    }

    void DrawCornerButton(const char* label) {
        DrawButton(ButtonX, ButtonY, ButtonW, ButtonH, label, GetColor(18, 50, 74), GetColor(127, 208, 255), GetColor(223, 244, 255));
    }

    // 1行のボタン（左に名前、右端に数字など）
    void DrawRow(int left, int top, int width, unsigned int fill, unsigned int border,
                 const std::string& label, unsigned int labelColor, const std::string& right, unsigned int rightColor) {
        DrawBox(left, top, left + width, top + RowH, fill, TRUE);
        DrawBox(left, top, left + width, top + RowH, border, FALSE);
        DrawString(left + 12, top + (RowH - 16) / 2, label.c_str(), labelColor);
        if (!right.empty()) {
            DrawString(left + width - 12 - TextWidth(right.c_str()), top + (RowH - 16) / 2, right.c_str(), rightColor);
        }
    }

    std::string EnergyText(int energy) {
        return "E " + std::to_string(energy);
    }

    // 持っている強化パーツの一覧（系統ごとに1行。なければ空）
    std::vector<std::string> SparePartLines(const World& world) {
        std::vector<std::string> lines;
        for (int f = 0; f < WeaponFamilyCount; ++f) {
            const WeaponFamily family = static_cast<WeaponFamily>(f);
            std::string line;
            for (PartKind part : FamilyParts(family)) {
                const int n = world.SpareParts(family, part);
                if (n > 0) {
                    if (!line.empty()) line += "、";
                    line += std::string(PartName(part)) + " " + std::to_string(n);
                }
            }
            if (!line.empty()) {
                lines.push_back(std::string(FamilyName(family)) + "：" + line);
            }
        }
        return lines;
    }

    // 列の中身の高さ（スクロールできる量を決めるため）
    float ContentHeight(int column, const World& world) {
        if (column == 0) {
            const size_t lines = std::max<size_t>(1, SparePartLines(world).size());
            return static_cast<float>(world.OwnedWeapons().size() * RowStep + 10 + LineH * (1 + lines));
        }
        return static_cast<float>(world.Weapons().size() * RowStep);
    }

    // 一覧の何行目を押したか（行がなければ −1）
    int HitListRow(int x, int y, int column, float scroll, size_t rowCount) {
        if (y < ListTop || y >= ListBottom || x < ColumnX[column] || x >= ColumnX[column] + RowW) {
            return -1;
        }
        const float local = y - ListTop + scroll;
        const int row = static_cast<int>(local / RowStep);
        if (row < 0 || static_cast<size_t>(row) >= rowCount || local - row * RowStep >= RowH) {
            return -1;
        }
        return row;
    }

    // 付けている強化パーツ（種類ごとにまとめ、系統の並び順で）
    std::vector<PartKind> AttachedPartKinds(const Weapon& w) {
        std::vector<PartKind> kinds;
        for (PartKind part : FamilyParts(FamilyOf(w.Kind()))) {
            if (w.CountPart(part) > 0) {
                kinds.push_back(part);
            }
        }
        return kinds;
    }
}

void BuildScreen::Open() {
    // 開いたときの指（「組み込み」ボタンを押した指）は、離すまで操作として扱わない
    pressing_ = true;
    dragging_ = true;
    dragColumn_ = -1;
    detailIndex_ = -1;
    deniedArea_ = -1;
    deniedRow_ = -1;
    deniedTimer_ = 0.0f;
}

float BuildScreen::MaxScroll(int column, const World& world) const {
    return std::max(0.0f, ContentHeight(column, world) - (ListBottom - ListTop));
}

void BuildScreen::Deny(int area, int row) {
    deniedArea_ = area;
    deniedRow_ = row;
    deniedTimer_ = DeniedSeconds;
}

bool BuildScreen::Update(World& world, const InputState& input, float dt) {
    if (deniedTimer_ > 0.0f) {
        deniedTimer_ -= dt;
        if (deniedTimer_ <= 0.0f) {
            deniedArea_ = -1;
            deniedRow_ = -1;
        }
    }

    if (input.primaryDown && !pressing_) {
        // 押した瞬間
        pressing_ = true;
        dragging_ = false;
        pressX_ = input.x;
        pressY_ = input.y;
        lastY_ = input.y;
        dragColumn_ = -1;
        if (detailIndex_ < 0 && input.y >= ListTop && input.y < ListBottom) {
            dragColumn_ = (input.x < 400) ? 0 : 1;
        }
        return false;
    }
    if (input.primaryDown && pressing_) {
        // 押したまま動かしている：一定以上動いたら列をスクロールする
        if (!dragging_ && dragColumn_ >= 0 && std::abs(input.y - pressY_) > DragThreshold) {
            dragging_ = true;
        }
        if (dragging_) {
            float& s = scroll_[dragColumn_];
            s = std::max(0.0f, std::min(MaxScroll(dragColumn_, world), s - static_cast<float>(input.y - lastY_)));
        }
        lastY_ = input.y;
        return false;
    }
    if (!input.primaryDown && pressing_) {
        // 指を離した瞬間：スクロールしていなければ、押した場所の操作をする。
        // タッチでは指を離すと位置が分からなくなるので、押した位置を使う
        pressing_ = false;
        if (!dragging_) {
            return HandleTap(world, pressX_, pressY_);
        }
    }
    return false;
}

bool BuildScreen::HandleTap(World& world, int x, int y) {
    if (detailIndex_ >= 0) {
        HandleDetailTap(world, x, y);
        return false;
    }
    if (Inside(x, y, ButtonX, ButtonY, ButtonW, ButtonH)) {
        return true;  // 「戻る」
    }

    // 左の列：持っている武装を1つ積む。エネルギーが足りなければ、その行を赤く光らせる
    const auto& owned = world.OwnedWeapons();
    const int left = HitListRow(x, y, 0, scroll_[0], owned.size());
    if (left >= 0) {
        const WeaponKind kind = owned[static_cast<size_t>(left)];
        if (world.CanEquip(kind)) {
            world.Equip(kind);
        } else {
            Deny(0, left);
        }
        return false;
    }

    // 右の列：積んでいる武装の詳しい画面を開く
    const int right = HitListRow(x, y, 1, scroll_[1], world.Weapons().size());
    if (right >= 0) {
        detailIndex_ = right;
        deniedArea_ = -1;
        deniedRow_ = -1;
    }
    return false;
}

void BuildScreen::HandleDetailTap(World& world, int x, int y) {
    const size_t index = static_cast<size_t>(detailIndex_);
    if (index >= world.Weapons().size() || Inside(x, y, CloseX, CloseY, CloseW, CloseH)) {
        detailIndex_ = -1;
        return;
    }
    if (Inside(x, y, RemoveX, RemoveY, RemoveW, RemoveH)) {
        world.Unequip(index);
        detailIndex_ = -1;
        scroll_[1] = std::min(scroll_[1], MaxScroll(1, world));
        return;
    }
    const Weapon& w = *world.Weapons()[index];

    // 付けている強化パーツ：タップで1つ外す
    const std::vector<PartKind> attached = AttachedPartKinds(w);
    for (size_t i = 0; i < attached.size(); ++i) {
        if (Inside(x, y, PartLeftX, PartTop + static_cast<int>(i) * RowStep, PartW, RowH)) {
            world.DetachPart(index, attached[i]);
            return;
        }
    }

    // 付けられる強化パーツ：タップで1つ付ける。エネルギーが足りなければ赤く光らせる
    const WeaponFamily family = FamilyOf(w.Kind());
    const auto& parts = FamilyParts(family);
    for (size_t i = 0; i < parts.size(); ++i) {
        if (Inside(x, y, PartRightX, PartTop + static_cast<int>(i) * RowStep, PartW, RowH)) {
            if (world.SpareParts(family, parts[i]) <= 0) {
                return;  // 持っていないものは押せない
            }
            if (world.CanAttachPart(index, parts[i])) {
                world.AttachPart(index, parts[i]);
            } else {
                Deny(1, static_cast<int>(i));
            }
            return;
        }
    }
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
    DrawCornerButton("戻る");
    DrawLine(0, 62, Config::ScreenWidth, 62, GetColor(44, 56, 80));
    DrawLine(399, 76, 399, ListBottom, GetColor(44, 56, 80));
    DrawString(LeftX, 76, "持っている武装（タップで積む）", GetColor(154, 170, 187));
    DrawString(RightX, 76, "積んでいる武装（タップで詳しく）", GetColor(154, 170, 187));

    // 左の列：持っている武装と、持っている強化パーツ（一覧の範囲の外は描かない）
    SetDrawArea(0, ListTop, 399, ListBottom);
    const auto& owned = world.OwnedWeapons();
    const int leftTop = ListTop - static_cast<int>(scroll_[0]);
    for (size_t i = 0; i < owned.size(); ++i) {
        const int top = leftTop + static_cast<int>(i) * RowStep;
        const WeaponKind kind = owned[i];
        if (deniedArea_ == 0 && static_cast<int>(i) == deniedRow_) {
            DrawRow(LeftX, top, RowW, GetColor(90, 20, 20), GetColor(255, 80, 80), WeaponName(kind), GetColor(255, 255, 255),
                    EnergyText(WeaponEnergy(kind)), GetColor(255, 255, 255));
        } else {
            DrawRow(LeftX, top, RowW, GetColor(27, 43, 61), GetColor(59, 95, 128), WeaponName(kind), GetColor(228, 241, 255),
                    EnergyText(WeaponEnergy(kind)), GetColor(255, 215, 106));
        }
    }
    int textY = leftTop + static_cast<int>(owned.size()) * RowStep + 10;
    DrawString(LeftX, textY, "持っている強化パーツ", GetColor(154, 170, 187));
    const std::vector<std::string> lines = SparePartLines(world);
    if (lines.empty()) {
        DrawString(LeftX, textY + LineH, "（なし）", GetColor(185, 198, 214));
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        DrawString(LeftX, textY + LineH * static_cast<int>(i + 1), lines[i].c_str(), GetColor(185, 198, 214));
    }

    // 右の列：積んでいる武装（1つずつ1行。付けた強化パーツの数と、強化パーツを含めたエネルギー）
    SetDrawArea(400, ListTop, Config::ScreenWidth, ListBottom);
    const auto& weapons = world.Weapons();
    const int rightTop = ListTop - static_cast<int>(scroll_[1]);
    for (size_t i = 0; i < weapons.size(); ++i) {
        const Weapon& w = *weapons[i];
        std::string label = WeaponName(w.Kind());
        if (!w.Parts().empty()) {
            label += " ＋" + std::to_string(w.Parts().size());
        }
        DrawRow(RightX, rightTop + static_cast<int>(i) * RowStep, RowW, GetColor(34, 51, 31), GetColor(79, 125, 69), label,
                GetColor(234, 255, 228), EnergyText(w.Energy()), GetColor(255, 215, 106));
    }
    SetDrawArea(0, 0, Config::ScreenWidth, Config::ScreenHeight);

    // スクロールできるときは、右端に今の位置を細い線で出す
    for (int c = 0; c < 2; ++c) {
        const float content = ContentHeight(c, world);
        const float view = static_cast<float>(ListBottom - ListTop);
        if (content > view) {
            const int barH = static_cast<int>(view * view / content);
            const int barY = ListTop + static_cast<int>((view - barH) * scroll_[c] / (content - view));
            const int barX = (c == 0) ? 393 : 794;
            DrawBox(barX, barY, barX + 4, barY + barH, GetColor(59, 95, 128), TRUE);
        }
    }

    // 武装の詳しい画面（組み込み画面の上に重ねる）
    if (detailIndex_ < 0 || static_cast<size_t>(detailIndex_) >= weapons.size()) {
        return;
    }
    const Weapon& w = *weapons[static_cast<size_t>(detailIndex_)];
    const WeaponFamily family = FamilyOf(w.Kind());
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200);
    DrawBox(0, 0, Config::ScreenWidth, Config::ScreenHeight, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(PanelX, PanelY, PanelX + PanelW, PanelY + PanelH, GetColor(17, 26, 42), TRUE);
    DrawBox(PanelX, PanelY, PanelX + PanelW, PanelY + PanelH, GetColor(74, 106, 144), FALSE);
    DrawString(PartLeftX, PanelY + 20, WeaponName(w.Kind()), GetColor(255, 255, 255));
    const int partsEnergy = w.Energy() - WeaponEnergy(w.Kind());
    DrawFormatString(PartLeftX, PanelY + 50, GetColor(255, 215, 106), "この武装のエネルギー　%d（本体 %d ＋ 強化パーツ %d）",
                     w.Energy(), WeaponEnergy(w.Kind()), partsEnergy);
    DrawButton(CloseX, CloseY, CloseW, CloseH, "閉じる", GetColor(18, 50, 74), GetColor(127, 208, 255), GetColor(223, 244, 255));

    DrawString(PartLeftX, PartTop - 24, "付けている強化パーツ（タップで外す）", GetColor(154, 170, 187));
    const std::vector<PartKind> attached = AttachedPartKinds(w);
    if (attached.empty()) {
        DrawString(PartLeftX, PartTop + 10, "（なし）", GetColor(185, 198, 214));
    }
    for (size_t i = 0; i < attached.size(); ++i) {
        DrawRow(PartLeftX, PartTop + static_cast<int>(i) * RowStep, PartW, GetColor(61, 42, 27), GetColor(176, 122, 60),
                std::string(PartName(attached[i])) + " ×" + std::to_string(w.CountPart(attached[i])), GetColor(255, 241, 224),
                "", 0);
    }

    DrawString(PartRightX, PartTop - 24, "付けられる強化パーツ（タップで付ける）", GetColor(154, 170, 187));
    const auto& parts = FamilyParts(family);
    for (size_t i = 0; i < parts.size(); ++i) {
        const int top = PartTop + static_cast<int>(i) * RowStep;
        const int spare = world.SpareParts(family, parts[i]);
        const std::string label = std::string(PartName(parts[i])) + " ×" + std::to_string(spare);
        if (deniedArea_ == 1 && static_cast<int>(i) == deniedRow_) {
            DrawRow(PartRightX, top, PartW, GetColor(90, 20, 20), GetColor(255, 80, 80), label, GetColor(255, 255, 255),
                    "＋" + std::to_string(w.NextPartEnergy()), GetColor(255, 255, 255));
            DrawString(PartRightX, top + RowH + 2, "エネルギーが足りません", GetColor(255, 128, 128));
        } else if (spare > 0) {
            DrawRow(PartRightX, top, PartW, GetColor(46, 36, 64), GetColor(122, 95, 168), label, GetColor(241, 232, 255),
                    "＋" + std::to_string(w.NextPartEnergy()), GetColor(255, 215, 106));
        } else {
            // 持っていないものは薄く表示する（押せない）
            DrawRow(PartRightX, top, PartW, GetColor(26, 22, 34), GetColor(60, 52, 76), label, GetColor(110, 104, 122), "", 0);
        }
    }

    // この系統の強化パーツの説明
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string text = std::string(PartName(parts[i])) + "：" + PartDescription(family, parts[i]);
        DrawString(PartLeftX, DescTop + static_cast<int>(i) * LineH, text.c_str(), GetColor(185, 198, 214));
    }

    DrawButton(RemoveX, RemoveY, RemoveW, RemoveH, "この武装を外す", GetColor(74, 26, 26), GetColor(255, 112, 112),
               GetColor(255, 208, 208));
}

bool BuildScreen::HitOpenButton(int x, int y) {
    return Inside(x, y, ButtonX, ButtonY, ButtonW, ButtonH);
}

void BuildScreen::DrawOpenButton() {
    DrawCornerButton("組み込み");
}
