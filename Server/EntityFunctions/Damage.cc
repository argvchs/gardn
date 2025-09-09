#include <Server/EntityFunctions.hh>

#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/Helpers.hh>
#include <iostream> 
#include <cmath>

static bool _yggdrasil_revival_clause(Simulation *sim, Entity &player) {
    for (uint32_t i = 0; i < player.loadout_count; ++i) {
        if (!player.loadout[i].already_spawned) continue;
        if (player.loadout[i].get_petal_id() != PetalID::kYggdrasil) continue;
        player.set_loadout_ids(i, PetalID::kNone);
        return true;
    }
    return false;
}

void inflict_damage(Simulation *sim, EntityID const atk_id, EntityID const def_id, float amt, uint8_t type) {
    {
        Entity& def_ent = sim->get_ent(def_id);
        Entity& atk_ent = sim->get_ent(atk_id);

        // �ж��Ƿ���һ���� TargetDummy
        bool def_is_dummy = def_ent.has_component(kMob) && def_ent.mob_id == MobID::kTargetDummy;
        bool atk_is_dummy = atk_ent.has_component(kMob) && atk_ent.mob_id == MobID::kTargetDummy;

        // ��ȡ��ʵ�壨������ڣ�
        Entity* def_parent = (def_ent.parent != NULL_ENTITY && sim->ent_alive(def_ent.parent)) ? &sim->get_ent(def_ent.parent) : nullptr;
        Entity* atk_parent = (atk_ent.parent != NULL_ENTITY && sim->ent_alive(atk_ent.parent)) ? &sim->get_ent(atk_ent.parent) : nullptr;

        // ���һ���� TargetDummy����һ����������߸������������
        if (def_is_dummy) {
            if (atk_ent.has_component(kMob) || (atk_parent && atk_parent->has_component(kMob)))
                return;
        }
        else if (atk_is_dummy) {
            if (def_ent.has_component(kMob) || (def_parent && def_parent->has_component(kMob)))
                return;
        }

    }
    if (amt <= 0) return;
    if (!sim->ent_alive(def_id)) return;
    Entity &defender = sim->get_ent(def_id);
    if (!defender.has_component(kHealth)) return;
    DEBUG_ONLY(assert(!defender.pending_delete);)
    DEBUG_ONLY(assert(defender.has_component(kHealth));)
    // if defender is in ghost mode, ignore damage
    if (defender.ghost_mode) return;
    if (defender.immunity_ticks > 0) return;
    if (type == DamageType::kContact) amt -= defender.armor;
    else if (type == DamageType::kPoison) amt -= defender.poison_armor;
    if (amt <= 0) return;
    //if (amt <= defender.armor) return;
    float old_health = defender.health;
    defender.set_damaged(1);
    defender.health = fclamp(defender.health - amt, 0, defender.health);  
    float damage_dealt = old_health - defender.health;
    //ant hole spawns
    //floor start, ceil end

    if (defender.has_component(kMob) && defender.mob_id == MobID::kAntHole) {
        uint32_t const num_waves = ANTHOLE_SPAWNS.size() - 1;
        uint32_t start = ceilf((defender.max_health - old_health) / defender.max_health * num_waves);
        uint32_t end = ceilf((defender.max_health - defender.health) / defender.max_health * num_waves);
        if (defender.health <= 0) end = num_waves + 1;
        for (uint32_t i = start; i < end; ++i) {
            for (MobID::T mob_id : ANTHOLE_SPAWNS[i]) {
                Entity& child = alloc_mob(sim, mob_id, defender.x, defender.y, defender.team);
                child.set_parent(defender.id);
                child.target = defender.target;
            }
        }

        /*
        uint32_t start = old_health * num_spawn_waves / defender.max_health;
        uint32_t end = ceilf(defender.health * num_spawn_waves / defender.max_health);
        std::printf("%d,%d,%d\n", start, end, num_spawn_waves);
        for (uint32_t i = start; i + 1 > end; --i) {
            for (MobID::T mob_id : ANTHOLE_SPAWNS[num_spawn_waves - i]) {
                Entity &child = alloc_mob(sim, mob_id, defender.x, defender.y, defender.team);
                child.set_parent(defender.id);
                child.target = defender.target;
            }
        }
            */
    }
    if (defender.has_component(kMob) && defender.mob_id == MobID::kTargetDummy) {
        const float drop_interval = 0.025f;
        uint32_t start = ceilf((defender.max_health - old_health) / (defender.max_health * drop_interval));
        uint32_t end = ceilf((defender.max_health - defender.health) / (defender.max_health * drop_interval));

        for (uint32_t i = start; i < end; ++i) {
            // ����ʷʫ����
            std::vector<uint32_t> epic_indices;
            for (uint32_t idx = 0; idx < PetalID::kNumPetals; ++idx) {
                if (PETAL_DATA[idx].rarity == RarityID::kEpic)
                    epic_indices.push_back(idx);
            }
            if (!epic_indices.empty()) {
                uint32_t chosen_idx = epic_indices[rand() % epic_indices.size()];
                Entity& drop = alloc_drop(sim, chosen_idx);
                float radius = defender.radius + 35;
                float angle = frand() * 2.0f * M_PI;
                float dist = radius + frand() * 35.0f;
                drop.set_x(defender.x + cos(angle) * dist);
                drop.set_y(defender.y + sin(angle) * dist);
            }
            if (frand() < 0.05f) {
                Entity& ygg = alloc_drop(sim, PetalID::kYggdrasil);
                float radius = defender.radius + 35;
                float angle = frand() * 2.0f * M_PI;
                float dist = radius + frand() * 35.0f;
                ygg.set_x(defender.x + cos(angle) * dist);
                ygg.set_y(defender.y + sin(angle) * dist);
            }
            // ׷�ݹ����߸�ʵ��
            Entity* attacker = nullptr;
            if (sim->ent_alive(atk_id)) {
                Entity& atk_ent = sim->get_ent(atk_id);

                // ������� Flower ���и�ʵ�壬������ȡ��ʵ��
                if (!atk_ent.has_component(kFlower) && !(atk_ent.parent == NULL_ENTITY)) {
                    attacker = &sim->get_ent(atk_ent.parent);
                }
                else {
                    // ���幥���߻��߸�ʵ�岻���ڣ�ֱ���� atk_ent
                    attacker = &atk_ent;
                }
            }
            if (attacker) {
                const int missile_count = 12;
                const float circle_radius = attacker->radius + 200.0f; // Բ�뾶 = ��ʵ��뾶 + 200
                float prediction_factor = 7.0f;
                float predicted_x = attacker->x + attacker->velocity.x * prediction_factor;
                float predicted_y = attacker->y + attacker->velocity.y * prediction_factor;
                for (int j = 0; j < missile_count; ++j) {
                    float angle = 2.0f * M_PI * j / missile_count;
                    float x = predicted_x + cos(angle) * circle_radius;
                    float y = predicted_y + sin(angle) * circle_radius;

                    Entity& missile = alloc_petal(sim, PetalID::kMissile, defender);
                    missile.damage = 5;
                    missile.health = missile.max_health = 50;
                    missile.team = defender.team;
                    entity_set_despawn_tick(missile, 3 * TPS);
                    missile.set_x(x);
                    missile.set_y(y);

                    // �����Ƕ�ָ��Բ��
                    Vector v(predicted_x - x, predicted_y - y);
                    missile.set_angle(v.angle());
                }
            }
        }
    }


    if (defender.health == 0 && defender.has_component(kFlower)) {
        if (_yggdrasil_revival_clause(sim, defender)) {
            defender.health = defender.max_health;
            defender.immunity_ticks = 10.0 * TPS;
            const int missile_count = 24;
            const float circle_radius = defender.radius; 

            float center_x = defender.x;
            float center_y = defender.y;

            for (int j = 0; j < missile_count; ++j) {
                float angle = 2.0f * M_PI * j / missile_count;
                float x = center_x + cos(angle) * circle_radius;
                float y = center_y + sin(angle) * circle_radius;

                Entity& missile = alloc_petal(sim, PetalID::kDandelion, defender);
                missile.damage = 0;
                missile.health = missile.max_health = 5000;
                missile.radius = 60;
                missile.mass = 10;
                missile.team = defender.team;
                entity_set_despawn_tick(missile, 3 * TPS);
                missile.set_x(x);
                missile.set_y(y);

                // �����Ƕ�ָ������
                Vector v(center_x - x, center_y - y);
                missile.set_angle(v.angle());
            }
        }
    }


    if (!sim->ent_exists(atk_id)) return;
    Entity &attacker = sim->get_ent(atk_id);

    if (type != DamageType::kReflect && defender.damage_reflection > 0)
        inflict_damage(sim, def_id, attacker.base_entity, damage_dealt * defender.damage_reflection, DamageType::kReflect);
    
    if (!sim->ent_alive(atk_id)) return;

    if (type == DamageType::kContact && defender.poison_ticks < attacker.poison_damage.time * TPS) {
        defender.poison_ticks = attacker.poison_damage.time * TPS;
        defender.poison_inflicted = attacker.poison_damage.damage / TPS;
        defender.poison_dealer = atk_id;
    }

    if (defender.slow_ticks < attacker.slow_inflict)
        defender.slow_ticks = attacker.slow_inflict;
    
    if (defender.has_component(kPetal)) {
        switch (defender.petal_id) {
            case PetalID::kDandelion:
                attacker.dandy_ticks = 10 * TPS;
                break;
            default:
                break;
        }
    }

    if (attacker.has_component(kPetal)) {
        if (!sim->ent_alive(defender.target))
            defender.target = attacker.parent;
        defender.last_damaged_by = attacker.parent;
    } else {
        if (!sim->ent_alive(defender.target))
            defender.target = atk_id;
        defender.last_damaged_by = atk_id;
    }
}

void inflict_heal(Simulation *sim, Entity &ent, float amt) {
    DEBUG_ONLY(assert(ent.has_component(kHealth));)
    if (ent.pending_delete || ent.health <= 0) return;
    if (ent.dandy_ticks > 0) return;
    ent.health = fclamp(ent.health + amt, 0, ent.max_health);
}