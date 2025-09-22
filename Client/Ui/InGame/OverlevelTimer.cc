#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>

#include <Shared/Map.hh>

using namespace Ui;

OverlevelTimer::OverlevelTimer(float w) : Element(w,w,{}) {}

static std::string get_overlevel_direction(float px, float py) {
    uint32_t current_index = Map::get_zone_from_pos(px, py);
    int current_difficulty = MAP_DATA[current_index].difficulty;
    ZoneDefinition const* nearest_higher = nullptr;
    float min_dx = std::numeric_limits<float>::max();

    for (auto const& zone : MAP_DATA) {
        if (zone.difficulty <= current_difficulty) continue;

        float zx = (zone.left + zone.right) * 0.5f;
        float dx = std::abs(zx - px);

        if (dx < min_dx) {
            min_dx = dx;
            nearest_higher = &zone;
        }
    }

    if (!nearest_higher) return ""; // 没有更高难度区域

    float zx = (nearest_higher->left + nearest_higher->right) * 0.5f;
    return zx > px ? "MOVE RIGHT →" : "← MOVE LEFT";
}


void OverlevelTimer::on_render(Renderer &ctx) {
    float ratio = Game::overlevel_timer / (PETAL_DISABLE_DELAY * TPS);
    ctx.set_fill(0x80000000);
    ctx.begin_path();
    ctx.arc(0,0,width/2);
    ctx.fill();
    ctx.set_fill(0xffea7f80);
    ctx.begin_path();
    ctx.move_to(0,0);
    ctx.partial_arc(0,0,width/2*0.8,-M_PI/2,-M_PI/2+2*M_PI*ratio,0);
    ctx.close_path();
    ctx.fill();
}

Element *Ui::make_overlevel_indicator() {
    Element *elt = new Ui::HContainer({
        new Ui::VContainer({
            new Ui::DynamicText(18, []() { return get_overlevel_direction(Game::simulation.get_ent(Game::player_id).get_x(), Game::simulation.get_ent(Game::player_id).get_y()); }, {.fill = 0xffea7f80, .h_justify = Ui::Style::Left }),
            new Ui::StaticText(14, "You are overleveled for this zone", { .fill = 0xffffffff, .h_justify = Style::Left }),
            new Ui::StaticText(14, "You will be unable to attack", { .fill = 0xffffffff, .h_justify = Style::Left }),
        }, 0, 3),
        new OverlevelTimer(50)
    }, 10, 20, {
        .fill = 0x40000000,
        .round_radius = 5,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.set_global_alpha(elt->animation);
        },
        .should_render = [](){
            if (!Game::alive()) return false;
            Entity const &player = Game::simulation.get_ent(Game::player_id);
            return MAP_DATA[Map::get_zone_from_pos(player.get_x(), player.get_y())].difficulty < Map::difficulty_at_level(score_to_level(Game::score))
            && Game::overlevel_timer > 0;
        }
    });
    elt->y = -80;
    return elt;
}