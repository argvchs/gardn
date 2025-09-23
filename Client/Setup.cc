#include <Client/Setup.hh>

#include <Client/DOM.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/Storage.hh>

#include <unordered_map>

#include <emscripten.h>

static char _get_key_from_code(std::string key) {
    if (key.length() == 1) {
        if (key[0] >= 'a' && key[0] <= 'z') key[0] -= 32; // ×ª´óÐ´
        return key[0];
    }

    if (key == "Enter") return '\r';
    if (key == "Backspace") return 8;
    if (key == "Tab") return 9;
    if (key == "Escape") return 27;
    if (key == "Shift") return '\x10';
    if (key == "Control") return 17;
    if (key == "Alt") return 18;
    if (key == "ArrowUp") return 38;
    if (key == "ArrowDown") return 40;
    if (key == "ArrowLeft") return 37;
    if (key == "ArrowRight") return 39;
    if (key == "Insert") return 45;
    if (key == "Delete") return 46;
    if (key == "Meta") return 91;

    return 0;
}

extern "C" {
    void mouse_event(float x, float y, uint8_t type, uint8_t button) {
        Input::mouse_x = x;
        Input::mouse_y = y;
        if (type == 0) {
            BitMath::set(Input::mouse_buttons_pressed, button);
            BitMath::set(Input::mouse_buttons_state, button);
        }
        else if (type == 2) {
            BitMath::set(Input::mouse_buttons_released, button);
            BitMath::unset(Input::mouse_buttons_state, button);
        }
    }

    void key_event(char *code, uint8_t type) {
        char button = _get_key_from_code(std::string(code));
        if (type == 0) {
            Input::keys_held.insert(button);
            Input::keys_held_this_tick.insert(button);
        }
        else if (type == 1) Input::keys_held.erase(button);
        free(code);
    }

    void touch_event(float x, float y, uint8_t type, uint32_t id) {
        if (type == 0) {
            Input::touches.insert({id, { .id = id, .x = x, .y = y, .dx = 0, .dy = 0, .saturated = 0 }});
        } else if (type == 2) {
            Input::touches.erase(id);
        } else {
            auto iter = Input::touches.find(id);
            if (iter == Input::touches.end()) return;
            Input::Touch &touch = iter->second;
            touch.dx = x - touch.x;
            touch.dy = y - touch.y;
            touch.x = x;
            touch.y = y;
        }
    }

    void wheel_event(float wheel) {
        Input::wheel_delta = wheel;
    }

    void clipboard_event(char *clipboard) {
        Input::clipboard = std::string(clipboard);
        free(clipboard);
    }

    void loop(double d, float width, float height) {
        Game::renderer.width = width;
        Game::renderer.height = height;
        Game::tick(d);
    }

}

int setup_inputs() {
    EM_ASM({
        window.addEventListener("keydown", (e) => {
            //e.preventDefault();
            !e.repeat && _key_event(stringToNewUTF8(e.key), 0);
        });
        window.addEventListener("keyup", (e) => {
            //e.preventDefault();
            !e.repeat && _key_event(stringToNewUTF8(e.key), 1);
        });
        window.addEventListener("mousedown", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio, e.clientY * devicePixelRatio, 0, +!!e.button);
        });
        window.addEventListener("mousemove", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio, e.clientY * devicePixelRatio, 1, +!!e.button);
        });
        window.addEventListener("mouseup", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio, e.clientY * devicePixelRatio, 2, +!!e.button);
        });
        window.addEventListener("touchstart", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 0, t.identifier);
        }, { passive: false });
        window.addEventListener("touchmove", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 1, t.identifier);
        }, { passive: false });
        window.addEventListener("touchend", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 2, t.identifier);
        }, { passive: false });
        window.addEventListener("touchcancel", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 2, t.identifier);
        }, { passive: false });
        window.addEventListener("paste", (e) => {
            try {
                const clip = e.clipboardData.getData("text/plain");
                _clipboard_event(stringToNewUTF8(clip));
            } catch(e) {
            };
        }, { capture: true });
        window.addEventListener("wheel", (e) => {
            _wheel_event(e.deltaY);
        });
    });
    return 0;
}

void main_loop() {
    EM_ASM({
        function loop(time)
        {
            Module.canvas.width = innerWidth * devicePixelRatio;
            Module.canvas.height = innerHeight * devicePixelRatio;
            _loop(time, innerWidth * devicePixelRatio, innerHeight * devicePixelRatio);
            requestAnimationFrame(loop);
        };
        requestAnimationFrame(loop);
    });
}

int setup_canvas() {
    EM_ASM({
        Module.canvas = document.getElementById("canvas");
        Module.canvas.width = innerWidth * devicePixelRatio;
        Module.canvas.height = innerHeight * devicePixelRatio;
        Module.canvas.oncontextmenu = function() { return false; };
        window.onbeforeunload = function(e) { return "Are you sure?"; };
        Module.ctxs = [];
        Module.availableCtxs = [];
        Module.TextDecoder = new TextDecoder('utf8');
    });
    return 0;
}

uint8_t check_mobile() {
    return EM_ASM_INT({
        return /iPhone|iPad|iPod|Android|BlackBerry/i.test(navigator.userAgent);
    });
}