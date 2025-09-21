#pragma once

class Translator
{
private:
	bool dragging = false;
	bool dragStarted = false;
	bool dragEnded = false;
	XMFLOAT3 intersection_start = XMFLOAT3(0, 0, 0);
	XMFLOAT3 axis = XMFLOAT3(1, 0, 0);
	float angle = 0;
	float angle_start = 0;
	bool has_selected_transform = false;
	wi::vector<uint64_t> temp_filters;
	wi::vector<wi::ecs::Entity> selectedWithHierarchy; // all the selected entities and their descendants
public:

	void Update(const wi::scene::CameraComponent& camera, const XMFLOAT4& currentMouse, const wi::Canvas& canvas);
	void Draw(const wi::scene::CameraComponent& camera, const XMFLOAT4& currentMouse, wi::graphics::CommandList cmd) const;

	// Attach selection to translator temporarily
	void PreTranslate();
	// Apply translator to selection
	void PostTranslate();

	wi::scene::Scene* scene = nullptr;
	wi::scene::TransformComponent transform;
	wi::vector<wi::scene::PickResult> selected; // all the selected picks
	wi::unordered_set<wi::ecs::Entity> selectedEntitiesLookup; // fast lookup for selected entities
	wi::vector<wi::ecs::Entity> selectedEntitiesNonRecursive; // selected entities that don't contain entities that would be included in recursive iterations

	float scale_snap = 1;
	float rotate_snap = XM_PIDIV4;
	float translate_snap = 1;
	float bounds_snap = 1;
	bool  bounds_snap_enabled = false;
	float opacity = 1;
	float darken_negative_axes = 1;

	// Which face - handle(small circle) are we hovering / dragging ? -1 = none, 0..5 = X - , X + , Y - , Y + , Z - , Z +
	int  bounds_hover_handle = -1;
	int  bounds_active_handle = -1;

	// World-space centers of each face (filled each frame from bounds_world)
	XMFLOAT3 bounds_face_centers[6] = {};

	// Visual size control (relative to gizmo distance scaler):
	// Note: you already have `dist` which scales gizmo visuals globally:
	float bounds_handle_radius_factor = 0.12f; // feel free to tweak (in world units ~ dist)

	enum TRANSLATOR_STATE
	{
		TRANSLATOR_IDLE,
		TRANSLATOR_X,
		TRANSLATOR_Y,
		TRANSLATOR_Z,
		TRANSLATOR_XY,
		TRANSLATOR_XZ,
		TRANSLATOR_YZ,
		TRANSLATOR_XYZ,
		TRANSLATOR_BOUNDS
	} state = TRANSLATOR_IDLE;

	XMMATRIX GetMirrorMatrix(TRANSLATOR_STATE state, const wi::scene::CameraComponent& camera) const;
	void WriteAxisText(TRANSLATOR_STATE axis, const wi::scene::CameraComponent& camera, char* text) const;

	float dist = 1;

	bool isTranslator = true;
	bool isScalator = false;
	bool isRotator = false;
	bool isBoundSizer = false;
	bool IsEnabled() const { return isTranslator || isRotator || isScalator || isBoundSizer; }
	void SetEnabled(bool value)
	{
		if (value && !IsEnabled())
		{
			isTranslator = true;
		}
		else if (!value && IsEnabled())
		{
			isTranslator = false;
			isScalator = false;
			isRotator = false;
			isBoundSizer = false;
		}
	}


	// Check if the drag started in this exact frame
	bool IsDragStarted() const { return dragStarted; };
	// Check if the drag ended in this exact frame
	bool IsDragEnded() const { return dragEnded; };

	bool IsInteracting() const { return state != TRANSLATOR_IDLE; }

	wi::scene::TransformComponent transform_start;
	wi::vector<XMFLOAT4X4> matrices_start;
	wi::vector<XMFLOAT4X4> matrices_current;

	// Working AABBs in WORLD space (selection & drag)
	wi::primitive::AABB bounds_world0;      // AABB at drag start
	wi::primitive::AABB bounds_world;       // live AABB while dragging

	// Drag state:
	XMFLOAT3 bounds_drag_normal = XMFLOAT3(0, 0, 0); // face normal we are dragging along
	XMFLOAT3 bounds_anchor = XMFLOAT3(0, 0, 0); // opposite anchor point (stays fixed)
	XMFLOAT3 bounds_drag_start_hit = XMFLOAT3(0, 0, 0); // starting hit point on the drag plane
};

