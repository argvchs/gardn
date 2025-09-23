#include <Client/Ui/InGame/Loadout.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Game.hh>
#include <Client/StaticData.hh>

#include <format>

using namespace Ui;

Element *Ui::UiLoadout::petal_tooltips[PetalID::kNumPetals] = {nullptr};

static void make_petal_tooltip(PetalID::T id) {
    PetalData const& d = PETAL_DATA[id];
    PetalAttributes const& a = d.attributes;
    std::string rld_str = d.reload == 0 ? "" :
        a.secondary_reload == 0 ? std::format("{:g}s ⟳", d.reload) :
        std::format("{:g}s + {:g}s ⟳", d.reload, a.secondary_reload);
    //std::cout << Renderer::get_ascii_text_size(rld_str.c_str()) << '\n';
    Element *tooltip = new Ui::VContainer({
        new Ui::HFlexContainer(
            new Ui::StaticText(20, PETAL_DATA[id].name, { .fill = 0xffffffff, .h_justify = Style::Left }),
            new Ui::HContainer({
                new Ui::StaticText(16, rld_str, {.fill = 0xffffffff }),
                a.unstackable ? new Ui::StaticText(16, "⮻", {.fill = 0xffffffff }) : nullptr
                }, 0, 0, {.h_justify = Style::Right }),
            5, 10, {}
        ),
        new Ui::StaticText(14, RARITY_NAMES[PETAL_DATA[id].rarity], { .fill = RARITY_COLORS[PETAL_DATA[id].rarity], .h_justify = Style::Left }),
        new Ui::Element(0,10),
        new Ui::StaticText(12, PETAL_DATA[id].description, { .fill = 0xffffffff, .h_justify = Style::Left }),
        (d.health || d.damage) ? new Ui::Element(0,10) : nullptr,
        d.health ? new Ui::HContainer({
            new Ui::StaticText(12, "Health: ", { .fill = 0xff66ff66, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}", d.health), { .fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, { .h_justify = Style::Left }) : nullptr,
        d.damage ? new Ui::HContainer({
            new Ui::StaticText(12, "Damage: ", { .fill = 0xffff6666, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}", d.damage), { .fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, { .h_justify = Style::Left }) : nullptr,
        a.constant_heal ? new Ui::HContainer({
            new Ui::StaticText(12, "Heal: ", { .fill = 0xffff94c9, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}/s", a.constant_heal), { .fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, { .h_justify = Style::Left }) : nullptr,
        a.burst_heal ? new Ui::HContainer({
            new Ui::StaticText(12, "Heal: ", { .fill = 0xffff94c9, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}", a.burst_heal), { .fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, { .h_justify = Style::Left }) : nullptr,
        a.poison_damage.damage ? new Ui::HContainer({
            new Ui::StaticText(12, "Poison: ", { .fill = 0xffce76db, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}", a.poison_damage.damage * a.poison_damage.time) + (a.poison_damage.time ? std::format(" ({:.2g}/s)", a.poison_damage.damage ) : ""), { .fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, { .h_justify = Style::Left }) : nullptr,
        a.spawns != MobID::kNumMobs ? new Ui::HContainer({
            new Ui::StaticText(12, "Contents: ", { .fill = 0xffd2eb34, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::string((a.spawn_count ? a.spawn_count : 1) * (d.count ? d.count : 1) > 1 ? std::to_string((a.spawn_count ? a.spawn_count : 1) * (d.count ? d.count : 1)) + "x " : "") + MOB_DATA[a.spawns].name + " (", { .fill = 0xffffffff, .h_justify = Style::Left }),
            new Ui::StaticText(12, RARITY_NAMES[MOB_DATA[a.spawns].rarity], { .fill = RARITY_COLORS[MOB_DATA[a.spawns].rarity], .h_justify = Style::Left }),
            new Ui::StaticText(12, ")", { .fill = 0xffffffff, .h_justify = Style::Left }),
        }, 0, 0, { .h_justify = Style::Left }) : nullptr,
        a.extra_health ? new Ui::HContainer({
            new Ui::StaticText(12, "Flower Health: ", {.fill = 0xff66ff66, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("+{:g}", a.extra_health), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.movement_speed ? new Ui::HContainer({
            new Ui::StaticText(12, "Movement Speed: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("+{:g}%",  a.movement_speed * 100), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.reduce_reload ? new Ui::HContainer({
            new Ui::StaticText(12, "Reload: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}%", (a.reduce_reload - 1) * 100), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.extra_range ? new Ui::HContainer({
            new Ui::StaticText(12, "Attack Range: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("+{:g}", a.extra_range ), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.extra_vision ? new Ui::HContainer({
            new Ui::StaticText(12, "Extra Vision: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}%", 1 / (1 - a.extra_vision) * 100), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.armor ? new Ui::HContainer({
            new Ui::StaticText(12, "Armor: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}", a.armor), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.controls ? new Ui::HContainer({
            new Ui::StaticText(12, "Controls: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, PETAL_DATA[a.controls].name, {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.non_removable ? new Ui::HContainer({
            new Ui::StaticText(12, "Cannot be unequipped.", {.fill = 0xffcde23b, .h_justify = Style::Left }),
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.extra_body_damage ? new Ui::HContainer({
            new Ui::StaticText(12, "Body Damage: ", {.fill = 0xffff6666, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("+{:g}", a.extra_body_damage), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.extra_radius ? new Ui::HContainer({
            new Ui::StaticText(12, "Flower Radius: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("+{:g}", a.extra_radius), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.extra_rot ? new Ui::HContainer({
            new Ui::StaticText(12, "Rotation Speed: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("+{:g} rad/s", a.extra_rot), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.poison_armor ? new Ui::HContainer({
            new Ui::StaticText(12, "Poison Resistance: ", {.fill = 0xffce76db, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g} /s",a.poison_armor), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.damage_reflection ? new Ui::HContainer({
            new Ui::StaticText(12, "Damage Reflection: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g}%", a.damage_reflection * 100), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
        a.slow_inflict ? new Ui::HContainer({
            new Ui::StaticText(12, "Duration: ", {.fill = 0xffcde23b, .h_justify = Style::Left }),
            new Ui::StaticText(12, std::format("{:g} s", a.slow_inflict), {.fill = 0xffffffff, .h_justify = Style::Left })
        }, 0, 0, {.h_justify = Style::Left }) : nullptr,
    }, 5, 2);
    tooltip->style.fill = 0x80000000;
    tooltip->style.round_radius = 6;
    tooltip->refactor();
    Ui::UiLoadout::petal_tooltips[id] = tooltip;
}

void Ui::make_petal_tooltips() {
    for (PetalID::T i = 0; i < PetalID::kNumPetals; ++i)
        make_petal_tooltip(i);
}