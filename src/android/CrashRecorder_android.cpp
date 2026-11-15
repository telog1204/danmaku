#include "pch.h"
#include "Platform.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "CrashState.h"

// Android：アプリが異常終了したときに、直前の状況を crash.txt に書き残す。
// 異常終了の合図（シグナル）を受け取る仕組みを登録し、受け取ったら書き出してから、もとの仕組み（OSの記録）に任せて終了する

namespace {
    constexpr const char* CrashFileName = "crash.txt";
    char g_path[512] = {};               // 書き出す先（起動時に決めておく）
    const int Signals[] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL };
    constexpr int SignalCount = 5;
    struct sigaction g_previous[SignalCount];
    char g_altStack[64 * 1024];          // スタックがあふれて落ちたときでも書き出せるよう、別のスタックで動かす

    const char* SignalName(int sig) {
        switch (sig) {
        case SIGSEGV: return "メモリの読み間違い（SIGSEGV）";
        case SIGBUS: return "メモリの読み間違い（SIGBUS）";
        case SIGABRT: return "強制終了（SIGABRT）";
        case SIGFPE: return "計算の誤り（SIGFPE）";
        case SIGILL: return "不正な命令（SIGILL）";
        }
        return "不明";
    }

    void OnCrash(int sig, siginfo_t*, void*) {
        // ここでは、異常終了の最中でも安全に使える処理（open・write・close・sigaction）だけを使う
        char report[1024];
        const size_t length = CrashState::FormatReport(report, sizeof(report), SignalName(sig));
        const int fd = open(g_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            size_t written = 0;
            while (written < length) {
                const ssize_t n = write(fd, report + written, length - written);
                if (n <= 0) break;
                written += static_cast<size_t>(n);
            }
            close(fd);
        }
        // もとの仕組みに戻して返る。同じ合図がもう一度起きて、OSがふだんどおり終了させる
        for (int i = 0; i < SignalCount; ++i) {
            if (Signals[i] == sig) {
                sigaction(sig, &g_previous[i], nullptr);
            }
        }
        if (sig == SIGABRT) {
            raise(sig);
        }
    }
}

void Platform::InstallCrashRecorder() {
    const std::string path = Platform::SaveFilePath(CrashFileName);
    std::strncpy(g_path, path.c_str(), sizeof(g_path) - 1);

    stack_t stack = {};
    stack.ss_sp = g_altStack;
    stack.ss_size = sizeof(g_altStack);
    sigaltstack(&stack, nullptr);

    struct sigaction action = {};
    action.sa_sigaction = OnCrash;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    for (int i = 0; i < SignalCount; ++i) {
        sigaction(Signals[i], &action, &g_previous[i]);
    }
}

std::string Platform::ReadCrashReport() {
    std::ifstream in(Platform::SaveFilePath(CrashFileName));
    if (!in) {
        return std::string();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text;
}

void Platform::ClearCrashReport() {
    std::remove(Platform::SaveFilePath(CrashFileName).c_str());
}
