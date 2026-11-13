#pragma once
#include <cmath>

// 位置や速さに使う2次元のベクトル
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }

    float Dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float LengthSq() const { return x * x + y * y; }
    float Length() const { return std::sqrt(LengthSq()); }

    // 長さを1にしたもの。長さが0のときは0のまま返す
    Vec2 Normalized() const {
        const float len = Length();
        return len > 0.0f ? Vec2(x / len, y / len) : Vec2();
    }

    // 左へ90度回したもの
    Vec2 Perpendicular() const { return Vec2(-y, x); }

    // 角度（ラジアン）の向きを指す、長さ1のベクトル
    static Vec2 FromAngle(float radians) { return Vec2(std::cos(radians), std::sin(radians)); }
};
