#pragma once

#include <Server/TeamManager.hh>

#include <Shared/Simulation.hh>

#include <set>

class Client;

class GameInstance {
    std::set<Client *> clients;
    TeamManager team_manager;
public:
    Simulation simulation;
    GameInstance();
    void init();
    void tick();
    void add_client(Client *);
    void remove_client(Client *);
    void chat(EntityID sender, std::string const& text);
    void broadcast_message(const std::string& msg);
    TeamManager& get_team_manager() { return team_manager; };
};