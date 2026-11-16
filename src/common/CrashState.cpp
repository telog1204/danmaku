#include "pch.h"
#include "CrashState.h"

namespace {
    // 異常終了の瞬間にも読めるよう、決まった場所に置く
    volatile int g_phase = 0;
    volatile int g_elapsedTenths = 0;
    volatile int g_counts[7] = {};

    const char* PhaseName(int phase) {
        switch (static_cast<CrashState::Phase>(phase)) {
        case CrashState::Phase::Menu: return "戦闘以外の画面";
        case CrashState::Phase::Spawn: return "敵の出現";
        case CrashState::Phase::Weapons: return "武装";
        case CrashState::Phase::Enemies: return "敵の動き";
        case CrashState::Phase::MoveBullets: return "弾の移動";
        case CrashState::Phase::MoveBeams: return "ビームの移動";
        case CrashState::Phase::MoveMissiles: return "誘導弾の移動";
        case CrashState::Phase::MoveShells: return "炸裂弾の移動";
        case CrashState::Phase::Explosions: return "爆発";
        case CrashState::Phase::Collisions: return "当たり判定";
        case CrashState::Phase::Messages: return "知らせ";
        case CrashState::Phase::Cleanup: return "消えたものの取り除き";
        case CrashState::Phase::Draw: return "描画";
        }
        return "不明";
    }

    // buffer に文字列を足していく（あふれる分は書かない）
    struct Writer {
        char* buffer;
        size_t capacity;
        size_t length;

        void Text(const char* s) {
            while (*s && length + 1 < capacity) buffer[length++] = *s++;
        }
        void Number(int value) {
            char digits[12];
            int n = 0;
            bool negative = value < 0;
            unsigned int v = negative ? static_cast<unsigned int>(-(value + 1)) + 1u : static_cast<unsigned int>(value);
            do {
                digits[n++] = static_cast<char>('0' + v % 10);
                v /= 10;
            } while (v > 0 && n < 11);
            if (negative) Text("-");
            while (n > 0 && length + 1 < capacity) buffer[length++] = digits[--n];
        }
    };
}

void CrashState::SetPhase(Phase phase) {
    g_phase = static_cast<int>(phase);
}

void CrashState::SetElapsed(float seconds) {
    g_elapsedTenths = static_cast<int>(seconds * 10.0f);
}

void CrashState::SetCounts(int enemies, int enemyBullets, int playerBullets, int missiles, int beams, int shells,
                           int explosions) {
    g_counts[0] = enemies;
    g_counts[1] = enemyBullets;
    g_counts[2] = playerBullets;
    g_counts[3] = missiles;
    g_counts[4] = beams;
    g_counts[5] = shells;
    g_counts[6] = explosions;
}

size_t CrashState::FormatReport(char* buffer, size_t capacity, const char* signalName) {
    Writer w{ buffer, capacity, 0 };
    w.Text("種類：");
    w.Text(signalName);
    w.Text("\n経過時間：");
    w.Number(g_elapsedTenths / 10);
    w.Text(".");
    w.Number(g_elapsedTenths % 10);
    w.Text("秒　処理：");
    w.Text(PhaseName(g_phase));
    w.Text("\n敵 ");
    w.Number(g_counts[0]);
    w.Text("／敵の弾 ");
    w.Number(g_counts[1]);
    w.Text("／自分の弾 ");
    w.Number(g_counts[2]);
    w.Text("\n誘導弾 ");
    w.Number(g_counts[3]);
    w.Text("／ビーム ");
    w.Number(g_counts[4]);
    w.Text("／炸裂弾 ");
    w.Number(g_counts[5]);
    w.Text("／爆発 ");
    w.Number(g_counts[6]);
    w.Text("\n");
    if (capacity > 0) {
        buffer[w.length] = '\0';
    }
    return w.length;
}
