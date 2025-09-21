#pragma once

#include <Client/Ui/Element.hh>

namespace Ui {
    class LevelBar final : public Element {
    public:
        LerpFloat progress;
        uint32_t level;
        LevelBar();
        virtual void on_render(Renderer &) override;
    };

    class LeaderboardSlot final : public Element {
    public:
        uint8_t pos;
        LerpFloat ratio;
        LeaderboardSlot(uint8_t);

        virtual void on_render(Renderer &) override;
    };

    class Minimap final : public Element {
    public:
        Minimap(float);
        virtual void on_render(Renderer &) override;
    };

    class OverlevelTimer final : public Element {
    public:
        OverlevelTimer(float);
        virtual void on_render(Renderer &) override;
    };

    class BroadcastDisplay final : public Element {
    public:
        BroadcastDisplay();
        virtual void on_render(Renderer&) override;
    };

    Element *make_leaderboard();
    Element *make_level_bar();
    Element *make_minimap();
    Element *make_overlevel_indicator();
    Element *make_broadcast_display();
}