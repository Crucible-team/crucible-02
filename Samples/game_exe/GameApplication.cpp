#include "stdafx.h"
#include <fstream>
#include <thread>

#define CONTENT_DIR "../../Content/"

using namespace wi::ecs;
using namespace wi::scene;
using namespace wi::graphics;



enum class EditorActions
{
	// Camera movement
	MOVE_CAMERA_FORWARD,
	MOVE_CAMERA_BACKWARD,
	MOVE_CAMERA_LEFT,
	MOVE_CAMERA_RIGHT,
	MOVE_CAMERA_UP,
	MOVE_CAMERA_DOWN,

	// Entity actions
	DUPLICATE_ENTITY,

	// Selection actions
	SELECT_ALL_ENTITIES,
	DESELECT_ALL_ENTITIES,
	FOCUS_ON_SELECTION,
	RENAME_SELECTED,

	// Edit actions
	UNDO_ACTION,
	REDO_ACTION,
	COPY_ACTION,
	CUT_ACTION,
	PASTE_ACTION,
	DELETE_ACTION,

	// User actions
	MOVE_TOGGLE_ACTION,
	ROTATE_TOGGLE_ACTION,
	SCALE_TOGGLE_ACTION,

	// Engine actions
	SCREENSHOT,
	SCREENSHOT_ALPHA,
	INSPECTOR_MODE,
	PLACE_INSTANCES,

	// Scene actions
	SAVE_SCENE_AS,
	SAVE_SCENE,

	// Transform actions
	ENABLE_TRANSFORM_TOOL,

	// View mode actions
	WIREFRAME_MODE,

	// Effect actions
	DEPTH_OF_FIELD_REFOCUS_TO_POINT,
	COLOR_GRADING_REFERENCE,

	// Other actions
	RAGDOLL_AND_PHYSICS_IMPULSE_TESTER,
	ORTHO_CAMERA,
	HIERARCHY_SELECT,
	ADD_TO_SPLINE,

	COUNT
};
struct HotkeyInfo
{
	wi::input::BUTTON button;
	bool press = false;
	bool control = false;
	bool shift = false;
};
HotkeyInfo hotkeyActions[size_t(EditorActions::COUNT)] = {
	{wi::input::BUTTON('W'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//MOVE_CAMERA_FORWARD,
	{wi::input::BUTTON('S'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//MOVE_CAMERA_BACKWARD,
	{wi::input::BUTTON('A'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//MOVE_CAMERA_LEFT,
	{wi::input::BUTTON('D'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//MOVE_CAMERA_RIGHT,
	{wi::input::BUTTON('E'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//MOVE_CAMERA_UP,
	{wi::input::BUTTON('Q'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//MOVE_CAMERA_DOWN,
	{wi::input::BUTTON('D'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//DUPLICATE_ENTITY,
	{wi::input::BUTTON('A'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//SELECT_ALL_ENTITIES,
	{wi::input::BUTTON::KEYBOARD_BUTTON_ESCAPE,	/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//DESELECT_ALL_ENTITIES,
	{wi::input::BUTTON('F'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//FOCUS_ON_SELECTION,
	{wi::input::BUTTON::KEYBOARD_BUTTON_F2,		/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//RENAME_SELECTED,
	{wi::input::BUTTON('Z'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//UNDO_ACTION,
	{wi::input::BUTTON('Y'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//REDO_ACTION,
	{wi::input::BUTTON('C'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//COPY_ACTION,
	{wi::input::BUTTON('X'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//CUT_ACTION,
	{wi::input::BUTTON('V'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//PASTE_ACTION,
	{wi::input::BUTTON::KEYBOARD_BUTTON_DELETE,	/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//DELETE_ACTION,
	{wi::input::BUTTON('1'),					/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//MOVE_TOGGLE_ACTION,
	{wi::input::BUTTON('2'),					/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//ROTATE_TOGGLE_ACTION,
	{wi::input::BUTTON('3'),					/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//SCALE_TOGGLE_ACTION,
	{wi::input::BUTTON::KEYBOARD_BUTTON_F3,		/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//SCREENSHOT,
	{wi::input::BUTTON::KEYBOARD_BUTTON_F4,		/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//SCREENSHOT_ALPHA,
	{wi::input::BUTTON('I'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//INSPECTOR_MODE,
	{wi::input::BUTTON::MOUSE_BUTTON_LEFT,		/*press=*/ true,		/*control=*/ true,		/*shift=*/ true},	//PLACE_INSTANCES,
	{wi::input::BUTTON('S'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ true},	//SAVE_SCENE_AS,
	{wi::input::BUTTON('S'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//SAVE_SCENE,
	{wi::input::BUTTON('T'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//ENABLE_TRANSFORM_TOOL,
	{wi::input::BUTTON('W'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//WIREFRAME_MODE,
	{wi::input::BUTTON('C'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//DEPTH_OF_FIELD_REFOCUS_TO_POINT,
	{wi::input::BUTTON('G'),					/*press=*/ false,		/*control=*/ true,		/*shift=*/ false},	//COLOR_GRADING_REFERENCE,
	{wi::input::BUTTON('P'),					/*press=*/ false,		/*control=*/ false,		/*shift=*/ false},	//RAGDOLL_AND_PHYSICS_IMPULSE_TESTER,
	{wi::input::BUTTON('O'),					/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//ORTHO_CAMERA,
	{wi::input::BUTTON('H'),					/*press=*/ true,		/*control=*/ false,		/*shift=*/ false},	//HIERARCHY_SELECT,
	{wi::input::BUTTON('E'),					/*press=*/ true,		/*control=*/ true,		/*shift=*/ false},	//ADD_TO_SPLINE,
};
static_assert(arraysize(hotkeyActions) == size_t(EditorActions::COUNT));
bool CheckInput(EditorActions action)
{
	const HotkeyInfo& hotkey = hotkeyActions[size_t(action)];
	bool ret = false;
	if (hotkey.press)
	{
		ret |= wi::input::Press(hotkey.button);
	}
	else
	{
		ret |= wi::input::Down(hotkey.button);
	}
	if (hotkey.control)
	{
		ret &= wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) || wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
	}
	if (hotkey.shift)
	{
		ret &= wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) || wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
	}
	return ret;
}
std::string GetInputString(EditorActions action)
{
	const HotkeyInfo& hotkey = hotkeyActions[size_t(action)];
	std::string ret = wi::input::ButtonToString(hotkey.button).text;
	if (hotkey.shift)
	{
		ret = "Shift + " + ret;
	}
	if (hotkey.control)
	{
		ret = "Ctrl + " + ret;
	}
	return ret;
}
void HotkeyRemap(Game_Application* main)
{
	static const wi::unordered_map<std::string, EditorActions> actionMap = {
		{"MOVE_CAMERA_FORWARD", EditorActions::MOVE_CAMERA_FORWARD},
		{"MOVE_CAMERA_BACKWARD", EditorActions::MOVE_CAMERA_BACKWARD},
		{"MOVE_CAMERA_LEFT", EditorActions::MOVE_CAMERA_LEFT},
		{"MOVE_CAMERA_RIGHT", EditorActions::MOVE_CAMERA_RIGHT},
		{"MOVE_CAMERA_UP", EditorActions::MOVE_CAMERA_UP},
		{"MOVE_CAMERA_DOWN", EditorActions::MOVE_CAMERA_DOWN},
		{"DUPLICATE_ENTITY", EditorActions::DUPLICATE_ENTITY},
		{"SELECT_ALL_ENTITIES", EditorActions::SELECT_ALL_ENTITIES},
		{"DESELECT_ALL_ENTITIES", EditorActions::DESELECT_ALL_ENTITIES},
		{"FOCUS_ON_SELECTION", EditorActions::FOCUS_ON_SELECTION},
		{"RENAME_SELECTED", EditorActions::RENAME_SELECTED},
		{"UNDO_ACTION", EditorActions::UNDO_ACTION},
		{"REDO_ACTION", EditorActions::REDO_ACTION},
		{"COPY_ACTION", EditorActions::COPY_ACTION},
		{"CUT_ACTION", EditorActions::CUT_ACTION},
		{"PASTE_ACTION", EditorActions::PASTE_ACTION},
		{"DELETE_ACTION", EditorActions::DELETE_ACTION},
		{"MOVE_TOGGLE_ACTION", EditorActions::MOVE_TOGGLE_ACTION},
		{"ROTATE_TOGGLE_ACTION", EditorActions::ROTATE_TOGGLE_ACTION},
		{"SCALE_TOGGLE_ACTION", EditorActions::SCALE_TOGGLE_ACTION},
		{"MAKE_NEW_SCREENSHOT", EditorActions::SCREENSHOT},
		{"MAKE_NEW_SCREENSHOT_ALPHA", EditorActions::SCREENSHOT_ALPHA},
		{"INSPECTOR_MODE", EditorActions::INSPECTOR_MODE},
		{"PLACE_INSTANCES", EditorActions::PLACE_INSTANCES},
		{"SAVE_SCENE_AS", EditorActions::SAVE_SCENE_AS},
		{"SAVE_SCENE", EditorActions::SAVE_SCENE},
		{"ENABLE_TRANSFORM_TOOL", EditorActions::ENABLE_TRANSFORM_TOOL},
		{"WIREFRAME_MODE", EditorActions::WIREFRAME_MODE},
		{"DEPTH_OF_FIELD_REFOCUS_TO_POINT", EditorActions::DEPTH_OF_FIELD_REFOCUS_TO_POINT},
		{"COLOR_GRADING_REFERENCE", EditorActions::COLOR_GRADING_REFERENCE},
		{"RAGDOLL_AND_PHYSICS_IMPULSE_TESTER", EditorActions::RAGDOLL_AND_PHYSICS_IMPULSE_TESTER},
		{"ORTHO_CAMERA", EditorActions::ORTHO_CAMERA},
		{"HIERARCHY_SELECT", EditorActions::HIERARCHY_SELECT},
	};
	static const wi::unordered_map<std::string, wi::input::BUTTON> buttonMap = {
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
		{"F12", wi::input::KEYBOARD_BUTTON_F12}
	};

	wi::config::Section hotkeyssection = main->config.GetSection("hotkeys");
	for (auto& x : hotkeyssection)
	{
		auto itAction = actionMap.find(wi::helper::toUpper(x.first));
		if (itAction == actionMap.end())
			continue;
		EditorActions action = itAction->second;
		std::string hotkeyString = wi::helper::toUpper(x.second);
		wi::input::BUTTON button = wi::input::BUTTON_NONE;

		// Find the main key from the whole hotkey string:
		std::string firstNonModifierKey;
		std::istringstream iss(hotkeyString);
		std::string token;
		while (std::getline(iss, token, '+'))
		{
			if (token != "CTRL" && token != "SHIFT" && !token.empty())
			{
				firstNonModifierKey = token;
			}
		}

		// Try to find the key in the map
		auto itButton = buttonMap.find(firstNonModifierKey);
		if (itButton != buttonMap.end())
		{
			button = itButton->second;
		}
		else
		{
			// Cast individual key to button
			if (firstNonModifierKey.length() == 1)
			{
				char c = firstNonModifierKey[0];
				if (std::isalnum(c))
				{
					button = wi::input::BUTTON(c);
				}
			}
		}

		// Remap hotkey if button is successfully found:
		if (button != wi::input::BUTTON_NONE)
		{

			hotkeyActions[size_t(action)] = HotkeyInfo{ button, hotkeyActions[size_t(action)].press, hotkeyString.find("CTRL") != std::string::npos, hotkeyString.find("SHIFT") != std::string::npos };
		}
	}
}

Game_Application::~Game_Application()
{
	config.Commit();
}

void Game_Application::Initialize()
{

	Application::Initialize();

	infoDisplay.active = false;
	infoDisplay.watermark = false;
	infoDisplay.fpsinfo = true;
	infoDisplay.resolution = false;
	infoDisplay.heap_allocation_counter = false;

	//Load hotkeys here
	
	HotkeyRemap(this);
	//renderer.main = this;

	renderer.init(canvas);
	renderer.Load();

	ActivatePath(&renderer);
}

void Game_Application::Update(float dt)
{
	Application::Update(dt);
	if (CheckInput(EditorActions::SCREENSHOT))
	{
		wi::backlog::post("You pressed the Screenshot Action Key!",wi::backlog::LogLevel::Warning);
	}
}

void Game_Application::Compose(wi::graphics::CommandList cmd)
{
	Application::Compose(cmd);
}

void Game_Renderer::ResizeLayout()
{
	RenderPath3D::ResizeLayout();

	/*float screenW = GetLogicalWidth();
	float screenH = GetLogicalHeight();
	label.SetPos(XMFLOAT2(screenW / 2.f - label.scale.x / 2.f, screenH * 0.95f));*/
}

void Game_Renderer::Render() const
{
	RenderPath3D::Render();
}


void Game_Renderer::Load()
{
	RenderPath3D::Load();
}

void Game_Renderer::Update(float dt)
{

	RenderPath3D::Update(dt);
}
