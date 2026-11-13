#pragma once
#include "Platform.h"

class World;

// 組み込み画面。開いている間は時間が止まる。
// 左の列「持っている武装」をタップすると1つ積み、右の列「積んでいる武装」をタップすると「武装の詳しい画面」を開く。
// 詳しい画面では、強化パーツの付け外しと、武装を外す操作をする。一覧は指でなぞる（ドラッグする）とスクロールする
class BuildScreen {
public:
    // 開いたときに呼ぶ（前に開いたときの表示を消す）
    void Open();

    // 入力を受けて、表示の時間を進める。閉じるときは true を返す
    bool Update(World& world, const InputState& input, float dt);

    void Draw(const World& world) const;

    // 戦闘画面の「組み込み」ボタン
    static bool HitOpenButton(int x, int y);
    static void DrawOpenButton();

private:
    // 指を離したときに、押した場所に応じた操作をする（スクロールしていたら何もしない）
    bool HandleTap(World& world, int x, int y);
    void HandleDetailTap(World& world, int x, int y);
    void Deny(int area, int row);
    float MaxScroll(int column, const World& world) const;

    // 押している指の状態
    bool pressing_ = false;
    bool dragging_ = false;    // 一定以上動かしたらスクロールとして扱い、タップにしない
    int pressX_ = 0;
    int pressY_ = 0;
    int lastY_ = 0;
    int dragColumn_ = -1;      // スクロールしている列（0：左、1：右、−1：なし）

    float scroll_[2] = { 0.0f, 0.0f };  // 左右の列のスクロール量（ドット）
    int detailIndex_ = -1;     // 詳しい画面を開いている武装（積んでいる武装の番号。−1 なら閉じている）

    // エネルギーが足りずに断った行を、赤く光らせる
    int deniedArea_ = -1;      // 0：左の列、1：詳しい画面の「付けられる強化パーツ」
    int deniedRow_ = -1;
    float deniedTimer_ = 0.0f;
};
