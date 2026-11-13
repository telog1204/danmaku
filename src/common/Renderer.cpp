#include "pch.h"
#include "Renderer.h"
#include <cstdio>
#include <DxLib.h>
#include "Config.h"
#include "World.h"

namespace {
    int Px(float v) { return static_cast<int>(std::lround(v)); }

    // 光って見える丸。加算合成で、薄い大きな丸の上に明るい丸を重ねる
    void DrawGlowCircle(const Vec2& p, float r, int cr, int cg, int cb) {
        DrawCircle(Px(p.x), Px(p.y), Px(r * 2.2f), GetColor(cr / 5, cg / 5, cb / 5), TRUE);
        DrawCircle(Px(p.x), Px(p.y), Px(r), GetColor(cr, cg, cb), TRUE);
    }

    void DrawCore(const Core& core) {
        DrawGlowCircle(core.pos, core.radius, 80, 220, 255);
        DrawCircle(Px(core.pos.x), Px(core.pos.y), Px(core.radius + 6.0f), GetColor(40, 120, 160), FALSE, 2);
    }

    void DrawEnemy(const Enemy& e) {
        const int x = Px(e.pos.x);
        const int y = Px(e.pos.y);
        const int r = Px(e.radius);
        switch (e.type) {
        case EnemyType::Charger: {
            // 機首の向きを指す三角
            const Vec2 f = Vec2::FromAngle(e.heading);
            const Vec2 s = f.Perpendicular();
            const Vec2 tip = e.pos + f * e.radius * 1.3f;
            const Vec2 left = e.pos - f * e.radius + s * e.radius;
            const Vec2 right = e.pos - f * e.radius - s * e.radius;
            DrawCircle(x, y, Px(e.radius * 2.0f), GetColor(50, 10, 10), TRUE);
            DrawTriangle(Px(tip.x), Px(tip.y), Px(left.x), Px(left.y), Px(right.x), Px(right.y), GetColor(255, 70, 60), TRUE);
            break;
        }
        case EnemyType::Shooter:
            // 四角
            DrawCircle(x, y, Px(e.radius * 2.0f), GetColor(50, 30, 5), TRUE);
            DrawBox(x - r, y - r, x + r, y + r, GetColor(255, 160, 40), TRUE);
            break;
        case EnemyType::Evader:
            // ひし形（三角2つ）
            DrawCircle(x, y, Px(e.radius * 2.0f), GetColor(45, 10, 50), TRUE);
            DrawTriangle(x, y - r - 3, x - r, y, x + r, y, GetColor(230, 80, 255), TRUE);
            DrawTriangle(x, y + r + 3, x - r, y, x + r, y, GetColor(230, 80, 255), TRUE);
            break;
        }
        // HPが減っていたら、残りを細い線で出す
        if (e.hp < e.maxHp) {
            const int w = r * 2;
            const int filled = w * std::max(0, e.hp) / e.maxHp;
            DrawBox(x - r, y - r - 7, x - r + filled, y - r - 5, GetColor(120, 255, 120), TRUE);
        }
    }

    void DrawHud(const World& world) {
        const Core& core = world.GetCore();
        // HPのゲージ
        const int barX = 10;
        const int barY = 10;
        const int barW = 200;
        const int barH = 10;
        DrawBox(barX, barY, barX + barW, barY + barH, GetColor(40, 40, 60), TRUE);
        DrawBox(barX, barY, barX + barW * core.hp / core.maxHp, barY + barH, GetColor(80, 220, 255), TRUE);
        DrawFormatString(barX, barY + 14, GetColor(255, 255, 255), "HP %d / %d", core.hp, core.maxHp);
        DrawFormatString(Config::ScreenWidth - 140, barY, GetColor(255, 255, 255), "TIME %6.1f", world.ElapsedSeconds());
    }

    void DrawCentered(int y, unsigned int color, const char* text) {
        const int w = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
        DrawString((Config::ScreenWidth - w) / 2, y, text, color);
    }

    void DrawGameOver(const World& world) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
        DrawBox(0, 0, Config::ScreenWidth, Config::ScreenHeight, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        const int cy = Config::ScreenHeight / 2;
        DrawCentered(cy - 30, GetColor(255, 80, 80), "GAME OVER");
        char line[64];
        std::snprintf(line, sizeof(line), "TIME %.1f", world.ElapsedSeconds());
        DrawCentered(cy, GetColor(255, 255, 255), line);
        if (world.GameOverSeconds() >= Config::RestartDelay) {
            DrawCentered(cy + 30, GetColor(200, 200, 200), "タップ（クリック）でやり直し");
        }
    }
}

void DrawWorld(const World& world) {
    // 光るもの（コア・敵・弾）は加算合成で描く
    SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
    DrawCore(world.GetCore());
    for (const Enemy& e : world.Enemies()) {
        DrawEnemy(e);
    }
    for (const Bullet& b : world.PlayerBullets()) {
        DrawGlowCircle(b.pos, b.radius, 120, 255, 230);
    }
    for (const Bullet& b : world.EnemyBullets()) {
        DrawGlowCircle(b.pos, b.radius, 255, 90, 160);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawHud(world);
    if (world.IsGameOver()) {
        DrawGameOver(world);
    }
}
