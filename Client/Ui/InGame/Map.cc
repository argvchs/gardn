#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Game.hh>

#include <Shared/Map.hh>

using namespace Ui;

Minimap::Minimap(float w) : Element(w, w* ARENA_HEIGHT / ARENA_WIDTH, {}) {}

void Minimap::on_render(Renderer& ctx) {
    // 绘制地图边框
    ctx.set_line_width(7);
    ctx.set_stroke(0xff444444);
    ctx.stroke_rect(-width / 2, -height / 2, width, height);

    ctx.translate(-width / 2, -height / 2);
    ctx.scale(width / ARENA_WIDTH);

    // 绘制地图区域
    for (ZoneDefinition const& def : MAP_DATA) {
        ctx.set_fill(def.color);
        ctx.fill_rect(def.left, def.top, def.right - def.left, def.bottom - def.top);
    }

    if (!Game::in_game()) return;

    Simulation& sim = Game::simulation;
    Entity const& camera = sim.get_ent(Game::camera_id);

    // 绘制靶子
    sim.for_each<kMob>([&](Simulation*, Entity const& ent) {
        if (ent.get_mob_id() == MobID::kTargetDummy) {
            uint32_t color = FLOWER_COLORS[ColorID::kRed];
            ctx.set_fill(color);
            ctx.set_stroke(Renderer::HSV(color, 0.8));
            ctx.set_line_width(ARENA_WIDTH / 120);
            ctx.begin_path();
            ctx.arc(ent.get_x(), ent.get_y(), ARENA_WIDTH / 60);
            ctx.fill();
            ctx.stroke();
        }
        });

    sim.for_each<kFlower>([&](Simulation*, Entity const& ent) {
        if (ent.id != camera.get_player() && ent.get_team() == sim.get_ent(camera.get_player()).get_team()) {
            uint32_t color = FLOWER_COLORS[ent.get_color()];
            ctx.set_fill(color);
            ctx.set_stroke(Renderer::HSV(color, 0.8));
            ctx.set_line_width(ARENA_WIDTH / 120);
            ctx.begin_path();
            ctx.arc(ent.get_x(), ent.get_y(), ARENA_WIDTH / 60);
            ctx.fill();
            ctx.stroke();
        }
        });

    // 绘制摄像机自己在最前面
    ctx.set_fill(0xffffe763);
    ctx.set_stroke(Renderer::HSV(0xffffe763, 0.8));
    ctx.set_line_width(ARENA_WIDTH / 120);
    ctx.begin_path();
    ctx.arc(camera.get_camera_x(), camera.get_camera_y(), ARENA_WIDTH / 60);
    ctx.fill();
    ctx.stroke();
}


Element* Ui::make_minimap() {
    Element* elt = new Ui::VContainer({
        new Ui::StaticText(20, "Minimap"),
        new Ui::Minimap(300)
        }, 20, 10, {
            .should_render = []() { return Game::should_render_game_ui(); },
            .h_justify = Style::Right,
            .v_justify = Style::Bottom
        });
    elt->y = -50;
    return elt;
}