#pragma once

class GameInstance;

class AsymmetricBattle {
public:
    explicit AsymmetricBattle(GameInstance* game);
    void update(); // ���ÿ֡����

private:
    struct AsymmetricBattleInternal;
    AsymmetricBattleInternal* internal; // ʵ����Աָ��
};
