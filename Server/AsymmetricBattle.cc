#include "AsymmetricBattle.hh"
#include <Server/Game.hh>         // GameInstance (Server::game)
#include <Server/EntityFunctions.hh>    // inflict_damage, DamageType
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>           // MobID, ColorID
#include <chrono>
#include <cmath>
#include <string>
#include <unistd.h>
#include <cstdlib>


struct AsymmetricBattle::AsymmetricBattleInternal {
    explicit AsymmetricBattleInternal(GameInstance* game)
        : game_instance(game),
        started(false),
        finished(false),
        restart_message_sent(false),
        countdown_seconds(3600),
        last_broadcast_second(-1),
        winner_color(-1) {
    }



    void tick() {
        using namespace std::chrono;

        // ����Ѿ��������������ɱ��ʤ������Ҳ�����������Ϣ
        if (finished) {
            kill_losers_continuously();
            // �����߼�������Ϸ����10��󲥱�����������
            auto now = steady_clock::now();
            int elapsed_since_finish = static_cast<int>(duration_cast<seconds>(now - finish_time).count());
            if(elapsed_since_finish >= 10) {
                game_instance->broadcast_message("SERVER RESTARTING...");
                throw;
            }
            return;
        }

        // �״���������ʱ
        if (!started) {
            started = true;
            start_time = steady_clock::now();
            last_broadcast_second = countdown_seconds;
        }

        // �ȼ�ⳡ���Ƿ���� TargetDummy���������� -> BLUE ʤ����
        {
            bool dummy_exists = false;
            Simulation& sim = game_instance->simulation;
            for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
                EntityID id(i, 0);
                if (!sim.ent_exists(id)) continue;
                Entity& ent = sim.get_ent(id);
                if (ent.mob_id == MobID::kTargetDummy) {
                    dummy_exists = true;
                    break;
                }
            }
            if (!dummy_exists) {
                // BLUE ʤ��
                finished = true;
                finish_time = steady_clock::now(); // ��¼����ʱ��
                winner_color = static_cast<int>(ColorID::kBlue);
                game_instance->broadcast_message("BLUE HAS WON THE GAME!");
                // ����Ҳ�����ִ��һ�λ�ɱ������ÿ֡Ҳ�����ɱ��
                kill_losers_continuously();
                return;
            }
        }

        // ����ʣ��ʱ�䣨����ϵͳʱ�䣩
        auto now = steady_clock::now();
        int elapsed = static_cast<int>(duration_cast<seconds>(now - start_time).count());
        int remaining = countdown_seconds - elapsed;
        if (remaining <= 0) {
            // RED ʤ��
            finished = true;
            finish_time = steady_clock::now(); // ��¼����ʱ��
            winner_color = static_cast<int>(ColorID::kRed);
            game_instance->broadcast_message("RED HAS WON THE GAME!");
            // ����ִ��һ�λ�ɱ������ÿ֡Ҳ�����ɱ��
            kill_losers_continuously();
            return;
        }

        // ==================== �µĲ����߼���ʼ ====================

        bool should_broadcast = false;

        if (remaining > 60) {
            // ʣ��ʱ�� > 1 ���ӣ�����Ƿ��Խ�� 2 ���ӣ�120�룩�ı߽�
            // ͨ������������ȷ��ʱ�������ġ��顱������鲻ͬ���򲥱�
            if ((remaining / 120) != (last_broadcast_second / 120)) {
                should_broadcast = true;
            }
        }
        else { // remaining <= 60
            // ʣ��ʱ�� <= 1 ����
            // 1. ����ϴβ���ʱʱ�仹�� 60 �����ϣ�˵���Ǹս������һ���ӣ���Ҫ��������
            if (last_broadcast_second > 60) {
                should_broadcast = true;
            }
            // 2. ���֮ǰ�����ڶ��� 60 �����ڣ������Ƿ��Խ�� 10 ��ı߽�
            else {
                // ʹ������ȡ���� 10 ����߼��ж�
                if (((remaining + 9) / 10) != ((last_broadcast_second + 9) / 10)) {
                    should_broadcast = true;
                }
            }
        }

        if (should_broadcast) {
            last_broadcast_second = remaining;
            std::string msg;
            if (remaining > 60) {
                int minutes = static_cast<int>(std::ceil(remaining / 60.0));
                msg = std::to_string(minutes) + " minutes until RED wins!";
            }
            else {
                // �� remaining <= 60 ʱ��ֱ����ʾ����
                msg = std::to_string(remaining) + " seconds until RED wins!";
            }
            game_instance->broadcast_message(msg);
        }
        // ==================== �µĲ����߼����� ====================
    }

    // ������ɱ��ʤ������ң��� finished==true ʱ�����ã�
    void kill_losers_continuously() {
        Simulation& sim = game_instance->simulation;
        if (winner_color < 0) return;

        for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
            EntityID id(i, 0);
            Entity& ent = sim.get_ent(id);
            if (!ent.has_component(kFlower)) continue;
            if (static_cast<int>(ent.color) != winner_color) {
                ent.health = 0; // ֱ�Ӹ� health���� killallmobs ����һ��
            }
        }
    }

    GameInstance* game_instance;
    bool started;
    bool finished;
    bool restart_message_sent; // ����һ����־λ����ֹ�ظ�����
    const int countdown_seconds; // ��������1800��
    int last_broadcast_second;    // �ϴμ�¼�����������ڱ����ظ��㲥��
    int winner_color;            // -1 δ������ ColorID ֵ
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time; // ��������¼��Ϸ����ʱ��
};

// ---------------- AsymmetricBattle ����ӿ� ----------------
AsymmetricBattle::AsymmetricBattle(GameInstance* game)
    : internal(new AsymmetricBattleInternal(game)) {
}

void AsymmetricBattle::update() {
    internal->tick();
}