#include <Server/Client.hh>

#include <Server/Game.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>

#include <Helpers/UTF8.hh>
#include <Server/picosha2.h>
#include <Shared/Binary.hh>
#include <Shared/Config.hh>
#include <sstream>
#include <array>
#include <iostream>
#include <cmath>

#define VALIDATE(expr) if (!expr) { client->disconnect(); return; }

constexpr std::array<uint32_t, RarityID::kNumRarities> RARITY_TO_XP = { 2, 10, 50, 200, 1000, 5000, 0 };

float mouse_world_x = 0;
float mouse_world_y = 0;

Client::Client() : game(nullptr) {}

void Client::init() {
    DEBUG_ONLY(assert(game == nullptr);)
    Server::game.add_client(this);    
}

void Client::remove() {
    if (game == nullptr) return;
    game->remove_client(this);
}

void Client::disconnect(int reason, std::string const &message) {
    if (ws == nullptr) return;
    remove();
    ws->end(reason, message);
}

uint8_t Client::alive() {
    if (game == nullptr) return false;
    Simulation *simulation = &game->simulation;
    return simulation->ent_exists(camera) 
    && simulation->ent_exists(simulation->get_ent(camera).get_player());
}

void Client::on_message(WebSocket *ws, std::string_view message, uint64_t code) {
    if (ws == nullptr) return;
    uint8_t const *data = reinterpret_cast<uint8_t const *>(message.data());
    Reader reader(data);
    Validator validator(data, data + message.size());
    Client *client = ws->getUserData();
    if (client == nullptr) {
        ws->end(CloseReason::kServer, "Server Error");
        return;
    }
    if (!client->verified) {
        if (client->check_invalid(validator.validate_uint8() && validator.validate_uint64())) return;
        if (reader.read<uint8_t>() != Serverbound::kVerify) {
            client->disconnect();
            return;
        }
        if (reader.read<uint64_t>() != VERSION_HASH) {
            client->disconnect(CloseReason::kOutdated, "Outdated Version");
            return;
        }
        client->verified = 1;
        client->init();
        return;
    }
    if (client->game == nullptr) {
        client->disconnect();
        return;
    }
    if (client->check_invalid(validator.validate_uint8())) return;
    switch (reader.read<uint8_t>()) {
        case Serverbound::kVerify:
            client->disconnect();
            return;
        case Serverbound::kClientInput: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            if (client->check_invalid(
                validator.validate_float() &&
                validator.validate_float() &&
                validator.validate_uint8()
            )) return;
            float x = reader.read<float>();
            float y = reader.read<float>();
            if (x == 0 && y == 0) player.acceleration.set(0,0);
            else {
                if (std::abs(x) > 5e3 || std::abs(y) > 5e3) break;
                Vector accel(x,y);
                float m = accel.magnitude();
                if (m > 200) accel.set_magnitude(PLAYER_ACCELERATION);
                else accel.set_magnitude(m / 200 * PLAYER_ACCELERATION);
                player.acceleration = accel;
            }
            mouse_world_x = player.get_x() + x / camera.get_fov();
            mouse_world_y = player.get_y() + y / camera.get_fov();

            // 遍历玩家装备的花瓣
            for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
                LoadoutSlot const& slot = player.loadout[i];
                PetalID::T slot_petal_id = slot.get_petal_id();
                struct PetalData const& petal_data = PETAL_DATA[slot_petal_id];
                if (petal_data.attributes.controls != PetalID::kNone) {
                    PetalID::T controlled_id = petal_data.attributes.controls;

                    simulation->for_each_entity([&](Simulation* sim2, Entity& ent) {
                        if (ent.get_parent() != player.id) return;          // 必须是该玩家的
                        if (ent.get_petal_id() != controlled_id) return;    // 必须是被控制的花瓣类型

                        // ==== 检查与其他同类实体的重叠 ====
                        sim2->for_each_entity([&](Simulation* sim3, Entity& other) {
                            if (&other == &ent) return;               // 跳过自己
                            if (other.get_parent() != player.id) return;    // 只管本玩家的
                            if (other.get_petal_id() != controlled_id) return;

                            float dx = ent.get_x() - other.get_x();
                            float dy = ent.get_y() - other.get_y();
                            float dist2 = dx * dx + dy * dy;
                            float min_dist = ent.get_radius() + other.get_radius();

                            if (dist2 < min_dist * min_dist) {
                                float dist = std::sqrt(dist2);
                                if (dist < 0.0001f) dist = 0.0001f; // 防止除零

                                // 计算分离向量
                                float overlap = 0.5f * (min_dist - dist);
                                float nx = dx / dist;
                                float ny = dy / dist;

                                // 推开双方
                                ent.set_x(ent.get_x() + nx * overlap * 4);
                                ent.set_y(ent.get_y() + ny * overlap * 4);
                                other.set_x(other.get_x() - nx * overlap * 4);
                                other.set_y(other.get_y() - ny * overlap * 4);
                            }
                            });

                        // ==== 控制朝向 ====
                        Vector aim(mouse_world_x - ent.get_x(), mouse_world_y - ent.get_y());

                        ent.set_angle(aim.angle());
                        if (BitMath::at(player.input, InputFlags::kDefending)) {
                            ent.set_angle(aim.angle() + M_PI);
                        }
                    });
                }
            }

            player.input = reader.read<uint8_t>();
            break;
        }
        case Serverbound::kClientSpawn: {
            if (client->alive()) break;
            //check string length
            std::string name;
            if (client->check_invalid(validator.validate_string(MAX_NAME_LENGTH))) return;
            reader.read<std::string>(name);
            if (client->check_invalid(UTF8Parser::is_valid_utf8(name))) return;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = alloc_player(simulation, camera.get_team());
            player_spawn(simulation, camera, player);
            player.set_name(name);
            std::string password;
            VALIDATE(validator.validate_string(MAX_PASSWORD_LENGTH));
            reader.read<std::string>(password);
            VALIDATE(UTF8Parser::is_valid_utf8(password));
            client->isAdmin = picosha2::hash256_hex_string(password) == PASSWORD;
       
            break;
        }
        case Serverbound::kPetalDelete: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            if (client->check_invalid(validator.validate_uint8())) return;
            uint8_t pos = reader.read<uint8_t>();
            if (pos >= MAX_SLOT_COUNT + player.get_loadout_count()) break;
            PetalID::T old_id = player.get_loadout_ids(pos);
            if (PETAL_DATA[old_id].attributes.non_removable) return;
            if (old_id != PetalID::kNone && old_id != PetalID::kBasic) {
                uint8_t rarity = PETAL_DATA[old_id].rarity;
                player.set_score(player.get_score() + RARITY_TO_XP[rarity]);
                //need to delete if over cap
                if (player.deleted_petals.size() == player.deleted_petals.capacity())
                    //removes old trashed old petal
                    PetalTracker::remove_petal(simulation, player.deleted_petals[0]);
                player.deleted_petals.push_back(old_id);
            }
            player.set_loadout_ids(pos, PetalID::kNone);
            break;
        }
        case Serverbound::kPetalSwap: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            if (client->check_invalid(validator.validate_uint8() && validator.validate_uint8())) return;
            uint8_t pos1 = reader.read<uint8_t>();
            if (pos1 >= MAX_SLOT_COUNT + player.get_loadout_count()) break;
            uint8_t pos2 = reader.read<uint8_t>();
            if (pos2 >= MAX_SLOT_COUNT + player.get_loadout_count()) break;
            PetalID::T tmp1 = player.get_loadout_ids(pos1);
            PetalID::T tmp2 = player.get_loadout_ids(pos2);
            if ((PETAL_DATA[tmp1].attributes.non_removable && pos2 >= player.get_loadout_count() && pos1 < player.get_loadout_count()) ||
                (PETAL_DATA[tmp2].attributes.non_removable && pos1 >= player.get_loadout_count() && pos2 < player.get_loadout_count()))
                return;

            player.set_loadout_ids(pos1, tmp2);
            player.set_loadout_ids(pos2, tmp1);
            break;
        }
        case Serverbound::kChatSend: {
            if (!client->alive()) break;
            Simulation* simulation = &client->game->simulation;
            Entity& camera = simulation->get_ent(client->camera);
            Entity& player = simulation->get_ent(camera.get_player());
            std::string text;
            VALIDATE(validator.validate_string(MAX_CHAT_LENGTH));
            reader.read<std::string>(text);
            VALIDATE(UTF8Parser::is_valid_utf8(text));
            text = UTF8Parser::trunc_string(text, MAX_CHAT_LENGTH);
            if (text.empty()) break;

            // 命令处理：以 '/' 开头的不进行转发
            if (!text.empty() && text[0] == '/') {
                command(client, text.substr(1), mouse_world_x, mouse_world_y);
                break; // 不放入广播
            }

            // 直接广播，不用维护队列
            client->game->chat(player.id, text);
            break;
        }

    }
}

void Client::command(Client* client, std::string const& text, float mouse_x, float mouse_y) {
    Simulation* simulation = &client->game->simulation;
    Entity& camera = simulation->get_ent(client->camera);
    Entity& player = simulation->get_ent(camera.get_player());
    float x = mouse_x;
    float y = mouse_y;

    std::istringstream iss(text);
    std::string command, arg;
    iss >> command;
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "kill") {
        simulation->get_ent(player.get_parent()).set_killed_by(player.get_name());
        simulation->request_delete(player.id);
    }

    if (!client->isAdmin) return;

    if (command == "drop" || command == "give") {
        PetalID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= PetalID::kNumPetals) continue;
            Entity& drop = alloc_drop(simulation, id);
            drop.set_x(player.get_x()), drop.set_y(player.get_y());
        }
    }
    else if (command == "dropto") {
        PetalID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= PetalID::kNumPetals) continue;
            Entity& drop = alloc_drop(simulation, id);
            drop.set_x(x), drop.set_y(y);
        }
    }
    else if (command == "tp") {
        try { iss >> arg, x = std::stof(arg), iss >> arg, y = std::stof(arg); }
        catch (const std::invalid_argument&) { return; }
        catch (const std::out_of_range&) { return; }
        player.set_x(x), player.set_y(y);
    }
    else if (command == "tpto") {
        player.set_x(x), player.set_y(y);
    }
    else if (command == "xp") {
        uint32_t xp;
        try { iss >> arg, xp = uint32_t(std::stoul(arg)); }
        catch (const std::invalid_argument&) { return; }
        catch (const std::out_of_range&) { return; }
        player.set_score(player.get_score() + xp);
    }
    else if (command == "setxp")
    {
        uint32_t xp;
        try { iss >> arg, xp = uint32_t(std::stoul(arg)); }
        catch (const std::invalid_argument&) { return; }
        catch (const std::out_of_range&) { return; }
        player.set_score(xp);
    }
    else if (command == "heal")
    {
        player.health = player.max_health;
        player.set_health_ratio(1);
    }
    else if (command == "spawn"){
        MobID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= MobID::kNumMobs) continue;
            alloc_mob(simulation, id, player.get_x(), player.get_y(), NULL_ENTITY);
        }
    }
    else if (command == "spawnto") {
        MobID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= MobID::kNumMobs) continue;
            alloc_mob(simulation, id, x, y, NULL_ENTITY);
        }
    }
    else if (command == "spawnally") {
        MobID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= MobID::kNumMobs) continue;
            alloc_mob(simulation, id, player.get_x(), player.get_y(), player.get_team());
        }
    }
    else if (command == "spawnallyto") {
        MobID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= MobID::kNumMobs) continue;
            alloc_mob(simulation, id, x, y, player.get_team());
        }
    }
    else if (command == "spawnenemy") {
        MobID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= MobID::kNumMobs) continue;
            alloc_mob(simulation, id, player.get_x(), player.get_y(), player.id);
        }
    }
    else if (command == "spawnenemyto") {
        MobID::T id;
        while (iss >> arg) {
            try { id = std::stoi(arg); }
            catch (const std::invalid_argument&) { continue; }
            catch (const std::out_of_range&) { continue; }
            if (id >= MobID::kNumMobs) continue;
            alloc_mob(simulation, id, x, y, player.id);
        }
    }
    else if (command == "killallmobs") {
        for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
            EntityID id(i, 0);
           // if (!simulation->ent_alive(id)) continue;
            Entity& ent = simulation->get_ent(id);
            if (ent.has_component(kMob)) {
                ent.health = 0;
            }
        }
    }
    else  if (command == "broadcast") {
        std::string text;
        std::getline(iss, text);  // 从输入流读取剩余整行文本
        if (!text.empty()) {
            Server::game.broadcast_message(text);  // 调用服务器广播函数
        }
    }
    else if (command == "god") {
        if (!player.immunity_ticks) {
            player.immunity_ticks = 99999 * TPS;
        }
        else {
            player.immunity_ticks = 0;
        }
    }

}

void Client::on_disconnect(WebSocket *ws, int code, std::string_view message) {
    std::printf("disconnect: [%d]\n", code);
    Client *client = ws->getUserData();
    if (client == nullptr) return;
    client->remove();
}

bool Client::check_invalid(bool valid) {
    if (valid) return false;
    std::cout << "client sent an invalid packet\n";
    //optional
    disconnect();

    return true;
}