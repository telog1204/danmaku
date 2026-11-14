#pragma once
#include "Platform.h"

class World;

// ゲームオーバーの結果表示。戦闘画面を暗くした上に、結果と「もう一度」「タイトルへ」のボタンを出す
class GameOverScreen {
public:
    enum class Choice {
        None,
        Retry,  // もう一度
        Title,  // タイトルへ
    };

    // ゲームオーバーになったときに呼ぶ。newRecord は新記録かどうか
    void Open(bool newRecord);

    // 入力を受ける。ボタンを押したらその選択を返す（少し待つまでは押せない）
    Choice Update(const World& world, const InputState& input);

    // 戦闘画面の上に重ねて描く
    void Draw(const World& world) const;

private:
    bool newRecord_ = false;
    bool pressing_ = false;
    int pressX_ = 0;
    int pressY_ = 0;
};
