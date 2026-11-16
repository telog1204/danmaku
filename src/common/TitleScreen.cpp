#include "pch.h"
#include "TitleScreen.h"
#include <cstdio>
#include <DxLib.h>
#include "Config.h"

namespace {
    constexpr int ParticleCount = 40;

    int Px(float v) { return static_cast<int>(std::lround(v)); }

    void DrawCenteredWithFont(int y, const char* text, unsigned int color, int font) {
        const int w = GetDrawStringWidthToHandle(text, static_cast<int>(std::strlen(text)), font);
        DrawStringToHandle((Config::ScreenWidth - w) / 2, y, text, color, font);
    }

    void DrawCentered(int y, const char* text, unsigned int color) {
        const int w = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
        DrawString((Config::ScreenWidth - w) / 2, y, text, color);
    }
}

int TitleFont() {
    static int font = -1;
    if (font == -1) {
        font = CreateFontToHandle(nullptr, 72, 4, DX_FONTTYPE_ANTIALIASING);
    }
    return font;
}

int LargeFont() {
    static int font = -1;
    if (font == -1) {
        font = CreateFontToHandle(nullptr, 40, 3, DX_FONTTYPE_ANTIALIASING);
    }
    return font;
}

TitleScreen::TitleScreen() {
    // 背景に流す光る弾（色は、敵の弾・速射砲の弾・手に入れたものの知らせの色から）
    static const int colors[][3] = { { 255, 90, 160 }, { 255, 60, 40 }, { 255, 215, 106 }, { 199, 155, 255 } };
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> x(0.0f, static_cast<float>(Config::ScreenWidth));
    std::uniform_real_distribution<float> y(0.0f, static_cast<float>(Config::ScreenHeight));
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> speed(10.0f, 40.0f);
    std::uniform_real_distribution<float> radius(2.0f, 5.0f);
    std::uniform_int_distribution<int> color(0, 3);
    for (int i = 0; i < ParticleCount; ++i) {
        Particle p;
        p.pos = Vec2(x(rng), y(rng));
        p.vel = Vec2::FromAngle(angle(rng)) * speed(rng);
        p.radius = radius(rng);
        const int c = color(rng);
        p.r = colors[c][0];
        p.g = colors[c][1];
        p.b = colors[c][2];
        particles_.push_back(p);
    }
}

void TitleScreen::Update(float dt) {
    // 画面の端から出たら、反対側から戻ってくる
    const float w = static_cast<float>(Config::ScreenWidth);
    const float h = static_cast<float>(Config::ScreenHeight);
    for (Particle& p : particles_) {
        p.pos += p.vel * dt;
        if (p.pos.x < -10.0f) p.pos.x += w + 20.0f;
        if (p.pos.x > w + 10.0f) p.pos.x -= w + 20.0f;
        if (p.pos.y < -10.0f) p.pos.y += h + 20.0f;
        if (p.pos.y > h + 10.0f) p.pos.y -= h + 20.0f;
    }
}

void TitleScreen::Draw(const Records& records, const std::string& crashReport) const {
    DrawBox(0, 0, Config::ScreenWidth, Config::ScreenHeight, GetColor(3, 5, 10), TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
    for (const Particle& p : particles_) {
        DrawCircle(Px(p.pos.x), Px(p.pos.y), Px(p.radius * 2.5f), GetColor(p.r / 5, p.g / 5, p.b / 5), TRUE);
        DrawCircle(Px(p.pos.x), Px(p.pos.y), Px(p.radius), GetColor(p.r, p.g, p.b), TRUE);
    }
    // タイトルは、ずらして重ねた薄い文字で光って見せる
    for (int d = 3; d >= 1; --d) {
        const int w = GetDrawStringWidthToHandle("弾幕", static_cast<int>(std::strlen("弾幕")), TitleFont());
        DrawStringToHandle((Config::ScreenWidth - w) / 2 - d, 170 - d, "弾幕", GetColor(20, 60, 80), TitleFont());
        DrawStringToHandle((Config::ScreenWidth - w) / 2 + d, 170 + d, "弾幕", GetColor(20, 60, 80), TitleFont());
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawCenteredWithFont(170, "弾幕", GetColor(191, 244, 255), TitleFont());

    DrawCentered(330, "タップ（クリック）で始める", GetColor(255, 255, 255));
    if (records.hasRecord) {
        char line[128];
        std::snprintf(line, sizeof(line), "最高記録　生存 %.1f秒　／　撃破 %d体", records.bestSeconds, records.bestKills);
        DrawCentered(410, line, GetColor(255, 233, 160));
    }

    // 前回の異常終了の記録（原因を調べるため）
    if (!crashReport.empty()) {
        int y = 446;
        DrawString(20, y, "前回、異常終了しました（この画面を保存して知らせてください）", GetColor(255, 110, 110));
        y += 20;
        size_t start = 0;
        while (start < crashReport.size() && y < Config::ScreenHeight - 16) {
            size_t end = crashReport.find('\n', start);
            if (end == std::string::npos) end = crashReport.size();
            const std::string line = crashReport.substr(start, end - start);
            DrawString(20, y, line.c_str(), GetColor(255, 190, 190));
            y += 18;
            start = end + 1;
        }
    }
}
