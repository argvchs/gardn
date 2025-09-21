#include <Server/Game.hh>

#include <Server/Client.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Map.hh>


static void _update_client(Simulation *sim, Client *client) {
    if (client == nullptr) return;
    if (!client->verified) return;
    if (sim == nullptr) return;
    if (!sim->ent_exists(client->camera)) return;
    std::set<EntityID> in_view;
    std::vector<EntityID> deletes;
    in_view.insert(client->camera);
    Entity &camera = sim->get_ent(client->camera);
    if (sim->ent_exists(camera.player)) 
        in_view.insert(camera.player);
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kClientUpdate);
    writer.write<EntityID>(client->camera);
    sim->spatial_hash.query(camera.camera_x, camera.camera_y, 960 / camera.fov + 50, 540 / camera.fov + 50, [&](Simulation *, Entity &ent){
        in_view.insert(ent.id);
    });
    sim->for_each<kMob>([&](Simulation*, Entity &ent){
    if (ent.mob_id == MobID::kTargetDummy) {
        in_view.insert(ent.id);
    }
});
    sim->for_each<kFlower>([&](Simulation*, Entity& ent) {
        if (ent.id != client->camera && ent.team == camera.team) {
            in_view.insert(ent.id);
        }
        });

    sim->for_each<kChat>([&](Simulation*, Entity& chat_ent) {
        if (!sim->ent_exists(chat_ent.parent)) return;
        Entity const& parent_ent = sim->get_ent(chat_ent.parent);
        if (parent_ent.team == camera.team) {
            in_view.insert(chat_ent.id);
        }
        });


    for (EntityID const &i: client->in_view) {
        if (!in_view.contains(i)) {
            writer.write<EntityID>(i);
            deletes.push_back(i);
        }
    }

    for (EntityID const &i : deletes)
        client->in_view.erase(i);

    writer.write<EntityID>(NULL_ENTITY);
    //upcreates
    for (EntityID id: in_view) {
        DEBUG_ONLY(assert(sim->ent_exists(id));)
        Entity &ent = sim->get_ent(id);
        uint8_t create = !client->in_view.contains(id);
        writer.write<EntityID>(id);
        writer.write<uint8_t>(create | (ent.pending_delete << 1));
        ent.write(&writer, BIT_AT(create, 0));
        client->in_view.insert(id);
    }
    writer.write<EntityID>(NULL_ENTITY);
    //write arena stuff
    writer.write<uint8_t>(client->seen_arena);
    sim->arena_info.write(&writer, client->seen_arena);
    client->seen_arena = 1;
    client->send_packet(writer.packet, writer.at - writer.packet);
}

GameInstance::GameInstance() : simulation(), clients(), team_manager(&simulation) {}

void GameInstance::init() {
    for (uint32_t i = 0; i < ENTITY_CAP / 2; ++i)
        Map::spawn_random_mob(&simulation, frand() * ARENA_WIDTH, frand() * ARENA_HEIGHT);
    #ifdef GAMEMODE_TDM
    team_manager.add_team(ColorID::kBlue);
    team_manager.add_team(ColorID::kRed);
    for (uint32_t i = 0; i < 5; ++i) {
        float x = lerp(MAP_DATA[3].left, MAP_DATA[3].right, (i + 0.5f) / 5.0f);
        float y = lerp(MAP_DATA[3].top, MAP_DATA[3].bottom, frand()); // 纵向仍随机
        Entity& mob = alloc_mob(&simulation, MobID::kTargetDummy, x, y, team_manager.teams[1]);
        mob.set_parent(NULL_ENTITY);
        mob.set_color(simulation.get_ent(team_manager.teams[1]).color);
        mob.base_entity = NULL_ENTITY;
    }

    #endif
}

void GameInstance::tick() {
    simulation.tick();
    for (Client *client : clients)
        _update_client(&simulation, client);
   simulation.post_tick();
 
}

void GameInstance::add_client(Client *client) {
    DEBUG_ONLY(assert(client->game != this);)
    if (client->game != nullptr)
        client->game->remove_client(client);
    client->game = this;
    clients.insert(client);
    Entity &ent = simulation.alloc_ent();
    ent.add_component(kCamera);
    ent.add_component(kRelations);
    #ifdef GAMEMODE_TDM
    EntityID team = team_manager.get_random_team();
    ent.set_team(team);
    ent.set_color(simulation.get_ent(team).color);
    ++simulation.get_ent(team).player_count;
    #else
    ent.set_team(ent.id);
    ent.set_color(ColorID::kYellow); 
    #endif
    
    ent.set_fov(BASE_FOV);
    ent.set_respawn_level(30);

    if (simulation.get_ent(team).color == ColorID::kRed) {
        ent.set_respawn_level(99);

        // 固定顺序的花瓣
        std::vector<PetalID::T> fixed_loadout = {
            PetalID::kAzalea,
            PetalID::kAzalea,
            PetalID::kBubble,
            PetalID::kTringer,
            PetalID::kTringer,
            PetalID::kTringer,
            PetalID::kPoisonPeas2,
            PetalID::kSalt,
            PetalID::kCorruption
        };

        // 填充角色背包
        for (uint32_t i = 0; i < fixed_loadout.size(); ++i) {
            ent.set_inventory(i, fixed_loadout[i]);
        }
    }
    else {
        for (uint32_t i = 0; i < loadout_slots_at_level(ent.respawn_level); ++i)
            ent.set_inventory(i, PetalID::kBasic);
        ent.set_inventory(loadout_slots_at_level(ent.respawn_level), PetalID::kRose);
        ent.set_inventory(loadout_slots_at_level(ent.respawn_level) + 1, PetalID::kBubble);

        if (frand() < 0.0001 && PetalTracker::get_count(&simulation, PetalID::kUniqueBasic) == 0)
            ent.set_inventory(0, PetalID::kUniqueBasic);
    }
        for (uint32_t i = 0; i < loadout_slots_at_level(ent.respawn_level); ++i)
            PetalTracker::add_petal(&simulation, ent.inventory[i]);
   
    client->camera = ent.id;
    client->seen_arena = 0;
}

void GameInstance::remove_client(Client *client) {
    DEBUG_ONLY(assert(client->game == this);)
    clients.erase(client);
    if (simulation.ent_exists(client->camera)) {
        Entity &c = simulation.get_ent(client->camera);
        if (simulation.ent_exists(c.team))
            --simulation.get_ent(c.team).player_count;
        if (simulation.ent_exists(c.player))
            simulation.request_delete(c.player);
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            PetalTracker::remove_petal(&simulation, c.inventory[i]);
        simulation.request_delete(client->camera);
    }
    client->game = nullptr;
}

void GameInstance::broadcast_message(std::string const& msg) {
    for (Client* client : clients) {

        Writer writer(Server::OUTGOING_PACKET);
        writer.write<uint8_t>(Clientbound::kBroadcast); // 新增的枚举类型
        writer.write<std::string>(msg);

        client->send_packet(writer.packet, writer.at - writer.packet);
    }
}