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
        case EnemyType::Boss:
            // 大きな四角（重装型を大きくした形）。HPは画面の上に出すので、ここでは出さない
            DrawCircle(x, y, Px(e.radius * 1.9f), GetColor(58, 16, 16), TRUE);
            DrawBox(x - r, y - r, x + r, y + r, GetColor(255, 176, 144), TRUE);
            DrawBox(x - r + 4, y - r + 4, x + r - 4, y + r - 4, GetColor(255, 90, 60), TRUE);
            return;
        }
        // HPが減っていたら、残りを細い線で出す
        if (e.hp < e.maxHp) {
            const int w = r * 2;
            const int filled = static_cast<int>(w * std::max(0.0f, e.hp) / e.maxHp);
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
        DrawFormatString(540, barY, GetColor(255, 255, 255), "TIME %6.1f", world.ElapsedSeconds());

        // 強敵のHP（画面の上の中央）。現れる少し前は「強敵接近」を点滅させる
        if (const Enemy* boss = world.FindBoss()) {
            const int bx = 250;
            const int by = 78;
            const int bw = 300;
            const char* label = "強敵";
            DrawString((Config::ScreenWidth - GetDrawStringWidth(label, static_cast<int>(std::strlen(label)))) / 2, 58,
                       label, GetColor(255, 154, 154));
            DrawBox(bx, by, bx + bw, by + 10, GetColor(60, 30, 30), TRUE);
            DrawBox(bx, by, bx + static_cast<int>(bw * std::max(0.0f, boss->hp) / boss->maxHp), by + 10,
                    GetColor(255, 80, 80), TRUE);
        } else if (world.IsBossWarning() && static_cast<int>(world.ElapsedSeconds() * 4.0f) % 2 == 0) {
            const char* label = "強敵接近";
            DrawString((Config::ScreenWidth - GetDrawStringWidth(label, static_cast<int>(std::strlen(label)))) / 2, 64,
                       label, GetColor(255, 90, 90));
        }

        // 積んでいる武装（同じものはまとめて数を出す）
        int y = Config::ScreenHeight - 22;
        const auto& weapons = world.Weapons();
        for (size_t i = 0; i < weapons.size();) {
            const WeaponKind kind = weapons[i]->Kind();
            size_t n = 0;
            while (i < weapons.size() && weapons[i]->Kind() == kind) {
                ++n;
                ++i;
            }
            if (n > 1) {
                DrawFormatString(10, y, GetColor(200, 230, 255), "%s ×%d", WeaponName(kind).c_str(), static_cast<int>(n));
            } else {
                DrawFormatString(10, y, GetColor(200, 230, 255), "%s", WeaponName(kind).c_str());
            }
            y -= 20;
        }

        // 拾ったものの知らせ（強敵のHPの下、新しいものほど下）
        int my = 96;
        for (const Message& m : world.Messages()) {
            unsigned int color = GetColor(255, 233, 160);
            if (m.kind == PickupKind::Part) color = GetColor(217, 194, 255);
            if (m.kind == PickupKind::Weapon) color = GetColor(200, 240, 255);
            const int w = GetDrawStringWidth(m.text.c_str(), static_cast<int>(m.text.size()));
            DrawString((Config::ScreenWidth - w) / 2, my, m.text.c_str(), color);
            my += 22;
        }
    }

    void DrawBeam(const Beam& b) {
        const Vec2 tail = b.Tail();
        DrawLine(Px(tail.x), Px(tail.y), Px(b.pos.x), Px(b.pos.y), GetColor(30, 60, 90), 7);
        DrawLine(Px(tail.x), Px(tail.y), Px(b.pos.x), Px(b.pos.y), GetColor(150, 220, 255), 3);
        DrawCircle(Px(b.pos.x), Px(b.pos.y), 3, GetColor(255, 255, 255), TRUE);
    }

    void DrawMissile(const Missile& m) {
        // 進む向きを指す細長い三角
        const Vec2 f = Vec2::FromAngle(m.heading);
        const Vec2 s = f.Perpendicular();
        const Vec2 tip = m.pos + f * 7.0f;
        const Vec2 left = m.pos - f * 5.0f + s * 3.0f;
        const Vec2 right = m.pos - f * 5.0f - s * 3.0f;
        const Vec2 flame = m.pos - f * 9.0f;
        DrawCircle(Px(flame.x), Px(flame.y), 3, GetColor(90, 50, 10), TRUE);
        DrawTriangle(Px(tip.x), Px(tip.y), Px(left.x), Px(left.y), Px(right.x), Px(right.y), GetColor(255, 230, 90), TRUE);
    }

    // 系統の色（落とし物に使う）
    void FamilyColor(WeaponFamily family, int& r, int& g, int& b) {
        switch (family) {
        case WeaponFamily::RapidGun: r = 80; g = 220; b = 230; return;
        case WeaponFamily::Beam: r = 90; g = 140; b = 255; return;
        case WeaponFamily::Missile: r = 230; g = 210; b = 60; return;
        case WeaponFamily::Shell: r = 255; g = 140; b = 40; return;
        }
        r = g = b = 255;
    }

    void DrawPickup(const Pickup& p) {
        const int x = Px(p.pos.x);
        const int y = Px(p.pos.y);
        int r = 255, g = 215, b = 106;
        switch (p.kind) {
        case PickupKind::Energy:
            // 黄色のひし形
            DrawCircle(x, y, 14, GetColor(60, 50, 20), TRUE);
            DrawTriangle(x, y - 9, x - 9, y, x + 9, y, GetColor(255, 215, 106), TRUE);
            DrawTriangle(x, y + 9, x - 9, y, x + 9, y, GetColor(255, 215, 106), TRUE);
            break;
        case PickupKind::Weapon:
            // 白い四角（ふちは系統の色）
            FamilyColor(p.family, r, g, b);
            DrawCircle(x, y, 14, GetColor(r / 5, g / 5, b / 5), TRUE);
            DrawBox(x - 8, y - 8, x + 9, y + 9, GetColor(r, g, b), TRUE);
            DrawBox(x - 5, y - 5, x + 6, y + 6, GetColor(255, 255, 255), TRUE);
            break;
        case PickupKind::Part:
            // 小さな丸（系統の色）
            FamilyColor(p.family, r, g, b);
            DrawGlowCircle(p.pos, 6.0f, r, g, b);
            break;
        }
    }

    void DrawExplosion(const Explosion& x) {
        // 残り時間が少ないほど暗くする
        const float k = x.maxLife > 0.0f ? std::max(0.0f, x.life / x.maxLife) : 0.0f;
        const int bright = static_cast<int>(60 + 140 * k);
        DrawCircle(Px(x.pos.x), Px(x.pos.y), Px(x.radius), GetColor(bright, bright * 2 / 5, bright / 8), TRUE);
        DrawCircle(Px(x.pos.x), Px(x.pos.y), Px(x.radius * 0.5f), GetColor(bright, bright * 3 / 5, bright / 4), TRUE);
    }

}

void DrawWorld(const World& world) {
    // 光るもの（コア・敵・弾）は加算合成で描く
    SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
    DrawCore(world.GetCore());
    for (const Enemy& e : world.Enemies()) {
        DrawEnemy(e);
    }
    for (const Explosion& x : world.Explosions()) {
        DrawExplosion(x);
    }
    for (const Bullet& b : world.PlayerBullets()) {
        if (b.life >= 0.0f) {
            DrawGlowCircle(b.pos, b.radius, 255, 170, 60);  // 炸裂弾の破片
        } else {
            DrawGlowCircle(b.pos, b.radius, 120, 255, 230);
        }
    }
    for (const Beam& b : world.Beams()) {
        DrawBeam(b);
    }
    for (const Missile& m : world.Missiles()) {
        DrawMissile(m);
    }
    for (const Shell& s : world.Shells()) {
        DrawGlowCircle(s.pos, Config::ShellRadius, 255, 140, 40);
    }
    for (const Pickup& p : world.Pickups()) {
        DrawPickup(p);
    }
    for (const Bullet& b : world.EnemyBullets()) {
        DrawGlowCircle(b.pos, b.radius, 255, 90, 160);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawHud(world);
}
