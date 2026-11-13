#pragma once

class World;
struct Enemy;

// 敵1体の動きと射撃を、dt 秒だけ進める（速さを決めて位置を動かし、撃つ時間なら弾を出す）
void UpdateEnemy(Enemy& enemy, World& world, float dt);
