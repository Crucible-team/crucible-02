#include "stdafx.h"
#include "MetadataWindow.h"
#include "json.hpp"
#include "wiHelper.h"

using json = nlohmann::json;

using namespace wi::ecs;
using namespace wi::scene;

void MetadataWindow::Create(EditorComponent* _editor)
{
    editor = _editor;
	wi::gui::Window::Create(ICON_METADATA " Metadata", wi::gui::Window::WindowControls::COLLAPSE | wi::gui::Window::WindowControls::CLOSE | wi::gui::Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL);
	SetSize(XMFLOAT2(300, 240));

	closeButton.SetTooltip("Delete MetadataComponent");
	OnClose([=](wi::gui::EventArgs args) {

		wi::Archive& archive = editor->AdvanceHistory();
		archive << EditorComponent::HISTORYOP_COMPONENT_DATA;
		editor->RecordEntity(archive, entity);

		editor->GetCurrentScene().metadatas.Remove(entity);

		editor->RecordEntity(archive, entity);

		editor->componentsWnd.RefreshEntityTree();
	});

	auto forEachSelected = [this] (auto func) {
		return [this, func] (auto args) {
			wi::scene::Scene& scene = editor->GetCurrentScene();
			for (auto& x : editor->translator.selected)
			{
				MetadataComponent* metadata = scene.metadatas.GetComponent(x.entity);
				if (metadata != nullptr)
				{
					func(metadata, args);
				}
			}
		};
	};

    presetCombo.Create("Preset: ");
    presetCombo.AddItem("Custom", (uint64_t)MetadataComponent::Preset::Custom);
    presetCombo.AddItem("Waypoint", (uint64_t)MetadataComponent::Preset::Waypoint);
    presetCombo.AddItem("Player", (uint64_t)MetadataComponent::Preset::Player);
    presetCombo.AddItem("Enemy", (uint64_t)MetadataComponent::Preset::Enemy);
    presetCombo.AddItem("Npc", (uint64_t)MetadataComponent::Preset::NPC);
    presetCombo.AddItem("Pickup", (uint64_t)MetadataComponent::Preset::Pickup);
    presetCombo.AddItem("Vehicle", (uint64_t)MetadataComponent::Preset::Vehicle);

    // Load dynamic entity class presets from entities/ folder(s) or presets.json
    dynamicEntityClasses.clear();
    dynamicEntityDefaults.clear();
    {
        wi::vector<std::string> candidate_dirs;
        candidate_dirs.push_back(wi::helper::GetCurrentPath() + "/entities/");
        candidate_dirs.push_back(wi::helper::GetCurrentPath() + "/Content/entities/");

        wi::unordered_set<std::string> unique_names;

        auto register_preset = [&](const std::string& name, const DynamicPresetDefaults* defaults) {
            if (name.empty()) return;
            bool newly_added = false;
            if (unique_names.count(name) == 0) {
                unique_names.insert(name);
                dynamicEntityClasses.push_back(name);
                newly_added = true;
            }
            if (defaults != nullptr)
            {
                // store defaults (update if missing or newly added)
                if (newly_added || dynamicEntityDefaults.find(name) == dynamicEntityDefaults.end())
                {
                    dynamicEntityDefaults[name] = *defaults;
                }
            }
        };

        auto parse_defaults = [&](const json& container, DynamicPresetDefaults& out_defaults) {
            const json* md = nullptr;
            if (container.is_object())
            {
                if (container.contains("metadata") && container["metadata"].is_object())
                {
                    md = &container["metadata"];
                }
                else
                {
                    // also allow direct typed groups at top level
                    md = &container;
                }
                if (md->contains("bool") && (*md)["bool"].is_object())
                {
                    for (auto it = (*md)["bool"].begin(); it != (*md)["bool"].end(); ++it)
                    {
                        if (it.value().is_boolean())
                            out_defaults.bools[it.key()] = it.value().get<bool>();
                    }
                }
                if (md->contains("int") && (*md)["int"].is_object())
                {
                    for (auto it = (*md)["int"].begin(); it != (*md)["int"].end(); ++it)
                    {
                        if (it.value().is_number_integer())
                            out_defaults.ints[it.key()] = it.value().get<int>();
                    }
                }
                if (md->contains("float") && (*md)["float"].is_object())
                {
                    for (auto it = (*md)["float"].begin(); it != (*md)["float"].end(); ++it)
                    {
                        if (it.value().is_number())
                            out_defaults.floats[it.key()] = it.value().get<float>();
                    }
                }
                if (md->contains("string") && (*md)["string"].is_object())
                {
                    for (auto it = (*md)["string"].begin(); it != (*md)["string"].end(); ++it)
                    {
                        if (it.value().is_string())
                            out_defaults.strings[it.key()] = it.value().get<std::string>();
                    }
                }
            }
        };

        for (const auto& dir : candidate_dirs)
        {
            if (!wi::helper::DirectoryExists(dir))
                continue;

            const std::string presets_file = dir + "presets.json";
            if (wi::helper::FileExists(presets_file))
            {
                wi::vector<uint8_t> data;
                if (wi::helper::FileRead(presets_file, data))
                {
                    try
                    {
                        json j = json::parse(data.begin(), data.end());
                        auto process_entry = [&](const json& e) {
                            std::string nm;
                            DynamicPresetDefaults defs;
                            if (e.is_string())
                            {
                                nm = e.get<std::string>();
                            }
                            else if (e.is_object())
                            {
                                if (e.contains("name") && e["name"].is_string())
                                    nm = e["name"].get<std::string>();
                                else if (e.contains("id") && e["id"].is_string())
                                    nm = e["id"].get<std::string>();
                                else if (e.contains("class") && e["class"].is_string())
                                    nm = e["class"].get<std::string>();
                                parse_defaults(e, defs);
                            }
                            if (!nm.empty())
                            {
                                register_preset(nm, &defs);
                            }
                        };

                        if (j.is_object())
                        {
                            for (const char* key : { "presets", "entity_presets", "entity_classes" })
                            {
                                if (j.contains(key) && j[key].is_array())
                                {
                                    for (auto& e : j[key])
                                        process_entry(e);
                                }
                            }
                        }
                        else if (j.is_array())
                        {
                            for (auto& e : j)
                                process_entry(e);
                        }
                    }
                    catch (...) {
                        // ignore JSON errors silently for editor UX
                    }
                }
            }

            // Fallback: enumerate individual .json files as entity classes
            wi::helper::GetFileNamesInDirectory(dir, [&](std::string path) {
                // normalize lower-case compare for presets.json
                std::string filename = wi::helper::GetFileNameFromPath(path);
                if (wi::helper::toLower(filename) == "presets.json")
                    return;
                if (wi::helper::toLower(wi::helper::GetExtensionFromFileName(filename)) != "json")
                    return;

                // Try to parse a display name and defaults from file, fallback to filename without extension
                std::string display = wi::helper::RemoveExtension(filename);
                DynamicPresetDefaults defs;
                wi::vector<uint8_t> data;
                if (wi::helper::FileRead(path, data))
                {
                    try
                    {
                        json jj = json::parse(data.begin(), data.end());
                        if (jj.is_object())
                        {
                            if (jj.contains("name") && jj["name"].is_string())
                            {
                                display = jj["name"].get<std::string>();
                            }
                            else if (jj.contains("class") && jj["class"].is_string())
                            {
                                display = jj["class"].get<std::string>();
                            }
                            parse_defaults(jj, defs);
                        }
                    }
                    catch (...) {
                        // ignore file-specific JSON errors
                    }
                }
                register_preset(display, &defs);
            }, "json");
        }

        for (size_t i = 0; i < dynamicEntityClasses.size(); ++i)
        {
            presetCombo.AddItem(dynamicEntityClasses[i], USER_PRESET_BASE + (uint64_t)i);
        }
    }

    presetCombo.OnSelect([=](wi::gui::EventArgs args){
        forEachSelected([this](auto metadata, auto args_inner){
            if (args_inner.userdata >= USER_PRESET_BASE)
            {
                // Dynamic entity class: store class name and keep preset as Custom
                size_t idx = size_t(args_inner.userdata - USER_PRESET_BASE);
                if (idx < dynamicEntityClasses.size())
                {
                    metadata->preset = MetadataComponent::Preset::Custom;
                    const std::string& cls = dynamicEntityClasses[idx];
                    metadata->string_values.set("entity_class", cls);

                    // Apply defaults from preset (override keys defined by preset, keep others)
                    auto it = dynamicEntityDefaults.find(cls);
                    if (it != dynamicEntityDefaults.end())
                    {
                        for (const auto& kv : it->second.bools)  metadata->bool_values.set(kv.first, kv.second);
                        for (const auto& kv : it->second.ints)   metadata->int_values.set(kv.first, kv.second);
                        for (const auto& kv : it->second.floats) metadata->float_values.set(kv.first, kv.second);
                        for (const auto& kv : it->second.strings)metadata->string_values.set(kv.first, kv.second);
                    }
                }
            }
            else
            {
                // Built-in preset: set preset and clear any previous entity_class label
                metadata->preset = (MetadataComponent::Preset)args_inner.userdata;
                if (metadata->string_values.has("entity_class"))
                {
                    metadata->string_values.erase("entity_class");
                }
            }
        })(args);
        RefreshEntries();
    });
    AddWidget(&presetCombo);

	addCombo.Create("");
	addCombo.SetInvalidSelectionText("+");
	addCombo.SetDropArrowEnabled(false);
	addCombo.AddItem("bool");
	addCombo.AddItem("int");
	addCombo.AddItem("float");
	addCombo.AddItem("string");
	addCombo.OnSelect([=] (auto args) {
		forEachSelected([] (auto metadata, auto args) {
			std::string property_name = "name";
			switch (args.iValue)
			{
			default:
			case 0:
				while (metadata->bool_values.has(property_name))
					property_name += "0";
				metadata->bool_values.set(property_name, false);
				break;
			case 1:
				while (metadata->int_values.has(property_name))
					property_name += "0";
				metadata->int_values.set(property_name, 0);
				break;
			case 2:
				while (metadata->float_values.has(property_name))
					property_name += "0";
				metadata->float_values.set(property_name, 0.0f);
				break;
			case 3:
				while (metadata->string_values.has(property_name))
					property_name += "0";
				metadata->string_values.set(property_name, "");
				break;
			}
		})(args);
		addCombo.SetSelectedWithoutCallback(-1);
		RefreshEntries();
	});
	AddWidget(&addCombo);

	SetMinimized(true);
	SetVisible(false);

	SetLocalizationEnabled(false);

	SetEntity(INVALID_ENTITY);
}

void MetadataWindow::SetEntity(Entity entity)
{
	bool changed = entity != this->entity;
	this->entity = entity;

	Scene& scene = editor->GetCurrentScene();
	const MetadataComponent* metadata = scene.metadatas.GetComponent(entity);

	if (metadata != nullptr)
	{
		bool selected_dynamic = false;
		if (metadata->string_values.has("entity_class"))
		{
			std::string cls = metadata->string_values.get("entity_class");
			for (size_t i = 0; i < dynamicEntityClasses.size(); ++i)
			{
				if (dynamicEntityClasses[i] == cls)
				{
					presetCombo.SetSelectedByUserdataWithoutCallback(USER_PRESET_BASE + (uint64_t)i);
					selected_dynamic = true;
					break;
				}
			}
		}
		if (!selected_dynamic)
		{
			presetCombo.SetSelectedByUserdataWithoutCallback((uint64_t)metadata->preset);
		}

		if (changed)
		{
			RefreshEntries();
		}

		SetEnabled(true);
	}
	else
	{
		this->entity = INVALID_ENTITY;
	}
}

void MetadataWindow::RefreshEntries()
{
	for (auto& entry : entries)
	{
		RemoveWidget(&entry.name);
		RemoveWidget(&entry.value);
		RemoveWidget(&entry.check);
		RemoveWidget(&entry.remove);
	}
	entries.clear();

	Scene& scene = editor->GetCurrentScene();
	MetadataComponent* metadata = scene.metadatas.GetComponent(entity);
	if (metadata == nullptr)
		return;

	auto forEachSelectedWithRefresh = [this] (auto name, auto func) {
		return [this, func, name] (auto args) {
			wi::scene::Scene& scene = editor->GetCurrentScene();
			for (auto& x : editor->translator.selected)
			{
				MetadataComponent* metadata = scene.metadatas.GetComponent(x.entity);
				if (metadata != nullptr)
				{
					func(metadata, args);
				}
			}
			wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT, [this](uint64_t userdata) {
				RefreshEntries();
			});
		};
	};

	// Note: to not disturb the ordering of entries while editing them, we iterate by the ordered names array in each table

	for (auto& name : metadata->bool_values.names)
	{
		Entry& entry = entries.emplace_back();
		entry.name.Create("");
		entry.name.SetText(name);
		entry.name.OnInputAccepted(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->bool_values.has(name))
				return;
			auto value = metadata->bool_values.get(name);
			metadata->bool_values.erase(name);
			metadata->bool_values.set(args.sValue, value);
		}));
		AddWidget(&entry.name);

		entry.is_bool = true;
		entry.check.Create("");
		entry.check.SetText(" = (bool) ");
		entry.check.SetCheck(metadata->bool_values.get(name));
		entry.check.OnClick(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->bool_values.has(name))
				return;
			metadata->bool_values.set(name, args.bValue);
		}));
		AddWidget(&entry.check);

		entry.remove.Create("");
		entry.remove.SetText("X");
		entry.remove.SetSize(XMFLOAT2(entry.remove.GetSize().y, entry.remove.GetSize().y));
		entry.remove.OnClick(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->bool_values.has(name))
				return;
			metadata->bool_values.erase(name);
		}));
		AddWidget(&entry.remove);
	}

	for (auto& name : metadata->int_values.names)
	{
		Entry& entry = entries.emplace_back();
		entry.name.Create("");
		entry.name.SetText(name);
		entry.name.OnInputAccepted(forEachSelectedWithRefresh(name, [name] (auto metadata, auto args) {
			if (!metadata->int_values.has(name))
				return;
			auto value = metadata->int_values.get(name);
			metadata->int_values.erase(name);
			metadata->int_values.set(args.sValue, value);
		}));
		AddWidget(&entry.name);

		entry.is_bool = false;
		entry.value.Create("");
		entry.value.SetDescription(" = (int) ");
		entry.value.SetSize(XMFLOAT2(60, entry.value.GetSize().y));
		entry.value.SetValue(metadata->int_values.get(name));
		entry.value.OnInputAccepted(forEachSelectedWithRefresh(name, [name] (auto metadata, auto args) {
			if (!metadata->int_values.has(name))
				return;
			metadata->int_values.set(name, args.iValue);
		}));
		AddWidget(&entry.value);

		entry.remove.Create("");
		entry.remove.SetText("X");
		entry.remove.SetSize(XMFLOAT2(entry.remove.GetSize().y, entry.remove.GetSize().y));
		entry.remove.OnClick(forEachSelectedWithRefresh(name, [name] (auto metadata, auto args) {
			if (!metadata->int_values.has(name))
				return;
			metadata->int_values.erase(name);
		}));
		AddWidget(&entry.remove);
	}

	for (auto& name : metadata->float_values.names)
	{
		Entry& entry = entries.emplace_back();
		entry.name.Create("");
		entry.name.SetText(name);
		entry.name.OnInputAccepted(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->float_values.has(name))
				return;
			auto value = metadata->float_values.get(name);
			metadata->float_values.erase(name);
			metadata->float_values.set(args.sValue, value);
		}));
		AddWidget(&entry.name);

		entry.is_bool = false;
		entry.value.Create("");
		entry.value.SetDescription(" = (float) ");
		entry.value.SetSize(XMFLOAT2(60, entry.value.GetSize().y));
		entry.value.SetValue(metadata->float_values.get(name));
		entry.value.OnInputAccepted(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->float_values.has(name))
				return;
			metadata->float_values.set(name, args.fValue);
		}));
		AddWidget(&entry.value);

		entry.remove.Create("");
		entry.remove.SetText("X");
		entry.remove.SetSize(XMFLOAT2(entry.remove.GetSize().y, entry.remove.GetSize().y));
		entry.remove.OnClick(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->float_values.has(name))
				return;
			metadata->float_values.erase(name);
		}));
		AddWidget(&entry.remove);
	}

	for (auto& name : metadata->string_values.names)
	{
		Entry& entry = entries.emplace_back();
		entry.name.Create("");
		entry.name.SetText(name);
		entry.name.OnInputAccepted(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->string_values.has(name))
				return;
			auto value = metadata->string_values.get(name);
			metadata->string_values.erase(name);
			metadata->string_values.set(args.sValue, value);
		}));
		AddWidget(&entry.name);

		entry.is_bool = false;
		entry.value.Create("");
		entry.value.SetDescription(" = (string) ");
		entry.value.SetSize(XMFLOAT2(120, entry.value.GetSize().y));
		entry.value.SetValue(metadata->string_values.get(name));
		entry.value.OnInputAccepted(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->string_values.has(name))
				return;
			metadata->string_values.set(name, args.sValue);
		}));
		AddWidget(&entry.value);

		entry.remove.Create("");
		entry.remove.SetText("X");
		entry.remove.SetSize(XMFLOAT2(entry.remove.GetSize().y, entry.remove.GetSize().y));
		entry.remove.OnClick(forEachSelectedWithRefresh(name, [=] (auto metadata, auto args) {
			if (!metadata->string_values.has(name))
				return;
			metadata->string_values.erase(name);
		}));
		AddWidget(&entry.remove);
	}

	editor->generalWnd.RefreshTheme();
}

void MetadataWindow::ResizeLayout()
{
	wi::gui::Window::ResizeLayout();
	layout.margin_left = 100;

	layout.add(presetCombo);
	layout.add_fullwidth(addCombo);

	layout.jump();

	for (auto& entry : entries)
	{
		entry.remove.SetPos(XMFLOAT2(layout.padding, layout.y));

		if (entry.is_bool)
		{
			entry.check.SetPos(XMFLOAT2(layout.width - layout.padding - entry.check.GetSize().x, layout.y));

			entry.name.SetSize(XMFLOAT2(layout.width - wi::font::TextWidth(entry.check.GetText(), entry.check.font.params) - layout.padding * 3 - entry.check.GetSize().x - entry.remove.GetSize().x, entry.name.GetSize().y));
		}
		else
		{
			entry.value.SetSize(XMFLOAT2(std::max(wi::font::TextWidth(entry.value.GetCurrentInputValue(), entry.value.font.params) + layout.padding, entry.value.GetSize().y), entry.value.GetSize().y));
			entry.value.SetPos(XMFLOAT2(layout.width - layout.padding - entry.value.GetSize().x, layout.y));

			entry.name.SetSize(XMFLOAT2(layout.width - wi::font::TextWidth(entry.value.GetDescription(), entry.value.font.params) - layout.padding * 3 - entry.value.GetSize().x - entry.remove.GetSize().x, entry.name.GetSize().y));
		}

		entry.name.SetPos(XMFLOAT2(entry.remove.GetPos().x + entry.remove.GetSize().x + layout.padding, layout.y));

		layout.y += entry.name.GetSize().y;
		layout.y += layout.padding;
		layout.y += entry.name.GetShadowRadius();
	}

}
