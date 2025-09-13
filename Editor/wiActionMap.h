#pragma once
#include "wiInput.h"
#include "wiConfig.h"
#include "wiHelper.h"
#include <optional>
#include "wiUnorderedMap.h"

namespace wi
{
    struct HotkeyInfo
    {
        wi::input::BUTTON button;
        bool press = false;
        bool control = false;
        bool shift = false;
        bool alt = false;
    };

    class ActionMap
    {
    public:
        void RegisterAction(const std::string& action_name, const HotkeyInfo& default_hotkey);

        bool CheckInput(const std::string& action_name, bool press = false, bool release = false, bool hold = false);

        std::string GetInputString(const std::string& action_name);

        void LoadFromConfig(const wi::config::Section& config);

    private:
        wi::unordered_map<std::string, HotkeyInfo> actions;
        static const wi::unordered_map<std::string, wi::input::BUTTON> buttonMap;

        std::optional<HotkeyInfo> ParseHotkeyString(const std::string& hotkey);
    };
}
