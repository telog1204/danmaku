#include "pch.h"
#include "Renderer.h"
#include <cstdio>
#include <DxLib.h>
#include "Config.h"
#include "World.h"

namespace {
    // フィールドの位置を画面の位置にする（カメラを引いて、ViewScale 倍に縮めて描く）
    int Px(float v) { return static_cast<int>(std::lround(v * Config::ViewScale)); }

    // フィールドの大きさ（半径・長さ）を画面の大きさにする。小さくなりすぎて消えないように、最低1ドットにする
    int Sz(float v) { return std::max(1, static_cast<int>(std::lround(v * Config::ViewScale))); }

    // 光って見える丸。加算合成で、薄い大きな丸の上に明るい丸を重ねる
    void DrawGlowCircle(const Vec2& p, float r, int cr, int cg, int cb) {
        DrawCircle(Px(p.x), Px(p.y), Sz(r * 2.2f), GetColor(cr / 5, cg / 5, cb / 5), TRUE);
        DrawCircle(Px(p.x), Px(p.y), Sz(r), GetColor(cr, cg, cb), TRUE);
    }

    // コア：明るい水色の丸だけ（周りの光や縁取りは描かない。後ろの砲身が縁から見えるように）
    void DrawCore(const Core& core) {
        DrawCircle(Px(core.pos.x), Px(core.pos.y), Sz(core.radius), GetColor(80, 220, 255), TRUE);
    }

    // 砲身（速射砲・ビーム砲）：コアの中心から、今向いている方向へ伸ばす。連装は、弾が出る砲の並びに合わせて平行に並べる。
    // rapid が true なら速射砲だけ、false ならビーム砲だけを描く（ビーム砲を先に描いて、速射砲を上に重ねるため）
    void DrawBarrels(const World& world, bool rapidOnly) {
        const Vec2 center = world.GetCore().pos;
        for (const auto& w : world.Weapons()) {
            if (!w->HasTurret()) continue;  // 向きを持たない武装（誘導兵器・炸裂弾）は描かない
            const bool rapid = (FamilyOf(w->Kind()) == WeaponFamily::RapidGun);
            if (rapid != rapidOnly) continue;
            const int barrels = w->Kind().barrels;
            // 速射砲の四連装は、撃ち方と同じく2門の並び
            const int mounts = (rapid && barrels == 4) ? 2 : barrels;
            const float spacing = rapid ? Config::RapidBarrelSpacing : Config::BeamBarrelSpacing;
            const float length = rapid ? Config::RapidBarrelLength : Config::BeamBarrelLength;
            const unsigned int color = rapid ? GetColor(255, 60, 40) : GetColor(150, 220, 255);
            const int thickness = Sz(rapid ? 3.0f : 4.0f);
            const Vec2 dir = Vec2::FromAngle(w->TurretAngle());
            const Vec2 side = dir.Perpendicular();
            for (int i = 0; i < mounts; ++i) {
                const Vec2 base = center + side * ((static_cast<float>(i) - (mounts - 1) * 0.5f) * spacing);
                const Vec2 tip = base + dir * length;
                DrawLine(Px(base.x), Px(base.y), Px(tip.x), Px(tip.y), color, thickness);
            }
        }
    }

    // 誘導兵器の発射装置：コアの左右から生える黄色の四角（固定で回らない）。誘導兵器を積んでいるときだけ、左右1つずつ描く。
    // コアの中心から外へ伸ばして、コアの後ろに描くので、コアから生えて見える
    void DrawMissileLaunchers(const World& world) {
        bool hasMissile = false;
        for (const auto& w : world.Weapons()) {
            if (FamilyOf(w->Kind()) == WeaponFamily::Missile) {
                hasMissile = true;
                break;
            }
        }
        if (!hasMissile) return;
        const Vec2 c = world.GetCore().pos;
        const float h = Config::MissileLauncherHeight * 0.5f;
        const unsigned int color = GetColor(255, 230, 90);
        DrawBox(Px(c.x - Config::MissileLauncherLength), Px(c.y - h), Px(c.x), Px(c.y + h), color, TRUE);
        DrawBox(Px(c.x), Px(c.y - h), Px(c.x + Config::MissileLauncherLength), Px(c.y + h), color, TRUE);
    }

    // 機首の向きを指す三角（大きさ size、色 c）
    void DrawArrow(const Enemy& e, float size, unsigned int c) {
        const Vec2 f = Vec2::FromAngle(e.heading);
        const Vec2 s = f.Perpendicular();
        const Vec2 tip = e.pos + f * size * 1.3f;
        const Vec2 left = e.pos - f * size + s * size;
        const Vec2 right = e.pos - f * size - s * size;
        DrawTriangle(Px(tip.x), Px(tip.y), Px(left.x), Px(left.y), Px(right.x), Px(right.y), c, TRUE);
    }

    // 型ごとの形と色。区分によって大きさが変わる（重はふちを付け、大艦隊の旗艦は金色の輪を付ける）
    void DrawEnemy(const Enemy& e) {
        const int x = Px(e.pos.x);
        const int y = Px(e.pos.y);
        const int r = Sz(e.radius);
        switch (e.type) {
        case EnemyType::Strafer:
            // 離脱型：紫の矢じり
            DrawCircle(x, y, Sz(e.radius * 2.0f), GetColor(45, 10, 50), TRUE);
            DrawArrow(e, e.radius, GetColor(230, 80, 255));
            break;
        case EnemyType::Kamikaze:
            // 自爆型：赤い矢じり
            DrawCircle(x, y, Sz(e.radius * 2.0f), GetColor(50, 10, 10), TRUE);
            DrawArrow(e, e.radius, GetColor(255, 70, 60));
            break;
        case EnemyType::Gunship:
            // 基本型：オレンジの四角
            DrawCircle(x, y, Sz(e.radius * 2.0f), GetColor(50, 30, 5), TRUE);
            DrawBox(x - r, y - r, x + r, y + r, GetColor(255, 160, 40), TRUE);
            break;
        case EnemyType::Torpedo:
            // 雷撃型：緑の四角に、機首の向きを指す線
            DrawCircle(x, y, Sz(e.radius * 2.0f), GetColor(10, 45, 25), TRUE);
            DrawBox(x - r, y - r, x + r, y + r, GetColor(80, 220, 130), TRUE);
            {
                const Vec2 tip = e.pos + Vec2::FromAngle(e.heading) * (e.radius * 1.6f);
                DrawLine(x, y, Px(tip.x), Px(tip.y), GetColor(160, 255, 190), Sz(3.0f));
            }
            break;
        case EnemyType::Carrier:
            // 空母型：青い横長の板（機首の向きに長い）
            DrawCircle(x, y, Sz(e.radius * 2.0f), GetColor(10, 25, 55), TRUE);
            {
                const Vec2 f = Vec2::FromAngle(e.heading) * (e.radius * 1.4f);
                const Vec2 s = Vec2::FromAngle(e.heading).Perpendicular() * (e.radius * 0.7f);
                const Vec2 a = e.pos + f + s, b = e.pos + f - s, c = e.pos - f - s, d = e.pos - f + s;
                const unsigned int col = GetColor(90, 150, 255);
                DrawTriangle(Px(a.x), Px(a.y), Px(b.x), Px(b.y), Px(c.x), Px(c.y), col, TRUE);
                DrawTriangle(Px(a.x), Px(a.y), Px(c.x), Px(c.y), Px(d.x), Px(d.y), col, TRUE);
            }
            break;
        }
        if (e.cls == EnemyClass::Heavy) {
            DrawCircle(x, y, Sz(e.radius * 1.5f), GetColor(200, 200, 220), FALSE, Sz(2.0f));
        }
        if (e.grandFlagship) {
            DrawCircle(x, y, Sz(e.radius * 1.9f), GetColor(255, 210, 80), FALSE, Sz(2.0f));
        }
        // HPが減っていたら、残りを細い線で出す（大艦隊の旗艦は画面の上に出すので出さない）
        if (e.hp < e.maxHp && !e.grandFlagship && e.cls != EnemyClass::Light) {
            const int w = r * 2;
            const int filled = static_cast<int>(w * std::max(0.0f, e.hp) / e.maxHp);
            DrawBox(x - r, y - r - Sz(7.0f), x - r + filled, y - r - Sz(7.0f) + Sz(2.0f), GetColor(120, 255, 120), TRUE);
        }
    }

    // 敵の誘導弾：進む向きを指す細長い三角
    void DrawEnemyMissile(const Bullet& b) {
        const Vec2 f = b.vel.Normalized();
        const Vec2 s = f.Perpendicular();
        const Vec2 tip = b.pos + f * 8.0f;
        const Vec2 left = b.pos - f * 5.0f + s * 3.5f;
        const Vec2 right = b.pos - f * 5.0f - s * 3.5f;
        DrawCircle(Px(b.pos.x), Px(b.pos.y), Sz(9.0f), GetColor(60, 15, 30), TRUE);
        DrawTriangle(Px(tip.x), Px(tip.y), Px(left.x), Px(left.y), Px(right.x), Px(right.y), GetColor(255, 110, 150), TRUE);
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

        // 大艦隊の旗艦の残りHP（画面の上の中央）。現れる少し前は「大艦隊接近」を点滅させる
        if (world.IsGrandFleetActive()) {
            const int bx = 250;
            const int by = 78;
            const int bw = 300;
            const char* label = "大艦隊";
            DrawString((Config::ScreenWidth - GetDrawStringWidth(label, static_cast<int>(std::strlen(label)))) / 2, 58,
                       label, GetColor(255, 210, 120));
            DrawBox(bx, by, bx + bw, by + 10, GetColor(60, 45, 20), TRUE);
            DrawBox(bx, by, bx + static_cast<int>(bw * world.GrandFleetHpRatio()), by + 10, GetColor(255, 190, 70), TRUE);
        } else if (world.IsGrandFleetWarning() && static_cast<int>(world.ElapsedSeconds() * 4.0f) % 2 == 0) {
            const char* label = "大艦隊接近";
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

    // 速射砲の弾：赤一色の短い線（進む向きの後ろへ尾を伸ばす。当たり判定は丸のまま）
    void DrawRapidBullet(const Bullet& b) {
        const Vec2 tail = b.pos - b.vel.Normalized() * Config::RapidBulletTailLength;
        DrawLine(Px(tail.x), Px(tail.y), Px(b.pos.x), Px(b.pos.y), GetColor(255, 60, 40), Sz(3.0f));
    }

    void DrawBeam(const Beam& b) {
        const Vec2 tail = b.Tail();
        DrawLine(Px(tail.x), Px(tail.y), Px(b.pos.x), Px(b.pos.y), GetColor(30, 60, 90), Sz(7.0f));
        DrawLine(Px(tail.x), Px(tail.y), Px(b.pos.x), Px(b.pos.y), GetColor(150, 220, 255), Sz(3.0f));
        DrawCircle(Px(b.pos.x), Px(b.pos.y), Sz(3.0f), GetColor(255, 255, 255), TRUE);
    }

    void DrawMissile(const Missile& m) {
        // 進む向きを指す細長い三角
        const Vec2 f = Vec2::FromAngle(m.heading);
        const Vec2 s = f.Perpendicular();
        const Vec2 tip = m.pos + f * 7.0f;
        const Vec2 left = m.pos - f * 5.0f + s * 3.0f;
        const Vec2 right = m.pos - f * 5.0f - s * 3.0f;
        const Vec2 flame = m.pos - f * 9.0f;
        DrawCircle(Px(flame.x), Px(flame.y), Sz(3.0f), GetColor(90, 50, 10), TRUE);
        DrawTriangle(Px(tip.x), Px(tip.y), Px(left.x), Px(left.y), Px(right.x), Px(right.y), GetColor(255, 230, 90), TRUE);
    }

    void DrawExplosion(const Explosion& x) {
        // 残り時間が少ないほど暗くする
        const float k = x.maxLife > 0.0f ? std::max(0.0f, x.life / x.maxLife) : 0.0f;
        const int bright = static_cast<int>(60 + 140 * k);
        DrawCircle(Px(x.pos.x), Px(x.pos.y), Sz(x.radius), GetColor(bright, bright * 2 / 5, bright / 8), TRUE);
        DrawCircle(Px(x.pos.x), Px(x.pos.y), Sz(x.radius * 0.5f), GetColor(bright, bright * 3 / 5, bright / 4), TRUE);
    }

}

void DrawWorld(const World& world) {
    // 誘導兵器の発射装置と砲身はコアの後ろに、色がそのまま見えるように描く（発射装置の上にビーム砲、その上に速射砲を重ねる）
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawMissileLaunchers(world);
    DrawBarrels(world, false);
    DrawBarrels(world, true);
    DrawCore(world.GetCore());
    // 光るもの（敵・弾）は加算合成で描く
    SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
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
            DrawRapidBullet(b);
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
    for (const Bullet& b : world.EnemyBullets()) {
        if (b.homing) {
            DrawEnemyMissile(b);
        } else {
            DrawGlowCircle(b.pos, b.radius, 255, 90, 160);
        }
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawHud(world);
}
