#pragma once
#include "wiUnorderedMap.h"
class EditorComponent;

class MetadataWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);

	EditorComponent* editor = nullptr;
	wi::ecs::Entity entity;
	void SetEntity(wi::ecs::Entity entity);

    wi::gui::ComboBox presetCombo;
    wi::gui::ComboBox addCombo;

    // Dynamic entity class presets loaded from JSON/files
    wi::vector<std::string> dynamicEntityClasses;
    static constexpr uint64_t USER_PRESET_BASE = 1000; // userdata offset for dynamic items

    struct DynamicPresetDefaults
    {
        wi::unordered_map<std::string, bool> bools;
        wi::unordered_map<std::string, int> ints;
        wi::unordered_map<std::string, float> floats;
        wi::unordered_map<std::string, std::string> strings;
    };
    wi::unordered_map<std::string, DynamicPresetDefaults> dynamicEntityDefaults; // key: class name

	struct Entry
	{
		wi::gui::Button remove;
		wi::gui::TextInputField name;
		wi::gui::TextInputField value;
		wi::gui::CheckBox check;
		bool is_bool = false;
	};
	std::deque<Entry> entries;

    void RefreshEntries();

    void ResizeLayout() override;
};

