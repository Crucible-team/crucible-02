#include "stdafx.h"
#include "wiActionMap.h"
#include <iostream>


namespace wi
{
    const wi::unordered_map<std::string, wi::input::BUTTON> ActionMap::buttonMap = {
        // ALIASES for hotkeys:
        {"ESC", wi::input::KEYBOARD_BUTTON_ESCAPE},
        {"INSERT", wi::input::KEYBOARD_BUTTON_INSERT},
        {"DELETE", wi::input::KEYBOARD_BUTTON_DELETE},
        {"HOME", wi::input::KEYBOARD_BUTTON_HOME},
        {"PAGEUP", wi::input::KEYBOARD_BUTTON_PAGEUP},
        {"PAGEDOWN", wi::input::KEYBOARD_BUTTON_PAGEDOWN},
        {"MOUSELEFT", wi::input::MOUSE_BUTTON_LEFT},
        {"MOUSERIGHT", wi::input::MOUSE_BUTTON_RIGHT},
        {"F1", wi::input::KEYBOARD_BUTTON_F1},
        {"F2", wi::input::KEYBOARD_BUTTON_F2},
        {"F3", wi::input::KEYBOARD_BUTTON_F3},
        {"F4", wi::input::KEYBOARD_BUTTON_F4},
        {"F5", wi::input::KEYBOARD_BUTTON_F5},
        {"F6", wi::input::KEYBOARD_BUTTON_F6},
        {"F7", wi::input::KEYBOARD_BUTTON_F7},
        {"F8", wi::input::KEYBOARD_BUTTON_F8},
        {"F9", wi::input::KEYBOARD_BUTTON_F9},
        {"F10", wi::input::KEYBOARD_BUTTON_F10},
        {"F11", wi::input::KEYBOARD_BUTTON_F11},
        {"F12", wi::input::KEYBOARD_BUTTON_F12},

        // GAMEPAD:
        {"GAMEPAD_BUTTON_UP", wi::input::GAMEPAD_BUTTON_UP},
        {"GAMEPAD_BUTTON_LEFT", wi::input::GAMEPAD_BUTTON_LEFT},
        {"GAMEPAD_BUTTON_DOWN", wi::input::GAMEPAD_BUTTON_DOWN},
        {"GAMEPAD_BUTTON_RIGHT", wi::input::GAMEPAD_BUTTON_RIGHT},
        {"GAMEPAD_BUTTON_1", wi::input::GAMEPAD_BUTTON_1},
        {"GAMEPAD_BUTTON_2", wi::input::GAMEPAD_BUTTON_2},
        {"GAMEPAD_BUTTON_3", wi::input::GAMEPAD_BUTTON_3},
        {"GAMEPAD_BUTTON_4", wi::input::GAMEPAD_BUTTON_4},
        {"GAMEPAD_BUTTON_5", wi::input::GAMEPAD_BUTTON_5},
        {"GAMEPAD_BUTTON_6", wi::input::GAMEPAD_BUTTON_6},
        {"GAMEPAD_BUTTON_7", wi::input::GAMEPAD_BUTTON_7},
        {"GAMEPAD_BUTTON_8", wi::input::GAMEPAD_BUTTON_8},
        {"GAMEPAD_BUTTON_9", wi::input::GAMEPAD_BUTTON_9},
        {"GAMEPAD_BUTTON_10", wi::input::GAMEPAD_BUTTON_10},
        {"GAMEPAD_BUTTON_11", wi::input::GAMEPAD_BUTTON_11},
        {"GAMEPAD_BUTTON_12", wi::input::GAMEPAD_BUTTON_12},
        {"GAMEPAD_BUTTON_13", wi::input::GAMEPAD_BUTTON_13},
        {"GAMEPAD_BUTTON_14", wi::input::GAMEPAD_BUTTON_14},
        {"GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_UP", wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_UP},
        {"GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_LEFT", wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_LEFT},
        {"GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_DOWN", wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_DOWN},
        {"GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_RIGHT", wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_RIGHT},
        {"GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_UP", wi::input::GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_UP},
        {"GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_LEFT", wi::input::GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_LEFT},
        {"GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_DOWN", wi::input::GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_DOWN},
        {"GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_RIGHT", wi::input::GAMEPAD_ANALOG_THUMBSTICK_R_AS_BUTTON_RIGHT},
        {"GAMEPAD_ANALOG_TRIGGER_L_AS_BUTTON", wi::input::GAMEPAD_ANALOG_TRIGGER_L_AS_BUTTON},
        {"GAMEPAD_ANALOG_TRIGGER_R_AS_BUTTON", wi::input::GAMEPAD_ANALOG_TRIGGER_R_AS_BUTTON},

        // MOUSE:
        {"MOUSE_BUTTON_LEFT", wi::input::MOUSE_BUTTON_LEFT},
        {"MOUSE_BUTTON_RIGHT", wi::input::MOUSE_BUTTON_RIGHT},
        {"MOUSE_BUTTON_MIDDLE", wi::input::MOUSE_BUTTON_MIDDLE},
        {"MOUSE_SCROLL_AS_BUTTON_UP", wi::input::MOUSE_SCROLL_AS_BUTTON_UP},
        {"MOUSE_SCROLL_AS_BUTTON_DOWN", wi::input::MOUSE_SCROLL_AS_BUTTON_DOWN},

        // KEYBOARD:
        {"KEYBOARD_BUTTON_UP", wi::input::KEYBOARD_BUTTON_UP},
        {"KEYBOARD_BUTTON_DOWN", wi::input::KEYBOARD_BUTTON_DOWN},
        {"KEYBOARD_BUTTON_LEFT", wi::input::KEYBOARD_BUTTON_LEFT},
        {"KEYBOARD_BUTTON_RIGHT", wi::input::KEYBOARD_BUTTON_RIGHT},
        {"KEYBOARD_BUTTON_SPACE", wi::input::KEYBOARD_BUTTON_SPACE},
        {"KEYBOARD_BUTTON_RSHIFT", wi::input::KEYBOARD_BUTTON_RSHIFT},
        {"KEYBOARD_BUTTON_LSHIFT", wi::input::KEYBOARD_BUTTON_LSHIFT},
        {"KEYBOARD_BUTTON_F1", wi::input::KEYBOARD_BUTTON_F1},
        {"KEYBOARD_BUTTON_F2", wi::input::KEYBOARD_BUTTON_F2},
        {"KEYBOARD_BUTTON_F3", wi::input::KEYBOARD_BUTTON_F3},
        {"KEYBOARD_BUTTON_F4", wi::input::KEYBOARD_BUTTON_F4},
        {"KEYBOARD_BUTTON_F5", wi::input::KEYBOARD_BUTTON_F5},
        {"KEYBOARD_BUTTON_F6", wi::input::KEYBOARD_BUTTON_F6},
        {"KEYBOARD_BUTTON_F7", wi::input::KEYBOARD_BUTTON_F7},
        {"KEYBOARD_BUTTON_F8", wi::input::KEYBOARD_BUTTON_F8},
        {"KEYBOARD_BUTTON_F9", wi::input::KEYBOARD_BUTTON_F9},
        {"KEYBOARD_BUTTON_F10", wi::input::KEYBOARD_BUTTON_F10},
        {"KEYBOARD_BUTTON_F11", wi::input::KEYBOARD_BUTTON_F11},
        {"KEYBOARD_BUTTON_F12", wi::input::KEYBOARD_BUTTON_F12},
        {"KEYBOARD_BUTTON_ENTER", wi::input::KEYBOARD_BUTTON_ENTER},
        {"KEYBOARD_BUTTON_ESCAPE", wi::input::KEYBOARD_BUTTON_ESCAPE},
        {"KEYBOARD_BUTTON_HOME", wi::input::KEYBOARD_BUTTON_HOME},
        {"KEYBOARD_BUTTON_RCONTROL", wi::input::KEYBOARD_BUTTON_RCONTROL},
        {"KEYBOARD_BUTTON_LCONTROL", wi::input::KEYBOARD_BUTTON_LCONTROL},
        {"KEYBOARD_BUTTON_DELETE", wi::input::KEYBOARD_BUTTON_DELETE},
        {"KEYBOARD_BUTTON_BACKSPACE", wi::input::KEYBOARD_BUTTON_BACKSPACE},
        {"KEYBOARD_BUTTON_PAGEDOWN", wi::input::KEYBOARD_BUTTON_PAGEDOWN},
        {"KEYBOARD_BUTTON_PAGEUP", wi::input::KEYBOARD_BUTTON_PAGEUP},
        {"KEYBOARD_BUTTON_NUMPAD0", wi::input::KEYBOARD_BUTTON_NUMPAD0},
        {"KEYBOARD_BUTTON_NUMPAD1", wi::input::KEYBOARD_BUTTON_NUMPAD1},
        {"KEYBOARD_BUTTON_NUMPAD2", wi::input::KEYBOARD_BUTTON_NUMPAD2},
        {"KEYBOARD_BUTTON_NUMPAD3", wi::input::KEYBOARD_BUTTON_NUMPAD3},
        {"KEYBOARD_BUTTON_NUMPAD4", wi::input::KEYBOARD_BUTTON_NUMPAD4},
        {"KEYBOARD_BUTTON_NUMPAD5", wi::input::KEYBOARD_BUTTON_NUMPAD5},
        {"KEYBOARD_BUTTON_NUMPAD6", wi::input::KEYBOARD_BUTTON_NUMPAD6},
        {"KEYBOARD_BUTTON_NUMPAD7", wi::input::KEYBOARD_BUTTON_NUMPAD7},
        {"KEYBOARD_BUTTON_NUMPAD8", wi::input::KEYBOARD_BUTTON_NUMPAD8},
        {"KEYBOARD_BUTTON_NUMPAD9", wi::input::KEYBOARD_BUTTON_NUMPAD9},
        {"KEYBOARD_BUTTON_MULTIPLY", wi::input::KEYBOARD_BUTTON_MULTIPLY},
        {"KEYBOARD_BUTTON_ADD", wi::input::KEYBOARD_BUTTON_ADD},
        {"KEYBOARD_BUTTON_SEPARATOR", wi::input::KEYBOARD_BUTTON_SEPARATOR},
        {"KEYBOARD_BUTTON_SUBTRACT", wi::input::KEYBOARD_BUTTON_SUBTRACT},
        {"KEYBOARD_BUTTON_DECIMAL", wi::input::KEYBOARD_BUTTON_DECIMAL},
        {"KEYBOARD_BUTTON_DIVIDE", wi::input::KEYBOARD_BUTTON_DIVIDE},
        {"KEYBOARD_BUTTON_TAB", wi::input::KEYBOARD_BUTTON_TAB},
        {"KEYBOARD_BUTTON_TILDE", wi::input::KEYBOARD_BUTTON_TILDE},
        {"KEYBOARD_BUTTON_INSERT", wi::input::KEYBOARD_BUTTON_INSERT},
        {"KEYBOARD_BUTTON_ALT", wi::input::KEYBOARD_BUTTON_ALT},
        {"KEYBOARD_BUTTON_ALTGR", wi::input::KEYBOARD_BUTTON_ALTGR},
    };

    void ActionMap::RegisterAction(const std::string& action_name, const HotkeyInfo& default_hotkey)
    {
        actions[wi::helper::toUpper(action_name)] = default_hotkey;
    }

    bool ActionMap::CheckInput(const std::string& action_name, bool press, bool release, bool hold)
    {
        auto it = actions.find(wi::helper::toUpper(action_name));
        if (it == actions.end())
        {
            return false;
        }
        const HotkeyInfo& hotkey = it->second;
        bool ret = false;
        if (press || hotkey.press)
        {
            ret |= wi::input::Press(hotkey.button);
        }
        else if (release)
        {
            ret |= wi::input::Release(hotkey.button);
        }
        else if (hold)
        {
            ret |= wi::input::Hold(hotkey.button);
        }
        else
        {
            ret |= wi::input::Down(hotkey.button);
        }

        if (hotkey.control)
        {
            ret &= wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) || wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        }
        if (hotkey.alt) {
            ret &= wi::input::Down(wi::input::KEYBOARD_BUTTON_ALT) || wi::input::Down(wi::input::KEYBOARD_BUTTON_ALTGR);
        }
        if (hotkey.shift)
        {
            ret &= wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) || wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
        }
        return ret;
    }

    std::string ActionMap::GetInputString(const std::string& action_name)
    {
        auto it = actions.find(wi::helper::toUpper(action_name));
        if (it == actions.end())
        {
            return "No Action"; // Or some default string
        }
        const HotkeyInfo& hotkey = it->second;
        std::string ret = wi::input::ButtonToString(hotkey.button).text;

        if (hotkey.shift)
        {
            ret = "Shift + " + ret;
        }

        if ((hotkey.alt))
        {
            ret = "Alt + " + ret;
        }

        if (hotkey.control)
        {
            ret = "Ctrl + " + ret;
        }

        return ret;
    }

    std::optional<HotkeyInfo> ActionMap::ParseHotkeyString(const std::string& hotkey)
    {
        HotkeyInfo out{};
        std::string hotkeyUpper = wi::helper::toUpper(hotkey);

		std::istringstream iss(hotkeyUpper);
        std::string token, lastNonMod;
        out.button = wi::input::BUTTON_NONE;
        while (std::getline(iss, token, '+'))
        {
            if (token.empty()) continue;
            // ctrl
            if (token == "CTRL" || token == "CONTROL" || token == "LCTRL" || token == "RCTRL") {
                out.control = true;
                continue;
            }
            // shift
            if (token == "SHIFT" || token == "LSHIFT" || token == "RSHIFT") {
                out.shift = true;
                continue;
            }
            // alt / option
            if (token == "ALT" || token == "OPTION" || token == "LALT" || token == "RALT") {
                out.alt = true;
                continue;
            }

            // Last non-modifier wins
            lastNonMod = token;
        }

        if (lastNonMod.empty())
            return std::nullopt;

        if (auto it = buttonMap.find(lastNonMod); it != buttonMap.end()) {
            out.button = it->second;
        }
        else if (lastNonMod.size() == 1) {
            unsigned char c = static_cast<unsigned char>(lastNonMod[0]);
            if (std::isalnum(c)) {
                out.button = wi::input::BUTTON(c); // single alnum fallback (A–Z, 0–9)
            }
        }

        return out;
    }

    void ActionMap::LoadFromConfig(const wi::config::Section& config)
    {
        for (auto& x : config)
        {
            auto itAction = actions.find(wi::helper::toUpper(x.first));
            if (itAction == actions.end())
                continue;
            if (auto parsed = ParseHotkeyString(x.second))
            {
                HotkeyInfo dst = HotkeyInfo{ parsed->button, actions[wi::helper::toUpper(x.first)].press, parsed->control, parsed->shift, parsed->alt };
                actions[wi::helper::toUpper(x.first)] = dst;
            }
        }
    }
}
