#pragma once

class World;

// 組み込み画面。開いている間は時間が止まり、持っている武装を積んだり、積んでいる武装を外したりできる。
// 左の列「持っている武装」をタップすると1つ積み、右の列「積んでいる武装」をタップすると外す
class BuildScreen {
public:
    // 開いたときに呼ぶ（前に開いたときの表示を消す）
    void Open();

    // 入力を受けて、表示の時間を進める。pressed はこのフレームで押した瞬間か。閉じるときは true を返す
    bool Update(World& world, bool pressed, int x, int y, float dt);

    void Draw(const World& world) const;

    // 戦闘画面の「組み込み」ボタン
    static bool HitOpenButton(int x, int y);
    static void DrawOpenButton();

private:
    int deniedRow_ = -1;       // エネルギーが足りずに積めなかった行（−1 ならなし）
    float deniedTimer_ = 0.0f; // その行を赤く光らせる残り時間（秒）
};
