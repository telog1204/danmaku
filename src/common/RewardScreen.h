#pragma once
#include "Platform.h"

class World;

// 強敵を倒したときの、報酬を選ぶ画面。3つの候補をカードで並べ、タップしたものを手に入れる
class RewardScreen {
public:
    // 開いたときに呼ぶ（そのとき押している指は、離すまで操作として扱わない）
    void Open();

    // 入力を受ける。報酬を選んで閉じるときは true を返す
    bool Update(World& world, const InputState& input);

    void Draw(const World& world) const;

private:
    bool pressing_ = false;
    bool ignoreUntilRelease_ = false;
    int pressX_ = 0;
    int pressY_ = 0;
};
