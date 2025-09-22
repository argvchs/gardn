#include <Client/Ui/InGame/GameInfo.hh>
#include <Client/DOM.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/Render/Renderer.hh>
#include <Shared/Config.hh>

#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>

using namespace Ui;

namespace {
    struct ChatDisplayEntry {
        EntityID id;           // 发送者实体ID
        std::string sender;    // 名字
        std::string text;      // 消息
        double timestamp;      // 本地时间
        uint32_t color;        // 渲染颜色
    };

    std::deque<ChatDisplayEntry> chat_history;
    int chat_scroll_offset = 0;
    bool is_dragging_scrollbar = false;

    constexpr size_t MAX_HISTORY = 200;
    constexpr size_t MAX_VISIBLE_LINES = 12;
    constexpr double RECENT_SEC = 10.0;
    constexpr float LINE_HEIGHT = 18.0f;
    constexpr float LEFT_PADDING = 6.0f;
    constexpr float RIGHT_PADDING = 6.0f;
    constexpr float SCROLLBAR_WIDTH = 10.0f;
    constexpr float CHAT_AREA_WIDTH = 600.0f;
    constexpr float CHAT_AREA_HEIGHT = LINE_HEIGHT * MAX_VISIBLE_LINES + 8.0f;
    constexpr size_t MAX_BUBBLES_PER_ENTITY = 5;
}

static double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// 从服务器或simulation收集消息
static void collect_chats_from_server() {
    for (const auto& msg : Game::chats) {
        // 获取发送者实体
        EntityID sender_id = msg.sender;
        std::string sender_name = "Unknown";
        uint32_t sender_color = 0xffffffff; // 默认白色
        if (Game::simulation.ent_exists(sender_id)) {
            Entity const& e = Game::simulation.get_ent(sender_id);
            sender_name = e.get_name().empty() ? "Unnamed" : e.get_name();
            sender_color = FLOWER_COLORS[e.get_color()];
        }

        chat_history.push_back({ sender_id, sender_name, msg.text, now_seconds(), sender_color });
        if (chat_history.size() > MAX_HISTORY) chat_history.pop_front();
        if (Game::show_chat) chat_scroll_offset = 0;
    }
    Game::chats.clear();
}



Chat::Chat(std::string& r, float w, float h, uint32_t m, Style s)
    : TextInput(r, w, h, m, s) {
}

static void render_chat_bubbles_all_screen(Renderer& renderer, const std::deque<ChatDisplayEntry>& chat_history)
{
    if (!Game::simulation.ent_exists(Game::camera_id)) return;
    const Entity& camera = Game::simulation.get_ent(Game::camera_id);
    float scale = Ui::scale * camera.get_fov();
    double now = now_seconds();

    // 找出所有出现过的实体 ID
    std::vector<EntityID> entity_ids;
    for (const auto& ce : chat_history) {
        if (!Game::simulation.ent_exists(ce.id)) continue;
        if (std::find(entity_ids.begin(), entity_ids.end(), ce.id) == entity_ids.end()) {
            entity_ids.push_back(ce.id);
        }
    }

    // 遍历每个实体渲染泡泡
    for (EntityID ent_id : entity_ids) {
        if (!Game::simulation.ent_exists(ent_id)) continue;
        const Entity& ent = Game::simulation.get_ent(ent_id);

        // 收集该实体仍在显示时间内的泡泡
        std::vector<const ChatDisplayEntry*> bubbles;
        for (const auto& ce : chat_history) {
            if (ce.id == ent_id && now - ce.timestamp <= RECENT_SEC) {
                bubbles.push_back(&ce);
            }
        }

        // 保留最新 5 个
        if (bubbles.size() > 5) {
            bubbles.erase(bubbles.begin(), bubbles.begin() + (bubbles.size() - 5));
        }

        // 倒序绘制（最新在上）
        float offset_y = 0.0f;
        for (auto it = bubbles.rbegin(); it != bubbles.rend(); ++it) {
            const ChatDisplayEntry* ce = *it;

            // 世界坐标 → 屏幕坐标
            float sx = (ent.get_x() - camera.get_camera_x()) * scale + renderer.width / 2.0f;
            float sy = (ent.get_y() - camera.get_camera_y()) * scale + renderer.height / 2.0f;

            // 气泡位置
            sy -= ent.get_radius() * scale + 25.0f + offset_y;

            RenderContext rc(&renderer); // 自动恢复矩阵
            renderer.reset_transform();
            renderer.translate(sx, sy);

            // 气泡绘制
            renderer.set_global_alpha(1 - ent.deletion_animation);
            renderer.scale(1 + 0.5f * ent.deletion_animation);

            float height = 18.0f;
            renderer.set_text_size(height);
            float width = renderer.get_text_size(ce->text.c_str());

            renderer.set_stroke((0xc0 << 24) | (FLOWER_COLORS[ent.get_color()] & 0xffffff));
            renderer.set_line_width(height + 6);
            renderer.round_line_cap();
            renderer.begin_path();
            renderer.move_to(-width / 2, 0);
            renderer.line_to(width / 2, 0);
            renderer.stroke();

            Renderer::TextArgs args;
            args.size = height;
            args.fill = 0xffffffff;
            renderer.draw_text(ce->text.c_str(), args);

            offset_y += height + 12.0f;
        }
    }
}




// 渲染函数
void Chat::on_render(Renderer& ctx) {
    collect_chats_from_server();

    float half_w = width / 2.0f;
    float half_h = height / 2.0f;
    float chat_half_w = CHAT_AREA_WIDTH / 2.0f;
    float chat_half_h = CHAT_AREA_HEIGHT / 2.0f;

    render_chat_bubbles_all_screen(Game::renderer, chat_history);


    std::vector<size_t> to_show_indices;
    if (Game::show_chat) {
        for (size_t i = 0; i < chat_history.size(); ++i) to_show_indices.push_back(i);
    }
    else {
        double now = now_seconds();
        for (size_t i = 0; i < chat_history.size(); ++i)
            if (now - chat_history[i].timestamp <= RECENT_SEC)
                to_show_indices.push_back(i);

        if (to_show_indices.size() > MAX_VISIBLE_LINES) {
            size_t start = to_show_indices.size() - MAX_VISIBLE_LINES;
            std::vector<size_t> tail(to_show_indices.begin() + start, to_show_indices.end());
            to_show_indices.swap(tail);
        }
    }

    if (!to_show_indices.empty()) {
        RenderContext rc(&ctx);
        float x_offset = chat_half_w - half_w;
        ctx.translate(x_offset, -half_h - chat_half_h - 4.0f);

        if (Game::show_chat) {
            ctx.set_fill(0x88000000);
            ctx.begin_path();
            ctx.rect(-chat_half_w, -chat_half_h, CHAT_AREA_WIDTH, CHAT_AREA_HEIGHT);
            ctx.fill();
        }

        size_t visible_lines_count = (size_t)(CHAT_AREA_HEIGHT / LINE_HEIGHT);
        int max_offset = std::max(0, (int)to_show_indices.size() - (int)visible_lines_count);

        // 滚轮与拖动逻辑
        if (Game::show_chat) {
            int wheel_delta = (int)Input::wheel_delta;
            if (BitMath::at(Input::mouse_buttons_released, Input::LeftMouse))
                is_dragging_scrollbar = false;

            if (is_dragging_scrollbar) {
                DOM::element_focus(this->name.c_str());
                float scroll_height = CHAT_AREA_HEIGHT;
                float bar_height = std::max(20.0f, scroll_height * (scroll_height / (chat_history.size() * LINE_HEIGHT)));
                float scroll_track_height = scroll_height - bar_height;
                float chat_area_center_y = this->screen_y - half_h - chat_half_h - 4.0f;
                float scroll_track_top_y = chat_area_center_y - chat_half_h;
                float mouse_y_in_track = Input::mouse_y - scroll_track_top_y;
                float mouse_ratio = std::clamp(mouse_y_in_track / scroll_track_height, 0.0f, 1.0f);
                chat_scroll_offset = (int)round(max_offset * (1.0f - mouse_ratio));
            }
            else if (wheel_delta != 0) {
                constexpr int SCROLL_STEP = 3;
                int direction = (wheel_delta > 0) ? 1 : -1;
                chat_scroll_offset = std::clamp(chat_scroll_offset - (direction * SCROLL_STEP), 0, max_offset);
            }
        }

        int end_line_idx = (int)to_show_indices.size() - chat_scroll_offset;
        int start_line_idx = std::max(0, end_line_idx - (int)visible_lines_count);

        // 绘制文字
        {
            RenderContext text_clip_rc(&ctx);
            ctx.begin_path();
            ctx.rect(-chat_half_w, -chat_half_h, CHAT_AREA_WIDTH, CHAT_AREA_HEIGHT);
            ctx.clip();

            float bottom_y = chat_half_h - LINE_HEIGHT / 2.0f - 4.0f;
            for (int i = start_line_idx; i < end_line_idx; ++i) {
                int lines_from_bottom = (end_line_idx - 1) - i;
                float current_y = bottom_y - lines_from_bottom * LINE_HEIGHT;
                const ChatDisplayEntry& ce = chat_history[to_show_indices[i]];

                float base_x = -chat_half_w + LEFT_PADDING;

                // 绘制发送者
                {
                    RenderContext text_rc(&ctx);
                    float name_width = ctx.get_ascii_text_size(ce.sender.c_str()) * (LINE_HEIGHT - 2.0f);
                    float name_center_x = base_x + name_width / 2.0f;
                    ctx.translate(name_center_x, current_y);
                    Renderer::TextArgs args;
                    args.fill = ce.color; // 根据team颜色渲染
                    args.stroke = 0xff222222;
                    args.size = LINE_HEIGHT - 2.0f;
                    ctx.draw_text(ce.sender.c_str(), args);
                    base_x += name_width;
                }

                // 绘制冒号
                {
                    RenderContext text_rc(&ctx);
                    float colon_width = ctx.get_ascii_text_size(": ") * (LINE_HEIGHT - 2.0f);
                    float colon_center_x = base_x + colon_width / 2.0f;
                    ctx.translate(colon_center_x, current_y);
                    Renderer::TextArgs args;
                    args.fill = 0xffffffff;
                    args.stroke = 0xff222222;
                    args.size = LINE_HEIGHT - 2.0f;
                    ctx.draw_text(": ", args);
                    base_x += colon_width;
                }

                // 绘制内容
                {
                    RenderContext text_rc(&ctx);
                    float content_width = ctx.get_ascii_text_size(ce.text.c_str()) * (LINE_HEIGHT - 2.0f);
                    float content_center_x = base_x + content_width / 2.0f;
                    ctx.translate(content_center_x, current_y);
                    Renderer::TextArgs args;
                    args.fill = 0xffffffff;
                    args.stroke = 0xff222222;
                    args.size = LINE_HEIGHT - 2.0f;
                    ctx.draw_text(ce.text.c_str(), args);
                }
            }
        }

        // 绘制滚动条
        if (Game::show_chat && max_offset > 0) {
            float scroll_height = CHAT_AREA_HEIGHT;
            float content_height = chat_history.size() * LINE_HEIGHT;
            float bar_height = std::max(20.0f, scroll_height * (scroll_height / content_height));
            float scroll_track_height = scroll_height - bar_height;
            float scroll_ratio = (float)(max_offset - chat_scroll_offset) / (float)max_offset;
            float bar_y = -chat_half_h + scroll_track_height * scroll_ratio;
            float bar_x = chat_half_w + RIGHT_PADDING + SCROLLBAR_WIDTH / 2.0f;

            if (BitMath::at(Input::mouse_buttons_pressed, Input::LeftMouse) && !is_dragging_scrollbar) {
                float chat_area_center_x = this->screen_x + x_offset;
                float chat_area_center_y = this->screen_y - half_h - chat_half_h - 4.0f;
                float bar_screen_x = chat_area_center_x + bar_x;
                float bar_screen_y = chat_area_center_y + bar_y;
                if (Input::mouse_x >= bar_screen_x - SCROLLBAR_WIDTH &&
                    Input::mouse_x <= bar_screen_x + SCROLLBAR_WIDTH &&
                    Input::mouse_y >= bar_screen_y &&
                    Input::mouse_y <= bar_screen_y + bar_height) {
                    is_dragging_scrollbar = true;
                }
            }

            ctx.set_fill(0x44cccccc);
            ctx.begin_path();
            ctx.round_rect(bar_x - SCROLLBAR_WIDTH / 2.0f, -chat_half_h, SCROLLBAR_WIDTH, scroll_height, SCROLLBAR_WIDTH / 2.0f);
            ctx.fill();

            ctx.set_fill(is_dragging_scrollbar ? 0xffffffff : 0xaacccccc);
            ctx.begin_path();
            ctx.round_rect(bar_x - SCROLLBAR_WIDTH / 2.0f, bar_y, SCROLLBAR_WIDTH, bar_height, SCROLLBAR_WIDTH / 2.0f);
            ctx.fill();
        }
    }

    // ChatInput自身
    if (Game::show_chat) {
        TextInput::on_render(ctx);
        if (!is_dragging_scrollbar) DOM::element_focus(this->name.c_str());
        if (animation > 0.99) {
            if (Input::keys_held_this_tick.contains(27)) {
                Game::show_chat = false;
                is_dragging_scrollbar = false;
            }
            else if (Input::keys_held_this_tick.contains('\r') || Input::keys_held_this_tick.contains(13)) {
                if (!ref.empty()) Game::send_chat(ref);
                ref.clear();
                Game::show_chat = false;
                is_dragging_scrollbar = false;
            }
        }
        return;
    }
    // 隐藏输入框
    DOM::element_hide(this->name.c_str());
    ctx.set_fill(0x88000000);
    ctx.begin_path();
    ctx.rect(-half_w, -half_h, width, height);
    ctx.fill();

    RenderContext rc(&ctx);
    std::string prompt_text = "Press [ENTER] or click here to chat";
    Renderer::TextArgs args;
    args.fill = 0xffffffff;
    args.stroke = 0xff222222;
    args.size = 14;
    ctx.draw_text(prompt_text.c_str(), args);

    float screen_x = this->screen_x;
    float screen_y = this->screen_y;
    bool inside = Input::mouse_x >= (screen_x - half_w) && Input::mouse_x <= (screen_x + half_w) &&
        Input::mouse_y >= (screen_y - half_h) && Input::mouse_y <= (screen_y + half_h);
    if (animation > 0.99 && !Game::show_chat) {
        bool click = BitMath::at(Input::mouse_buttons_pressed, Input::LeftMouse) && inside;
        bool enter = Input::keys_held_this_tick.contains('\r');
        if (click || enter) {
            Game::show_chat = true;
            DOM::update_text(this->name.c_str(), ref, max);
            DOM::element_focus(this->name.c_str());
        }
    }
}


void Chat::on_render_skip(Renderer& ctx) {
    if (!Game::alive()) {
        Game::show_chat = false;
        is_dragging_scrollbar = false;
    }
    else if (Input::keys_held_this_tick.contains('\r')) {
        Game::show_chat = true;
        DOM::update_text(name.c_str(), ref, max);
    }
    TextInput::on_render_skip(ctx);
}

Element* Ui::make_chat() {
    Element* elt = new Ui::Chat(Game::chat_text, 300, 25, MAX_CHAT_LENGTH, {
        .should_render = []() { return Game::alive(); }
        });
    elt->style.h_justify = Style::Left;
    elt->style.v_justify = Style::Bottom;
    elt->x = 0;
    elt->y = -170;
    return elt;
}