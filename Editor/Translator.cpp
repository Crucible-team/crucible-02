#include "stdafx.h"
#include "Translator.h"
#include "wiRenderer.h"
#include "wiInput.h"
#include "wiMath.h"
#include "shaders/ShaderInterop_Renderer.h"
#include "wiEventHandler.h"

using namespace wi::ecs;
using namespace wi::scene;
using namespace wi::graphics;
using namespace wi::primitive;

namespace Translator_Internal
{
	PipelineState pso_solidpart;
	PipelineState pso_wirepart;
	const float origin_size = 0.2f;
	const float axis_length = 3.5f;
	const float plane_min = 0.5f;
	const float plane_max = 1.5f;
	const float circle_radius = axis_length;
	const float circle_width = 1;
	const float circle2_radius = circle_radius + 0.7f;
	const float circle2_width = 0.3f;

	void LoadShaders()
	{
		GraphicsDevice* device = wi::graphics::GetDevice();

		{
			PipelineStateDesc desc;

			desc.vs = wi::renderer::GetShader(wi::enums::VSTYPE_VERTEXCOLOR);
			desc.ps = wi::renderer::GetShader(wi::enums::PSTYPE_VERTEXCOLOR);
			desc.il = wi::renderer::GetInputLayout(wi::enums::ILTYPE_VERTEXCOLOR);
			desc.dss = wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEFAULT);
			desc.rs = wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
			desc.bs = wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
			desc.pt = PrimitiveTopology::TRIANGLELIST;

			device->CreatePipelineState(&desc, &pso_solidpart);
		}

		{
			PipelineStateDesc desc;

			desc.vs = wi::renderer::GetShader(wi::enums::VSTYPE_VERTEXCOLOR);
			desc.ps = wi::renderer::GetShader(wi::enums::PSTYPE_VERTEXCOLOR);
			desc.il = wi::renderer::GetInputLayout(wi::enums::ILTYPE_VERTEXCOLOR);
			desc.dss = wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEFAULT);
			desc.rs = wi::renderer::GetRasterizerState(wi::enums::RSTYPE_WIRE_DOUBLESIDED);
			desc.bs = wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
			desc.pt = PrimitiveTopology::LINELIST;

			device->CreatePipelineState(&desc, &pso_wirepart);
		}
	}


	struct Vertex
	{
		XMFLOAT4 position;
		XMFLOAT4 color;
	};
	const Vertex cubeVerts[] = {
		{XMFLOAT4(-1,1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,-1,1), XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,1,1),	XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,-1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,-1,1), XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,1,1),	XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,-1,1), XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,1,1),	XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,1,1),	XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,-1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,-1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,-1,1), XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,-1,1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,-1,-1,1),  XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,-1,1),   XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(1,1,1,1),	XMFLOAT4(1,1,1,1)},
		{XMFLOAT4(-1,1,-1,1),  XMFLOAT4(1,1,1,1)},
	};
}
using namespace Translator_Internal;

static wi::primitive::AABB ComputeSelectionAABB_World(
	wi::scene::Scene& scene,
	const wi::vector<wi::ecs::Entity>& entities)
{
	using wi::primitive::AABB;
	AABB out;
	bool first = true;

	for (auto e : entities)
	{
		// Prefer object’s mesh AABB in world space if available; fallback to transform center as tiny box:
		auto* tf = scene.transforms.GetComponent(e);
		if (!tf) continue;

		AABB aabb;
		{
			// If the entity has an object and mesh data, query its world AABB:
			auto* obj = scene.objects.GetComponent(e);
			if (obj && obj->meshID != wi::ecs::INVALID_ENTITY)
			{
				auto* mesh = scene.meshes.GetComponent(obj->meshID);
				if (mesh)
				{
					aabb = mesh->aabb;
					// Mesh AABB is typically in local space; move to world by transform:
					aabb = aabb.transform(tf->GetWorldMatrix());
				}
			}
		}

		if (aabb.getHalfWidth().x == 0 && aabb.getHalfWidth().y == 0 && aabb.getHalfWidth().z == 0)
		{
			// Fallback 1cm box at transform position:
			XMFLOAT3 p = tf->GetPosition();
			aabb.createFromHalfWidth(p, XMFLOAT3(0.01f, 0.01f, 0.01f));
		}

		out = first ? aabb : AABB::Merge(out, aabb);
		first = false;
	}
	return out;
}


void Translator::Update(const CameraComponent& camera, const XMFLOAT4& currentMouse, const wi::Canvas& canvas)
{
	if (selected.empty())
	{
		transform.ClearTransform();
		transform.UpdateTransform();
		state = TRANSLATOR_IDLE;
		return;
	}

	dragStarted = false;
	dragEnded = false;

	XMVECTOR pos = transform.GetPositionV();

	// Non recursive selection will be computed to not apply recursive operations two times
	//	A recursive operation is for example translating a parent transform
	//	An other recursive operation is serializing selected parent entities
	Scene& scene = *this->scene;
	selectedEntitiesLookup.clear();
	for (auto& x : selected)
	{
		selectedEntitiesLookup.insert(x.entity);
	}
	selectedEntitiesNonRecursive.clear();
	for (auto& x : selected)
	{
		const HierarchyComponent* hier = scene.hierarchy.GetComponent(x.entity);
		if (hier == nullptr || selectedEntitiesLookup.count(hier->parentID) == 0)
		{
			selectedEntitiesNonRecursive.push_back(x.entity);
		}
	}

	if (isBoundSizer)
	{
		PreTranslate();
		if (!has_selected_transform)
		{
			state = TRANSLATOR_IDLE;
			return;
		}

		const Ray ray = wi::renderer::GetPickRay((long)currentMouse.x, (long)currentMouse.y, canvas, camera);
		const XMVECTOR rayOrigin = XMLoadFloat3(&ray.origin);
		const XMVECTOR rayDir = XMLoadFloat3(&ray.direction);

		XMFLOAT3 p = transform.GetPosition();
		dist = std::max(wi::math::Distance(p, camera.Eye) * 0.05f, 0.0001f);

		// Establish (or refresh) live bounds every frame:
		bounds_world = ComputeSelectionAABB_World(scene, selectedEntitiesNonRecursive);

		{
			const XMFLOAT3 bmin = bounds_world.getMin();
			const XMFLOAT3 bmax = bounds_world.getMax();
			const float cx = (bmin.x + bmax.x) * 0.5f;
			const float cy = (bmin.y + bmax.y) * 0.5f;
			const float cz = (bmin.z + bmax.z) * 0.5f;

			// Order: 0:X-, 1:X+, 2:Y-, 3:Y+, 4:Z-, 5:Z+
			bounds_face_centers[0] = XMFLOAT3(bmin.x, cy, cz);
			bounds_face_centers[1] = XMFLOAT3(bmax.x, cy, cz);
			bounds_face_centers[2] = XMFLOAT3(cx, bmin.y, cz);
			bounds_face_centers[3] = XMFLOAT3(cx, bmax.y, cz);
			bounds_face_centers[4] = XMFLOAT3(cx, cy, bmin.z);
			bounds_face_centers[5] = XMFLOAT3(cx, cy, bmax.z);
		}

		// Hover test (when not dragging): choose face/center
		if (!dragging)
		{
			state = TRANSLATOR_IDLE;
			bounds_drag_normal = XMFLOAT3(0, 0, 0);
			bounds_hover_handle = -1;

			// Face planes & normals of current bounds:
			const XMFLOAT3 bmin = bounds_world.getMin();
			const XMFLOAT3 bmax = bounds_world.getMax();
			struct Face { XMFLOAT3 n; float d; XMFLOAT3 p0, p1; }; // plane normal, signed D, two opposite corners for bounds test

			// Build 6 faces (axis-aligned):
			Face faces[6] = {
				{ XMFLOAT3(-1,0,0),  bmin.x, XMFLOAT3(bmin.x,bmin.y,bmin.z), XMFLOAT3(bmin.x,bmax.y,bmax.z) }, // X-
				{ XMFLOAT3(1,0,0), -bmax.x, XMFLOAT3(bmax.x,bmin.y,bmin.z), XMFLOAT3(bmax.x,bmax.y,bmax.z) }, // X+
				{ XMFLOAT3(0,-1,0),  bmin.y, XMFLOAT3(bmin.x,bmin.y,bmin.z), XMFLOAT3(bmax.x,bmin.y,bmax.z) }, // Y-
				{ XMFLOAT3(0, 1,0), -bmax.y, XMFLOAT3(bmin.x,bmax.y,bmin.z), XMFLOAT3(bmax.x,bmax.y,bmax.z) }, // Y+
				{ XMFLOAT3(0,0,-1),  bmin.z, XMFLOAT3(bmin.x,bmin.y,bmin.z), XMFLOAT3(bmax.x,bmax.y,bmin.z) }, // Z-
				{ XMFLOAT3(0,0, 1), -bmax.z, XMFLOAT3(bmin.x,bmin.y,bmax.z), XMFLOAT3(bmax.x,bmax.y,bmax.z) }, // Z+
			};

			float best_t = std::numeric_limits<float>::max();
			int   best_i = -1;

			{
				// simple ray-sphere test per face center:
				float best_t = std::numeric_limits<float>::max();
				const float handleR = bounds_handle_radius_factor * dist; // world-space
				const float r2 = handleR * handleR;

				for (int i = 0; i < 6; ++i)
				{
					const XMVECTOR C = XMLoadFloat3(&bounds_face_centers[i]);
					const XMVECTOR m = rayOrigin - C;
					const float b = XMVectorGetX(XMVector3Dot(m, rayDir));
					const float c = XMVectorGetX(XMVector3Dot(m, m)) - r2;
					// If ray origin is outside sphere (c>0) and pointing away (b>0), no hit:
					if (c > 0.0f && b > 0.0f) continue;

					const float discr = b * b - c;
					if (discr < 0.0f) continue;

					float t = -b - std::sqrt(discr);
					if (t < 0) t = 0; // ray starts inside sphere
					if (t < best_t) { best_t = t; bounds_hover_handle = i; }
				}
			}

			for (int i = 0; i < 6; ++i)
			{
				const XMVECTOR N = XMLoadFloat3(&faces[i].n);
				const float denom = XMVectorGetX(XMVector3Dot(N, rayDir));
				if (std::abs(denom) < 1e-4f) continue;

				// Plane point: any corner on that face; we use faces[i].p0
				const XMVECTOR P0 = XMLoadFloat3(&faces[i].p0);
				const float t = XMVectorGetX(XMVector3Dot(N, (P0 - rayOrigin))) / denom;
				if (t <= 0) continue;

				const XMVECTOR hit = rayOrigin + rayDir * t;
				XMFLOAT3 H; XMStoreFloat3(&H, hit);

				// Check if H lies within the face rectangle (other two axes within [min,max])
				if (H.x >= bmin.x - 1e-4f && H.x <= bmax.x + 1e-4f &&
					H.y >= bmin.y - 1e-4f && H.y <= bmax.y + 1e-4f &&
					H.z >= bmin.z - 1e-4f && H.z <= bmax.z + 1e-4f)
				{
					// but constrain to the two axes that define the face:
					int ax = (i < 2) ? 0 : (i < 4 ? 1 : 2); // axis normal
					bool inFace = true;
					if (ax != 0) inFace &= (H.x >= bmin.x - 1e-4f && H.x <= bmax.x + 1e-4f);
					if (ax != 1) inFace &= (H.y >= bmin.y - 1e-4f && H.y <= bmax.y + 1e-4f);
					if (ax != 2) inFace &= (H.z >= bmin.z - 1e-4f && H.z <= bmax.z + 1e-4f);

					if (inFace && t < best_t) { best_t = t; best_i = i; }
				}
			}
			if(bounds_hover_handle >= 0)
			{
				switch (bounds_hover_handle / 2) // 0=>X,1=>Y,2=>Z
				{
				case 0: state = TRANSLATOR_X; break;
				case 1: state = TRANSLATOR_Y; break;
				default: state = TRANSLATOR_Z; break;
				}
				// face normal by handle index:
				static const XMFLOAT3 normals[6] = {
					XMFLOAT3(-1,0,0), XMFLOAT3(1,0,0),
					XMFLOAT3(0,-1,0), XMFLOAT3(0,1,0),
					XMFLOAT3(0,0,-1), XMFLOAT3(0,0,1),
				};
				bounds_drag_normal = normals[bounds_hover_handle];
			}
			else if (best_i >= 0)
			{
				// Map face to translator state (so Draw() can color like axes/planes):
				switch (best_i)
				{
				case 0: case 1: state = TRANSLATOR_X; break; // X- / X+
				case 2: case 3: state = TRANSLATOR_Y; break; // Y- / Y+
				case 4: case 5: state = TRANSLATOR_Z; break; // Z- / Z+
				}
				bounds_drag_normal = faces[best_i].n;

				// Anchor is the opposite face corner (kept fixed while dragging this face):
				bounds_anchor = XMFLOAT3(
					(best_i == 0) ? bmax.x : (best_i == 1) ? bmin.x : (bmin.x + bmax.x) * 0.5f,
					(best_i == 2) ? bmax.y : (best_i == 3) ? bmin.y : (bmin.y + bmax.y) * 0.5f,
					(best_i == 4) ? bmax.z : (best_i == 5) ? bmin.z : (bmin.z + bmax.z) * 0.5f
				);

				bounds_hover_handle = best_i;
			}
			else
			{
				// Center handle (move whole box): reuse XYZ
				if (bounds_world.intersects(ray))
				{
					state = TRANSLATOR_XYZ;
				}
			}
		}

		


		// Begin/continue drag:
		if (dragging || (state != TRANSLATOR_IDLE && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)))
		{
			XMVECTOR planeNormal;
			if (state == TRANSLATOR_XYZ)
			{
				planeNormal = XMVector3Normalize(camera.GetAt());
			}
			else
			{
				XMVECTOR N = XMLoadFloat3(&bounds_drag_normal);
				XMVECTOR wrong = XMVector3Cross(camera.GetAt(), N);
				planeNormal = XMVector3Cross(wrong, N);
			}

			const XMVECTOR planePoint =
				dragging
				? XMLoadFloat3(&bounds_world0.getCenter())
				: XMLoadFloat3(&bounds_world.getCenter());

			const XMVECTOR plane = XMPlaneFromPointNormal(planePoint, planeNormal);

			if (XMVectorGetX(XMVectorAbs(XMVector3Dot(planeNormal, rayDir))) < 0.001f)
			{
				state = TRANSLATOR_IDLE;
				return;
			}

			const XMVECTOR hit = XMPlaneIntersectLine(plane, rayOrigin, rayOrigin + rayDir * camera.zFarP);

			if (!dragging)
			{
				dragStarted = true;
				transform_start = transform;
				matrices_start = matrices_current;  // you already capture these for apply/undo
				bounds_world0 = bounds_world;      // remember original AABB
				bounds_active_handle = (state == TRANSLATOR_XYZ) ? -1 : bounds_hover_handle;

				XMStoreFloat3(&bounds_drag_start_hit, hit);
			}

			
			
			XMFLOAT3 H; XMStoreFloat3(&H, hit);

			if (state == TRANSLATOR_XYZ)
			{
				// Move: offset whole AABB by delta
				XMFLOAT3 delta;
				XMStoreFloat3(&delta, hit - XMLoadFloat3(&bounds_drag_start_hit));

				// snapping:
				if (wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_LCONTROL) || wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_RCONTROL) || bounds_snap_enabled)
				{
					auto s = bounds_snap > 0 ? bounds_snap : 1.0f;
					delta.x = std::round(delta.x / s) * s;
					delta.y = std::round(delta.y / s) * s;
					delta.z = std::round(delta.z / s) * s;
				}

				transform = transform_start;
				transform.Translate(XMLoadFloat3(&delta)); // live preview
			}
			else
			{
				// Face drag: measure signed distance along normal from original center plane:
				const XMVECTOR N = XMLoadFloat3(&bounds_drag_normal);        // true face normal
				const XMVECTOR H0 = XMLoadFloat3(&bounds_drag_start_hit);     // start hit on drag plane
				float d = XMVectorGetX(XMVector3Dot(N, hit - H0));

				// Snap:
				if (wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_LCONTROL) || wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_RCONTROL) || bounds_snap_enabled)
				{
					auto s = bounds_snap > 0 ? bounds_snap : 1.0f;
					d = std::round(d / s) * s;
				}

				// Build new target AABB by sliding one face:
				wi::primitive::AABB target = bounds_world0;
				XMFLOAT3 n; XMStoreFloat3(&n, N);

				// Move the appropriate min/max along that axis:
				if (n.x > 0.5f) target._max.x += d;
				else if (n.x < -0.5f)target._min.x += d;
				else if (n.y > 0.5f) target._max.y += d;
				else if (n.y < -0.5f)target._min.y += d;
				else if (n.z > 0.5f) target._max.z += d;
				else if (n.z < -0.5f)target._min.z += d;

				// Compute scale & translation that maps bounds_world0 to target
				// NOTE: MVP assumes world-aligned (no complex rotation on selection).
				const XMFLOAT3 half0 = bounds_world0.getHalfWidth();
				const XMFLOAT3 s0 = XMFLOAT3(half0.x * 2.0f, half0.y * 2.0f, half0.z * 2.0f);
				const XMFLOAT3 half1 = target.getHalfWidth();
				const XMFLOAT3 s1 = XMFLOAT3(half1.x * 2.0f, half1.y * 2.0f, half1.z * 2.0f);
				
				XMFLOAT3 scale = XMFLOAT3(
					s0.x > 1e-6f ? s1.x / s0.x : 1.0f,
					s0.y > 1e-6f ? s1.y / s0.y : 1.0f,
					s0.z > 1e-6f ? s1.z / s0.z : 1.0f
				);

				XMFLOAT3 c0 = bounds_world0.getCenter();
				XMFLOAT3 c1 = target.getCenter();
				XMFLOAT3 t = XMFLOAT3(c1.x - c0.x, c1.y - c0.y, c1.z - c0.z);

				transform = transform_start;

				// Apply scale in local-axis space:
				// TODO: for rotated selections, decompose & apply in local basis aligned to selection rotation.
				transform.Scale(scale);

				// Then translate:
				transform.Translate(XMLoadFloat3(&t));
			}

			transform.UpdateTransform();
			dragging = true;
			PostTranslate(); // keep parity with your other tools for live updates
		}

		// End drag:
		if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
		{
			if (dragging) { dragEnded = true; /* TODO: push history op if you do that elsewhere */ }
			dragging = false;
			bounds_active_handle = -1;
		}

		return; // <-- Bounds handled; skip the classic gizmo branch
	}

	if (IsEnabled())
	{
		PreTranslate();
		if (!has_selected_transform)
		{
			state = TRANSLATOR_IDLE;
			return;
		}

		const Ray ray = wi::renderer::GetPickRay((long)currentMouse.x, (long)currentMouse.y, canvas, camera);
		const XMVECTOR rayOrigin = XMLoadFloat3(&ray.origin);
		const XMVECTOR rayDir = XMLoadFloat3(&ray.direction);

		if (!dragging)
		{
			state = TRANSLATOR_IDLE;

			// Decide which state to enter for dragging:
			XMMATRIX P = camera.GetProjection();
			XMMATRIX V = camera.GetView();
			XMMATRIX W = XMMatrixIdentity();
			XMFLOAT3 p = transform.GetPosition();

			dist = std::max(wi::math::Distance(p, camera.Eye) * 0.05f, 0.0001f);

			if (isRotator)
			{
				XMVECTOR plane_zy = XMPlaneFromPointNormal(pos, XMVectorSet(1, 0, 0, 0));
				XMVECTOR plane_xz = XMPlaneFromPointNormal(pos, XMVectorSet(0, 1, 0, 0));
				XMVECTOR plane_xy = XMPlaneFromPointNormal(pos, XMVectorSet(0, 0, 1, 0));

				XMVECTOR intersection = XMPlaneIntersectLine(plane_zy, rayOrigin, rayOrigin + rayDir * camera.zFarP);
				float dist_x = XMVectorGetX(XMVector3LengthSq(intersection - rayOrigin));
				float len_x = XMVectorGetX(XMVector3Length(intersection - pos)) / dist;
				intersection = XMPlaneIntersectLine(plane_xz, rayOrigin, rayOrigin + rayDir * camera.zFarP);
				float dist_y = XMVectorGetX(XMVector3LengthSq(intersection - rayOrigin));
				float len_y = XMVectorGetX(XMVector3Length(intersection - pos)) / dist;
				intersection = XMPlaneIntersectLine(plane_xy, rayOrigin, rayOrigin + rayDir * camera.zFarP);
				float dist_z = XMVectorGetX(XMVector3LengthSq(intersection - rayOrigin));
				float len_z = XMVectorGetX(XMVector3Length(intersection - pos)) / dist;

				float range = circle_width * 0.5f;
				float perimeter = circle_radius - range;
				float best_dist = std::numeric_limits<float>::max();
				if (std::abs(perimeter - len_x) <= range && dist_x < best_dist)
				{
					state = TRANSLATOR_X;
					axis = XMFLOAT3(1, 0, 0);
					best_dist = dist_x;
				}
				if (std::abs(perimeter - len_y) <= range && dist_y < best_dist)
				{
					state = TRANSLATOR_Y;
					axis = XMFLOAT3(0, 1, 0);
					best_dist = dist_y;
				}
				if (std::abs(perimeter - len_z) <= range && dist_z < best_dist)
				{
					state = TRANSLATOR_Z;
					axis = XMFLOAT3(0, 0, 1);
					best_dist = dist_z;
				}

				XMVECTOR screen_normal = XMVector3Normalize(camera.GetEye() - pos);
				XMVECTOR plane_screen = XMPlaneFromPointNormal(pos, screen_normal);
				intersection = XMPlaneIntersectLine(plane_screen, rayOrigin, rayOrigin + rayDir * camera.zFarP);
				float len_screen = XMVectorGetX(XMVector3Length(intersection - pos)) / dist;
				range = circle2_width * 0.5f;
				perimeter = circle2_radius - range;
				if (std::abs(perimeter - len_screen) <= range)
				{
					state = TRANSLATOR_XYZ;
					XMStoreFloat3(&axis, screen_normal);
				}
				
			}
			else
			{
				AABB aabb_origin;
				aabb_origin.createFromHalfWidth(p, XMFLOAT3(origin_size * dist, origin_size * dist, origin_size * dist));

				XMFLOAT3 maxp;
				XMStoreFloat3(&maxp, pos + XMVector3Transform(XMVectorSet(axis_length, 0, 0, 0) * dist, GetMirrorMatrix(TRANSLATOR_X, camera)));
				AABB aabb_x = AABB::Merge(AABB(wi::math::Min(p, maxp), wi::math::Max(p, maxp)), aabb_origin);

				XMStoreFloat3(&maxp, pos + XMVector3Transform(XMVectorSet(0, axis_length, 0, 0) * dist, GetMirrorMatrix(TRANSLATOR_Y, camera)));
				AABB aabb_y = AABB::Merge(AABB(wi::math::Min(p, maxp), wi::math::Max(p, maxp)), aabb_origin);

				XMStoreFloat3(&maxp, pos + XMVector3Transform(XMVectorSet(0, 0, axis_length, 0) * dist, GetMirrorMatrix(TRANSLATOR_Z, camera)));
				AABB aabb_z = AABB::Merge(AABB(wi::math::Min(p, maxp), wi::math::Max(p, maxp)), aabb_origin);

				XMFLOAT3 minp;
				XMStoreFloat3(&minp, pos + XMVector3Transform(XMVectorSet(plane_min, plane_min, 0, 0) * dist, GetMirrorMatrix(TRANSLATOR_XY, camera)));
				XMStoreFloat3(&maxp, pos + XMVector3Transform(XMVectorSet(plane_max, plane_max, 0, 0) * dist, GetMirrorMatrix(TRANSLATOR_XY, camera)));
				AABB aabb_xy = AABB(wi::math::Min(minp, maxp), wi::math::Max(minp, maxp));

				XMStoreFloat3(&minp, pos + XMVector3Transform(XMVectorSet(plane_min, 0, plane_min, 0) * dist, GetMirrorMatrix(TRANSLATOR_XZ, camera)));
				XMStoreFloat3(&maxp, pos + XMVector3Transform(XMVectorSet(plane_max, 0, plane_max, 0) * dist, GetMirrorMatrix(TRANSLATOR_XZ, camera)));
				AABB aabb_xz = AABB(wi::math::Min(minp, maxp), wi::math::Max(minp, maxp));

				XMStoreFloat3(&minp, pos + XMVector3Transform(XMVectorSet(0, plane_min, plane_min, 0) * dist, GetMirrorMatrix(TRANSLATOR_YZ, camera)));
				XMStoreFloat3(&maxp, pos + XMVector3Transform(XMVectorSet(0, plane_max, plane_max, 0) * dist, GetMirrorMatrix(TRANSLATOR_YZ, camera)));
				AABB aabb_yz = AABB(wi::math::Min(minp, maxp), wi::math::Max(minp, maxp));

				if (aabb_origin.intersects(ray))
				{
					state = TRANSLATOR_XYZ;
				}
				else if (aabb_x.intersects(ray))
				{
					state = TRANSLATOR_X;
				}
				else if (aabb_y.intersects(ray))
				{
					state = TRANSLATOR_Y;
				}
				else if (aabb_z.intersects(ray))
				{
					state = TRANSLATOR_Z;
				}

				if (isTranslator && state != TRANSLATOR_XYZ)
				{
					// these can overlap, so take closest one (by checking plane ray trace distance):
					XMVECTOR N = XMVectorSet(0, 0, 1, 0);

					float prio = FLT_MAX;
					if (aabb_xy.intersects(ray))
					{
						state = TRANSLATOR_XY;
						prio = XMVectorGetX(XMVector3Dot(N, (rayOrigin - pos) / XMVectorAbs(XMVector3Dot(N, rayDir))));
					}

					N = XMVectorSet(0, 1, 0, 0);
					float d = XMVectorGetX(XMVector3Dot(N, (rayOrigin - pos) / XMVectorAbs(XMVector3Dot(N, rayDir))));
					if (d < prio && aabb_xz.intersects(ray))
					{
						state = TRANSLATOR_XZ;
						prio = d;
					}

					N = XMVectorSet(1, 0, 0, 0);
					d = XMVectorGetX(XMVector3Dot(N, (rayOrigin - pos) / XMVectorAbs(XMVector3Dot(N, rayDir))));
					if (d < prio && aabb_yz.intersects(ray))
					{
						state = TRANSLATOR_YZ;
					}
				}
			}
		}

		if (dragging || (state != TRANSLATOR_IDLE && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)))
		{
			// Dragging operation:
			if (isRotator)
			{
				XMVECTOR intersection = XMPlaneIntersectLine(XMPlaneFromPointNormal(pos, XMLoadFloat3(&axis)), rayOrigin, rayOrigin + rayDir * camera.zFarP);

				if (!dragging)
				{
					dragStarted = true;
					transform_start = transform;
					XMStoreFloat3(&intersection_start, intersection);
					matrices_start = matrices_current;
				}
				XMVECTOR intersectionPrev = XMLoadFloat3(&intersection_start);

				XMVECTOR o = XMVector3Normalize(intersectionPrev - pos);
				XMVECTOR c = XMVector3Normalize(intersection - pos);
				XMFLOAT3 original, current;
				XMStoreFloat3(&original, o);
				XMStoreFloat3(&current, c);
				angle = wi::math::GetAngle(original, current, axis);

				switch (state)
				{
				case Translator::TRANSLATOR_X:
					angle_start = wi::math::GetAngle(XMFLOAT3(0, 1, 0), original, axis);
					break;
				case Translator::TRANSLATOR_Y:
					angle_start = wi::math::GetAngle(XMFLOAT3(0, 0, 1), original, axis);
					break;
				case Translator::TRANSLATOR_Z:
					angle_start = wi::math::GetAngle(XMFLOAT3(1, 0, 0), original, axis);
					break;
				case Translator::TRANSLATOR_XYZ:
					{
						XMMATRIX M = XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(), XMVector3Normalize(transform.GetPositionV() - camera.GetEye()), camera.GetUp()));
						XMFLOAT3 ref;
						XMStoreFloat3(&ref, XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), M));
						angle_start = wi::math::GetAngle(ref, original, axis);
					}
					break;
				default:
					break;
				}

				if (wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_LCONTROL))
				{
					// Snap mode:
					angle = std::round(angle / rotate_snap) * rotate_snap;
				}
				angle = std::fmod(angle, XM_2PI);

				transform = transform_start;
				transform.Rotate(XMQuaternionRotationAxis(XMLoadFloat3(&axis), angle));
			}
			else
			{
				XMVECTOR plane, planeNormal;
				if (state == TRANSLATOR_X)
				{
					XMVECTOR axis = XMVectorSet(1, 0, 0, 0);
					XMVECTOR wrong = XMVector3Cross(camera.GetAt(), axis);
					planeNormal = XMVector3Cross(wrong, axis);
					this->axis = XMFLOAT3(1, 0, 0);
				}
				else if (state == TRANSLATOR_Y)
				{
					XMVECTOR axis = XMVectorSet(0, 1, 0, 0);
					XMVECTOR wrong = XMVector3Cross(camera.GetAt(), axis);
					planeNormal = XMVector3Cross(wrong, axis);
					this->axis = XMFLOAT3(0, 1, 0);
				}
				else if (state == TRANSLATOR_Z)
				{
					XMVECTOR axis = XMVectorSet(0, 0, 1, 0);
					XMVECTOR wrong = XMVector3Cross(camera.GetAt(), axis);
					planeNormal = XMVector3Cross(wrong, axis);
					this->axis = XMFLOAT3(0, 0, 1);
				}
				else if (state == TRANSLATOR_XY)
				{
					planeNormal = XMVectorSet(0, 0, 1, 0);
				}
				else if (state == TRANSLATOR_XZ)
				{
					planeNormal = XMVectorSet(0, 1, 0, 0);
				}
				else if (state == TRANSLATOR_YZ)
				{
					planeNormal = XMVectorSet(1, 0, 0, 0);
				}
				else
				{
					// xyz
					planeNormal = camera.GetAt();
				}
				plane = XMPlaneFromPointNormal(pos, XMVector3Normalize(planeNormal));

				if (XMVectorGetX(XMVectorAbs(XMVector3Dot(planeNormal, rayDir))) < 0.001f)
				{
					state = TRANSLATOR_IDLE;
					return;
				}
				XMVECTOR intersection = XMPlaneIntersectLine(plane, rayOrigin, rayOrigin + rayDir * camera.zFarP);

				if (!dragging)
				{
					dragStarted = true;
					transform_start = transform;
					XMStoreFloat3(&intersection_start, intersection);
					matrices_start = matrices_current;
				}
				XMVECTOR intersectionPrev = XMLoadFloat3(&intersection_start);

				XMVECTOR deltaV;
				if (state == TRANSLATOR_X)
				{
					XMVECTOR A = pos, B = pos + XMVectorSet(1, 0, 0, 0);
					XMVECTOR P = wi::math::ClosestPointOnLine(A, B, intersection);
					XMVECTOR PPrev = wi::math::ClosestPointOnLine(A, B, intersectionPrev);
					deltaV = P - PPrev;
				}
				else if (state == TRANSLATOR_Y)
				{
					XMVECTOR A = pos, B = pos + XMVectorSet(0, 1, 0, 0);
					XMVECTOR P = wi::math::ClosestPointOnLine(A, B, intersection);
					XMVECTOR PPrev = wi::math::ClosestPointOnLine(A, B, intersectionPrev);
					deltaV = P - PPrev;
				}
				else if (state == TRANSLATOR_Z)
				{
					XMVECTOR A = pos, B = pos + XMVectorSet(0, 0, 1, 0);
					XMVECTOR P = wi::math::ClosestPointOnLine(A, B, intersection);
					XMVECTOR PPrev = wi::math::ClosestPointOnLine(A, B, intersectionPrev);
					deltaV = P - PPrev;
				}
				else
				{
					deltaV = intersection - intersectionPrev;

					if (isScalator)
					{
						deltaV = XMVectorReplicate(XMVectorGetY(deltaV));
					}
				}

				transform = transform_start;
				if (isTranslator)
				{
					transform.Translate(deltaV);
				}
				if (isScalator)
				{
					deltaV = XMVector3TransformNormal(deltaV, GetMirrorMatrix(state, camera));
					deltaV = XMVector3Rotate(deltaV, transform.GetRotationV());
					XMFLOAT3 delta;
					XMStoreFloat3(&delta, deltaV);
					XMFLOAT3 scale = transform.GetScale();
					scale = wi::math::Max(scale, XMFLOAT3(0.001f, 0.001f, 0.001f)); // no zero division
					scale = XMFLOAT3((1.0f / scale.x) * (scale.x + delta.x), (1.0f / scale.y) * (scale.y + delta.y), (1.0f / scale.z) * (scale.z + delta.z));
					transform.Scale(scale);
				}

				if (wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_LCONTROL) || wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_RCONTROL))
				{
					// Snap to grid mode:
					if (isTranslator)
					{
						transform.translation_local.x = std::round(transform.translation_local.x / translate_snap) * translate_snap;
						transform.translation_local.y = std::round(transform.translation_local.y / translate_snap) * translate_snap;
						transform.translation_local.z = std::round(transform.translation_local.z / translate_snap) * translate_snap;
					}
					if (isScalator)
					{
						transform.scale_local.x = std::max(scale_snap, std::round(transform.scale_local.x / scale_snap) * scale_snap);
						transform.scale_local.y = std::max(scale_snap, std::round(transform.scale_local.y / scale_snap) * scale_snap);
						transform.scale_local.z = std::max(scale_snap, std::round(transform.scale_local.z / scale_snap) * scale_snap);
					}
				}
				if (wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_LSHIFT) || wi::input::Down(wi::input::BUTTON::KEYBOARD_BUTTON_RSHIFT))
				{
					// Snap to surface mode:

					// 1.) Collect all objects which could be in the hierarchy of any of the selected ones
					//	This is important because we could select a top parent that doesn't have object
					//	but I still want to disable children's filters
					selectedWithHierarchy.clear();
					for (size_t i = 0; i < scene.objects.GetCount(); ++i)
					{
						Entity entity = scene.objects.GetEntity(i);
						for (auto& potential_parent : selectedEntitiesNonRecursive)
						{
							if (entity == potential_parent || scene.Entity_IsDescendant(entity, potential_parent))
							{
								selectedWithHierarchy.push_back(entity);
								break;
							}
						}
					}

					// 2.) Disable all object filters which are part of selection hierarchy for the next picking
					//	This is to filter them out from picking, we only want to pick what's underneath
					temp_filters.reserve(selectedWithHierarchy.size());
					for (auto& entity : selectedWithHierarchy)
					{
						ObjectComponent* object = scene.objects.GetComponent(entity);
						if (object == nullptr)
							continue;
						temp_filters.push_back(uint64_t(object->filterMask) | (uint64_t(object->filterMaskDynamic) << 32ull));
						object->filterMask = 0;
						object->filterMaskDynamic = 0;
					}

					// 3.) Pick into scene to determine placement position
					Ray ray = wi::renderer::GetPickRay((long)currentMouse.x, (long)currentMouse.y, canvas, camera);
					wi::scene::Scene::RayIntersectionResult result = scene.Intersects(ray, wi::enums::FILTER_OBJECT_ALL);
					transform.translation_local = result.position;

					// 4.) Restore all filters to original values
					size_t ind = 0;
					for (auto& entity : selectedWithHierarchy)
					{
						ObjectComponent* object = scene.objects.GetComponent(entity);
						if (object == nullptr)
							continue;
						uint64_t tmp = temp_filters[ind++];
						object->filterMask = uint32_t(tmp);
						object->filterMaskDynamic = uint32_t(tmp >> 32ull);
					}
				}
			}

			transform.UpdateTransform();

			dragging = true;
		}

		if (isScalator || isRotator || isTranslator)
		{
			if (dragging)
			{
				PostTranslate();
			}
		}
		
		if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
		{
			if (dragging)
			{
				dragEnded = true;
			}
			dragging = false;
		}
	}
	else
	{
		if (dragging)
		{
			dragEnded = true;
		}
		dragging = false;
		state = TRANSLATOR_IDLE;
	}
}
void Translator::Draw(const CameraComponent& camera, const XMFLOAT4& currentMouse, CommandList cmd) const
{
	if (!IsEnabled() || selected.empty() || !has_selected_transform)
	{
		return;
	}

	if (isBoundSizer)
	{
		static bool shaders_loaded = false;
		if (!shaders_loaded)
		{
			shaders_loaded = true;
			static wi::eventhandler::Handle handle = wi::eventhandler::Subscribe(
				wi::eventhandler::EVENT_RELOAD_SHADERS,
				[](uint64_t) { Translator_Internal::LoadShaders(); }
			);
			Translator_Internal::LoadShaders();
		}

		GraphicsDevice* device = wi::graphics::GetDevice();
		device->EventBegin("BoundsSizer", cmd);

		// Draw wireframe AABB of current selection:
		wi::primitive::AABB aabb = bounds_world;
		XMFLOAT3 bmin = aabb.getMin();
		XMFLOAT3 bmax = aabb.getMax();

		// Make 12 edges as line list (8 corners -> 12 edges):
		XMFLOAT3 c[8] = {
			{bmin.x,bmin.y,bmin.z}, {bmax.x,bmin.y,bmin.z},
			{bmin.x,bmax.y,bmin.z}, {bmax.x,bmax.y,bmin.z},
			{bmin.x,bmin.y,bmax.z}, {bmax.x,bmin.y,bmax.z},
			{bmin.x,bmax.y,bmax.z}, {bmax.x,bmax.y,bmax.z},
		};
		uint16_t edges[24] = {
			0,1, 1,3, 3,2, 2,0, // bottom rectangle
			4,5, 5,7, 7,6, 6,4, // top rectangle
			0,4, 1,5, 2,6, 3,7  // verticals
		};

		// Allocate and draw:
		device->BindPipelineState(&pso_wirepart, cmd);

		struct Vtx { XMFLOAT4 p; XMFLOAT4 c; };
		Vtx v[24];
		for (int i = 0; i < 12; ++i) {
			v[i * 2 + 0] = { XMFLOAT4(c[edges[i * 2 + 0]].x, c[edges[i * 2 + 0]].y, c[edges[i * 2 + 0]].z, 1), XMFLOAT4(1,1,1,1) };
			v[i * 2 + 1] = { XMFLOAT4(c[edges[i * 2 + 1]].x, c[edges[i * 2 + 1]].y, c[edges[i * 2 + 1]].z, 1), XMFLOAT4(1,1,1,1) };
		}

		auto mem = device->AllocateGPU(sizeof(v), cmd);
		std::memcpy(mem.data, v, sizeof(v));

		const GPUBuffer* vbs[] = { &mem.buffer };
		const uint32_t   strides[] = { sizeof(Vtx) };
		const uint64_t   offsets[] = { mem.offset };
		device->BindVertexBuffers(vbs, 0, 1, strides, offsets, cmd);

		MiscCB sb;
		XMMATRIX VP = camera.GetViewProjection(); // note: you already recompute a jitter-free VP earlier
		XMStoreFloat4x4(&sb.g_xTransform, VP);    // feed world verts directly, so transform is just VP
		sb.g_xColor = XMFLOAT4(1, 1, 1, opacity);
		device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
		device->Draw(24, 0, cmd);

		// --- Face handle discs (six small circles) ---
		{
			device->BindPipelineState(&pso_solidpart, cmd);

			const uint32_t segmentCount = 36;
			const uint32_t vertexCount = segmentCount * 3; // triangle fan strip per circle
			const float handleR = bounds_handle_radius_factor * dist;

			for (int i = 0; i < 6; ++i)
			{
				// Color by axis family; boost alpha if active/hovered:
				XMFLOAT4 col;
				switch (i / 2) { // 0=>X,1=>Y,2=>Z
				case 0: col = XMFLOAT4(1, 0, 0, 0.35f); break;
				case 1: col = XMFLOAT4(0, 1, 0, 0.35f); break;
				default:col = XMFLOAT4(0, 0, 1, 0.35f); break;
				}
				if (i == bounds_hover_handle) col.w = 0.55f;
				if (i == bounds_active_handle) col.w = 0.75f;
				col.w *= opacity;

				// Allocate CPU-side vertex memory for this disc:
				GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(Translator_Internal::Vertex) * vertexCount, cmd);
				uint8_t* dst = (uint8_t*)mem.data;

				// Build a small screen-facing triangle fan at origin (we'll transform it into place by matrix below):
				for (uint32_t s = 0; s < segmentCount; ++s)
				{
					const float a0 = (float)s / (float)segmentCount * XM_2PI;
					const float a1 = (float)(s + 1) / (float)segmentCount * XM_2PI;

					const Translator_Internal::Vertex verts[] = {
						{ XMFLOAT4(0, 0, 0, 1),                           XMFLOAT4(1,1,1,1) },
						{ XMFLOAT4(0, std::cos(a0) * handleR, std::sin(a0) * handleR, 1), XMFLOAT4(1,1,1,1) },
						{ XMFLOAT4(0, std::cos(a1) * handleR, std::sin(a1) * handleR, 1), XMFLOAT4(1,1,1,1) },
					};
					std::memcpy(dst, verts, sizeof(verts));
					dst += sizeof(verts);
				}

				const GPUBuffer* vbs[] = { &mem.buffer };
				const uint32_t strides[] = { sizeof(Translator_Internal::Vertex) };
				const uint64_t offsets[] = { mem.offset };
				device->BindVertexBuffers(vbs, 0, 1, strides, offsets, cmd);

				// Make the disc face the camera and sit at the face center:
				// (This is the same “billboard circle” pattern you use for the rotator screen-facing circle)
				XMMATRIX toScreenFacing =
					XMMatrixRotationY(XM_PIDIV2) *
					XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(),
						XMVector3Normalize(XMLoadFloat3(&bounds_face_centers[i]) - camera.GetEye()),
						camera.GetUp()));

				MiscCB sb;
				XMMATRIX VP = camera.GetViewProjection(); // you already use jitter-free VP above
				XMStoreFloat4x4(&sb.g_xTransform,
					toScreenFacing *
					XMMatrixTranslation(bounds_face_centers[i].x, bounds_face_centers[i].y, bounds_face_centers[i].z) *
					VP);
				sb.g_xColor = col;
				device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
				device->Draw(vertexCount, 0, cmd);
			}
		}

		// Optional: draw subtle filled quads on faces when hovered (using pso_solidpart) and color by state
		// TODO: face “hot” coloring based on `state` (X/Y/Z → red/green/blue highlight like your other gizmos)

		device->EventEnd(cmd);
		return; // Bounds gizmo drawn; skip classic gizmo rendering
	}

	static bool shaders_loaded = false;
	if (!shaders_loaded)
	{
		shaders_loaded = true;
		static wi::eventhandler::Handle handle = wi::eventhandler::Subscribe(wi::eventhandler::EVENT_RELOAD_SHADERS, [](uint64_t userdata) { Translator_Internal::LoadShaders(); });
		Translator_Internal::LoadShaders();
	}

	Scene& scene = *this->scene;

	GraphicsDevice* device = wi::graphics::GetDevice();

	device->EventBegin("Translator", cmd);

	CameraComponent cam_tmp = camera;
	cam_tmp.jitter = XMFLOAT2(0, 0); // remove temporal jitter
	cam_tmp.UpdateCamera();
	XMMATRIX VP = cam_tmp.GetViewProjection();

	MiscCB sb;

	XMMATRIX mat = XMMatrixScaling(dist, dist, dist)*XMMatrixTranslationFromVector(transform.GetPositionV()) * VP;
	XMMATRIX matX = XMMatrixIdentity();
	XMMATRIX matY = XMMatrixRotationZ(XM_PIDIV2)*XMMatrixRotationY(XM_PIDIV2);
	XMMATRIX matZ = XMMatrixRotationY(-XM_PIDIV2)*XMMatrixRotationZ(-XM_PIDIV2);

	const float channel_min = 0.25f; // min color channel, to avoid pure red/green/blue
	const XMFLOAT4 highlight_color = XMFLOAT4(1, 0.6f, 0, 1);

	// Axes:
	{
		device->BindPipelineState(&pso_solidpart, cmd);

		uint32_t vertexCount = 0;
		GraphicsDevice::GPUAllocation mem;

		if (isRotator)
		{
			const uint32_t segmentCount = 90;
			const uint32_t circle_triangleCount = segmentCount * 2;
			vertexCount = circle_triangleCount * 3;
			mem = device->AllocateGPU(sizeof(Vertex) * vertexCount, cmd);
			uint8_t* dst = (uint8_t*)mem.data;
			for (uint32_t i = 0; i < segmentCount; ++i)
			{
				const float angle0 = (float)i / (float)segmentCount * XM_2PI;
				const float angle1 = (float)(i + 1) / (float)segmentCount * XM_2PI;

				// circle:
				const float circle_radius_inner = circle_radius - circle_width;
				const float circle_halfway = circle_radius - circle_width * 0.5f;
				const Vertex verts[] = {
					{XMFLOAT4(0, std::sin(angle0) * circle_radius_inner, std::cos(angle0) * circle_radius_inner, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::sin(angle1) * circle_radius_inner, std::cos(angle1) * circle_radius_inner, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::sin(angle0) * circle_radius, std::cos(angle0) * circle_radius, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::sin(angle0) * circle_radius, std::cos(angle0) * circle_radius, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::sin(angle1) * circle_radius, std::cos(angle1) * circle_radius, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::sin(angle1) * circle_radius_inner, std::cos(angle1) * circle_radius_inner, 1), XMFLOAT4(1,1,1,1)},
				};
				std::memcpy(dst, verts, sizeof(verts));
				dst += sizeof(verts);
			}
		}
		else
		{
			const uint32_t segmentCount = 18;
			const uint32_t cylinder_triangleCount = segmentCount * 2;
			const uint32_t cone_triangleCount = cylinder_triangleCount;
			if (isTranslator)
			{
				vertexCount = (cylinder_triangleCount + cone_triangleCount) * 3;
			}
			else if (isScalator)
			{
				vertexCount = cylinder_triangleCount * 3 + arraysize(cubeVerts);
			}
			mem = device->AllocateGPU(sizeof(Vertex) * vertexCount, cmd);

			const float cone_length = 0.75f;
			float cylinder_length = axis_length;
			if (isTranslator)
			{
				cylinder_length -= cone_length;
			}
			uint8_t* dst = (uint8_t*)mem.data;
			for (uint32_t i = 0; i < segmentCount; ++i)
			{
				const float angle0 = (float)i / (float)segmentCount * XM_2PI;
				const float angle1 = (float)(i + 1) / (float)segmentCount * XM_2PI;
				// cylinder base:
				{
					const float cylinder_radius = 0.075f;
					const Vertex verts[] = {
						{XMFLOAT4(origin_size, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(origin_size, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(origin_size, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), XMFLOAT4(1,1,1,1)},
					};
					std::memcpy(dst, verts, sizeof(verts));
					dst += sizeof(verts);
				}
				if (isTranslator)
				{
					// cone cap:
					const float cone_radius = origin_size;
					const Vertex verts[] = {
						{XMFLOAT4(cylinder_length, 0, 0, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle0) * cone_radius, std::cos(angle0) * cone_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle1) * cone_radius, std::cos(angle1) * cone_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(axis_length, 0, 0, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle0) * cone_radius, std::cos(angle0) * cone_radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(cylinder_length, std::sin(angle1) * cone_radius, std::cos(angle1) * cone_radius, 1), XMFLOAT4(1,1,1,1)},
					};
					std::memcpy(dst, verts, sizeof(verts));
					dst += sizeof(verts);
				}
			}

			if (isScalator)
			{
				// cube cap:
				for (uint32_t i = 0; i < arraysize(cubeVerts); ++i)
				{
					Vertex vert = cubeVerts[i];
					vert.position.x = vert.position.x * origin_size + cylinder_length - origin_size;
					vert.position.y = vert.position.y * origin_size;
					vert.position.z = vert.position.z * origin_size;
					std::memcpy(dst, &vert, sizeof(vert));
					dst += sizeof(vert);
				}
			}
		}

		const GPUBuffer* vbs[] = {
			&mem.buffer,
		};
		const uint32_t strides[] = {
			sizeof(Vertex),
		};
		const uint64_t offsets[] = {
			mem.offset,
		};
		device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

		float darken = 1;

		// x
		XMStoreFloat4x4(&sb.g_xTransform, matX * GetMirrorMatrix(TRANSLATOR_X, camera) * mat);
		darken = camera.Eye.x < transform.translation_local.x ? darken_negative_axes : 1;
		sb.g_xColor = state == TRANSLATOR_X ? highlight_color : XMFLOAT4(darken, channel_min * darken, channel_min * darken, 1);
		sb.g_xColor.w *= opacity;
		device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
		device->Draw(vertexCount, 0, cmd);

		// y
		XMStoreFloat4x4(&sb.g_xTransform, matY * GetMirrorMatrix(TRANSLATOR_Y, camera)* mat);
		darken = camera.Eye.y < transform.translation_local.y ? darken_negative_axes : 1;
		sb.g_xColor = state == TRANSLATOR_Y ? highlight_color : XMFLOAT4(channel_min * darken, darken, channel_min * darken, 1);
		sb.g_xColor.w *= opacity;
		device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
		device->Draw(vertexCount, 0, cmd);

		// z
		XMStoreFloat4x4(&sb.g_xTransform, matZ * GetMirrorMatrix(TRANSLATOR_Z, camera)* mat);
		darken = camera.Eye.z < transform.translation_local.z ? darken_negative_axes : 1;
		sb.g_xColor = state == TRANSLATOR_Z ? highlight_color : XMFLOAT4(channel_min * darken, channel_min * darken, darken, 1);
		sb.g_xColor.w *= opacity;
		device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
		device->Draw(vertexCount, 0, cmd);

	}

	if (isRotator)
	{
		// An other circle for rotator, a bit thinner and screen facing, so new geo:
		const uint32_t segmentCount = 90;
		const uint32_t circle2_triangleCount = segmentCount * 2;
		uint32_t vertexCount = circle2_triangleCount * 3;
		GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(Vertex) * vertexCount, cmd);
		uint8_t* dst = (uint8_t*)mem.data;
		for (uint32_t i = 0; i < segmentCount; ++i)
		{
			const float angle0 = (float)i / (float)segmentCount * XM_2PI;
			const float angle1 = (float)(i + 1) / (float)segmentCount * XM_2PI;

			// circle:
			const float circle2_radius_inner = circle2_radius - circle2_width;
			const float circle2_halfway = circle2_radius - circle2_width * 0.5f;
			const Vertex verts[] = {
				{XMFLOAT4(0, std::sin(angle0) * circle2_radius_inner, std::cos(angle0) * circle2_radius_inner, 1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(0, std::sin(angle1) * circle2_radius_inner, std::cos(angle1) * circle2_radius_inner, 1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(0, std::sin(angle0) * circle2_radius, std::cos(angle0) * circle2_radius, 1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(0, std::sin(angle0) * circle2_radius, std::cos(angle0) * circle2_radius, 1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(0, std::sin(angle1) * circle2_radius, std::cos(angle1) * circle2_radius, 1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(0, std::sin(angle1) * circle2_radius_inner, std::cos(angle1) * circle2_radius_inner, 1), XMFLOAT4(1,1,1,1)},
			};
			std::memcpy(dst, verts, sizeof(verts));
			dst += sizeof(verts);
		}

		const GPUBuffer* vbs[] = {
			&mem.buffer,
		};
		const uint32_t strides[] = {
			sizeof(Vertex),
		};
		const uint64_t offsets[] = {
			mem.offset,
		};
		device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

		XMStoreFloat4x4(&sb.g_xTransform,
			XMMatrixRotationY(XM_PIDIV2) *
			XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(), XMVector3Normalize(transform.GetPositionV() - camera.GetEye()), camera.GetUp())) *
			mat
		);
		sb.g_xColor = state == TRANSLATOR_XYZ ? highlight_color : XMFLOAT4(1, 1, 1, 0.5f);
		sb.g_xColor.w *= opacity;
		device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
		device->Draw(vertexCount, 0, cmd);
	}

	// Origin:
	if(!isRotator)
	{
		device->BindPipelineState(&pso_solidpart, cmd);

		GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(cubeVerts), cmd);
		std::memcpy(mem.data, cubeVerts, sizeof(cubeVerts));
		const GPUBuffer* vbs[] = {
			&mem.buffer,
		};
		const uint32_t strides[] = {
			sizeof(Vertex),
		};
		const uint64_t offsets[] = {
			mem.offset,
		};
		device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

		XMStoreFloat4x4(&sb.g_xTransform, XMMatrixScaling(origin_size, origin_size, origin_size) * mat);
		sb.g_xColor = state == TRANSLATOR_XYZ ? highlight_color : XMFLOAT4(0.5f, 0.5f, 0.5f, 1);
		sb.g_xColor.w *= opacity;
		device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
		device->Draw(arraysize(cubeVerts), 0, cmd);
	}

	// Planes:
	if (isTranslator)
	{
		// Wire part:
		{
			device->BindPipelineState(&pso_wirepart, cmd);

			const Vertex verts[] = {
				{XMFLOAT4(plane_min,plane_min,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_min,plane_max,0,1), XMFLOAT4(1,1,1,1)},

				{XMFLOAT4(plane_min,plane_max,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_max,plane_max,0,1), XMFLOAT4(1,1,1,1)},

				{XMFLOAT4(plane_max,plane_max,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_max,plane_min,0,1), XMFLOAT4(1,1,1,1)},

				{XMFLOAT4(plane_max,plane_min,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_min,plane_min,0,1), XMFLOAT4(1,1,1,1)},
			};
			GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(verts), cmd);
			std::memcpy(mem.data, verts, sizeof(verts));
			const GPUBuffer* vbs[] = {
				&mem.buffer,
			};
			const uint32_t strides[] = {
				sizeof(Vertex),
			};
			const uint64_t offsets[] = {
				mem.offset,
			};
			device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

			// xy
			XMStoreFloat4x4(&sb.g_xTransform, matX * GetMirrorMatrix(TRANSLATOR_XY, camera) * mat);
			sb.g_xColor = state == TRANSLATOR_XY ? highlight_color : XMFLOAT4(channel_min, channel_min, 1, 1);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(arraysize(verts), 0, cmd);

			// xz
			XMStoreFloat4x4(&sb.g_xTransform, matZ * GetMirrorMatrix(TRANSLATOR_XZ, camera) * mat);
			sb.g_xColor = state == TRANSLATOR_XZ ? highlight_color : XMFLOAT4(channel_min, 1, channel_min, 1);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(arraysize(verts), 0, cmd);

			// yz
			XMStoreFloat4x4(&sb.g_xTransform, matY * GetMirrorMatrix(TRANSLATOR_YZ, camera) * mat);
			sb.g_xColor = state == TRANSLATOR_YZ ? highlight_color : XMFLOAT4(1, channel_min, channel_min, 1);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(arraysize(verts), 0, cmd);
		}

		// Quad part:
		{
			device->BindPipelineState(&pso_solidpart, cmd);

			const Vertex verts[] = {
				{XMFLOAT4(plane_min,plane_min,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_max,plane_min,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_max,plane_max,0,1), XMFLOAT4(1,1,1,1)},

				{XMFLOAT4(plane_min,plane_min,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_max,plane_max,0,1), XMFLOAT4(1,1,1,1)},
				{XMFLOAT4(plane_min,plane_max,0,1), XMFLOAT4(1,1,1,1)},
			};
			GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(verts), cmd);
			std::memcpy(mem.data, verts, sizeof(verts));
			const GPUBuffer* vbs[] = {
				&mem.buffer,
			};
			const uint32_t strides[] = {
				sizeof(Vertex),
			};
			const uint64_t offsets[] = {
				mem.offset,
			};
			device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

			// xy
			XMStoreFloat4x4(&sb.g_xTransform, matX * GetMirrorMatrix(TRANSLATOR_XY, camera) * mat);
			sb.g_xColor = state == TRANSLATOR_XY ? highlight_color : XMFLOAT4(channel_min, channel_min, 1, 0.4f);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(arraysize(verts), 0, cmd);

			// xz
			XMStoreFloat4x4(&sb.g_xTransform, matZ * GetMirrorMatrix(TRANSLATOR_XZ, camera) * mat);
			sb.g_xColor = state == TRANSLATOR_XZ ? highlight_color : XMFLOAT4(channel_min, 1, channel_min, 0.4f);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(arraysize(verts), 0, cmd);

			// yz
			XMStoreFloat4x4(&sb.g_xTransform, matY * GetMirrorMatrix(TRANSLATOR_YZ, camera) * mat);
			sb.g_xColor = state == TRANSLATOR_YZ ? highlight_color : XMFLOAT4(1, channel_min, channel_min, 0.4f);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(arraysize(verts), 0, cmd);
		}
	}


	// Axis texts:
	if(!isRotator)
	{
		char TEXT[3];
		XMMATRIX R = XMLoadFloat3x3(&camera.rotationMatrix);

		wi::font::Params params;
		params.v_align = wi::font::WIFALIGN_CENTER;
		params.h_align = wi::font::WIFALIGN_CENTER;
		params.scaling = 0.04f * dist;
		params.customProjection = &VP;
		params.customRotation = &R;
		params.shadowColor = wi::Color(0, 0, 0, uint8_t(127 * opacity));
		params.shadow_softness = 0.8f;
		XMVECTOR pos = transform.GetPositionV();

		float darken = 1;
		darken = camera.Eye.x < transform.translation_local.x ? darken_negative_axes : 1;
		params.color = wi::Color::fromFloat4(XMFLOAT4(darken, channel_min * darken, channel_min * darken, opacity));
		XMStoreFloat3(&params.position, pos + XMVector3Transform(XMVectorSet(axis_length + 0.5f, 0, 0, 0) * dist, GetMirrorMatrix(TRANSLATOR_X, camera)));
		std::memset(TEXT, 0, sizeof(TEXT));
		WriteAxisText(TRANSLATOR_X, camera, TEXT);
		wi::font::Draw(TEXT, strlen(TEXT), params, cmd);

		darken = camera.Eye.y < transform.translation_local.y ? darken_negative_axes : 1;
		params.color = wi::Color::fromFloat4(XMFLOAT4(channel_min * darken, darken, channel_min * darken, opacity));
		XMStoreFloat3(&params.position, pos + XMVector3Transform(XMVectorSet(0, axis_length + 0.5f, 0, 0) * dist, GetMirrorMatrix(TRANSLATOR_Y, camera)));
		std::memset(TEXT, 0, sizeof(TEXT));
		WriteAxisText(TRANSLATOR_Y, camera, TEXT);
		wi::font::Draw(TEXT, strlen(TEXT), params, cmd);

		darken = camera.Eye.z < transform.translation_local.z ? darken_negative_axes : 1;
		params.color = wi::Color::fromFloat4(XMFLOAT4(channel_min * darken, channel_min * darken, darken, opacity));
		XMStoreFloat3(&params.position, pos + XMVector3Transform(XMVectorSet(0, 0, axis_length + 0.5f, 0) * dist, GetMirrorMatrix(TRANSLATOR_Z, camera)));
		std::memset(TEXT, 0, sizeof(TEXT));
		WriteAxisText(TRANSLATOR_Z, camera, TEXT);
		wi::font::Draw(TEXT, strlen(TEXT), params, cmd);
	}


	// Dragging visualizer:
	if (dragging)
	{
		if (isTranslator)
		{
			// Origin circle:
			{
				device->BindPipelineState(&pso_solidpart, cmd);

				const uint32_t segmentCount = 36;
				const uint32_t vertexCount = segmentCount * 3;
				GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(Vertex) * vertexCount, cmd);

				uint8_t* dst = (uint8_t*)mem.data;
				for (uint32_t i = 0; i < segmentCount; ++i)
				{
					const float angle0 = (float)i / (float)segmentCount * XM_2PI;
					const float angle1 = (float)(i + 1) / (float)segmentCount * XM_2PI;

					const float radius = 0.2f * dist;
					const Vertex verts[] = {
						{XMFLOAT4(0, 0, 0, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(0, std::cos(angle0) * radius, std::sin(angle0) * radius, 1), XMFLOAT4(1,1,1,1)},
						{XMFLOAT4(0, std::cos(angle1) * radius, std::sin(angle1) * radius, 1), XMFLOAT4(1,1,1,1)},
					};
					std::memcpy(dst, verts, sizeof(verts));
					dst += sizeof(verts);
				}

				const GPUBuffer* vbs[] = {
					&mem.buffer,
				};
				const uint32_t strides[] = {
					sizeof(Vertex),
				};
				const uint64_t offsets[] = {
					mem.offset,
				};
				device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

				XMStoreFloat4x4(&sb.g_xTransform,
					XMMatrixRotationY(XM_PIDIV2)*
					XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(), XMVector3Normalize(transform_start.GetPositionV() - camera.GetEye()), camera.GetUp()))*
					XMMatrixTranslationFromVector(transform_start.GetPositionV())*
					VP
				);
				sb.g_xColor = XMFLOAT4(1, 1, 1, 0.5f);
				sb.g_xColor.w *= opacity;
				device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
				device->Draw(vertexCount, 0, cmd);
			}

			// Line:
			{
				device->BindPipelineState(&pso_wirepart, cmd);
				const Vertex verts[] = {
					{XMFLOAT4(transform_start.translation_local.x, transform_start.translation_local.y, transform_start.translation_local.z, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(transform.translation_local.x, transform.translation_local.y, transform.translation_local.z, 1), XMFLOAT4(1,1,1,1)},
				};
				GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(verts), cmd);
				std::memcpy(mem.data, verts, sizeof(verts));
				const GPUBuffer* vbs[] = {
					&mem.buffer,
				};
				const uint32_t strides[] = {
					sizeof(Vertex),
				};
				const uint64_t offsets[] = {
					mem.offset,
				};
				device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

				XMStoreFloat4x4(&sb.g_xTransform, VP);
				sb.g_xColor = XMFLOAT4(1, 1, 1, 1);
				sb.g_xColor.w *= opacity;
				device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
				device->Draw(arraysize(verts), 0, cmd);
			}
		}
		else if (isRotator)
		{
			device->BindPipelineState(&pso_solidpart, cmd);

			const uint32_t segmentCount = 90;
			const uint32_t vertexCount = segmentCount * 3;
			GraphicsDevice::GPUAllocation mem = device->AllocateGPU(sizeof(Vertex) * vertexCount, cmd);

			switch (state)
			{
			case Translator::TRANSLATOR_X:
				XMStoreFloat4x4(&sb.g_xTransform, matX * mat);
				break;
			case Translator::TRANSLATOR_Y:
				XMStoreFloat4x4(&sb.g_xTransform, matY * mat);
				break;
			case Translator::TRANSLATOR_Z:
				XMStoreFloat4x4(&sb.g_xTransform, matZ * mat);
				break;
			case Translator::TRANSLATOR_XYZ:
				XMStoreFloat4x4(&sb.g_xTransform,
					XMMatrixRotationY(XM_PIDIV2) *
					XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(), XMVector3Normalize(transform.GetPositionV() - camera.GetEye()), camera.GetUp())) *
					mat
				);
				break;
			default:
				break;
			}

			uint8_t* dst = (uint8_t*)mem.data;
			for (uint32_t i = 0; i < segmentCount; ++i)
			{
				const float angle0 = (float)i / (float)segmentCount * angle + angle_start;
				const float angle1 = (float)(i + 1) / (float)segmentCount * angle + angle_start;

				const float radius = state == TRANSLATOR_XYZ ? (circle2_radius - circle2_width) : (circle_radius - circle_width);
				const Vertex verts[] = {
					{XMFLOAT4(0, 0, 0, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::cos(angle0) * radius, std::sin(angle0) * radius, 1), XMFLOAT4(1,1,1,1)},
					{XMFLOAT4(0, std::cos(angle1) * radius, std::sin(angle1) * radius, 1), XMFLOAT4(1,1,1,1)},
				};
				std::memcpy(dst, verts, sizeof(verts));
				dst += sizeof(verts);
			}

			const GPUBuffer* vbs[] = {
				&mem.buffer,
			};
			const uint32_t strides[] = {
				sizeof(Vertex),
			};
			const uint64_t offsets[] = {
				mem.offset,
			};
			device->BindVertexBuffers(vbs, 0, arraysize(vbs), strides, offsets, cmd);

			sb.g_xColor = XMFLOAT4(1, 1, 1, 0.5f);
			sb.g_xColor.w *= opacity;
			device->BindDynamicConstantBuffer(sb, CBSLOT_RENDERER_MISC, cmd);
			device->Draw(vertexCount, 0, cmd);
		}

		wi::font::Params params;
		params.posX = currentMouse.x - 20;
		params.posY = currentMouse.y + 20;
		params.v_align = wi::font::WIFALIGN_TOP;
		params.h_align = wi::font::WIFALIGN_RIGHT;
		params.scaling = 0.8f;
		params.shadowColor = wi::Color::Black();

		char text[256] = {};

		if (isTranslator)
		{
			snprintf(text, arraysize(text), "Offset = %.2f, %.2f, %.2f", transform.translation_local.x - transform_start.translation_local.x, transform.translation_local.y - transform_start.translation_local.y, transform.translation_local.z - transform_start.translation_local.z);
		}
		if (isRotator)
		{
			switch (state)
			{
			case Translator::TRANSLATOR_X:
				snprintf(text, arraysize(text), "Axis = X\nAngle = %.2f degrees", wi::math::RadiansToDegrees(angle));
				break;
			case Translator::TRANSLATOR_Y:
				snprintf(text, arraysize(text), "Axis = Y\nAngle = %.2f degrees", wi::math::RadiansToDegrees(angle));
				break;
			case Translator::TRANSLATOR_Z:
				snprintf(text, arraysize(text), "Axis = Z\nAngle = %.2f degrees", wi::math::RadiansToDegrees(angle));
				break;
			case Translator::TRANSLATOR_XYZ:
				snprintf(text, arraysize(text), "Axis = Screen\nAngle = %.2f degrees", wi::math::RadiansToDegrees(angle));
				break;
			default:
				break;
			}
		}
		if (isScalator)
		{
			snprintf(text, arraysize(text), "Scaling = %.2f, %.2f, %.2f", transform.scale_local.x / transform_start.scale_local.x, transform.scale_local.y / transform_start.scale_local.y, transform.scale_local.z / transform_start.scale_local.z);
		}
		params.shadowColor.setA(uint8_t(opacity * 255.0f));
		params.color.setA(uint8_t(opacity * 255.0f));
		wi::font::Draw(text, params, cmd);
	}

	device->EventEnd(cmd);
}

void Translator::PreTranslate()
{
	Scene& scene = *this->scene;

	if (!dragging)
	{
		transform.ClearTransform();
	}
	has_selected_transform = false;

	// Find the center of all the entities that are selected:
	XMVECTOR centerV = XMVectorSet(0, 0, 0, 0);
	float count = 0;
	for (auto& x : selected)
	{
		TransformComponent* transform = scene.transforms.GetComponent(x.entity);
		if (transform != nullptr)
		{
			transform->UpdateTransform();
			centerV = XMVectorAdd(centerV, transform->GetPositionV());
			count += 1.0f;
			has_selected_transform = true;
		}
	}

	if (!has_selected_transform)
		return;

	// Offset translator to center position and perform attachments:
	if (count > 0)
	{
		centerV /= count;
		XMStoreFloat3(&transform.translation_local, centerV);
		transform.SetDirty();
		transform.UpdateTransform();
	}

	// translator "bind matrix"
	XMMATRIX B = XMMatrixInverse(nullptr, XMLoadFloat4x4(&transform.world));

	matrices_current.clear();
	for (auto& x : selectedEntitiesNonRecursive)
	{
		TransformComponent* transform_selected = scene.transforms.GetComponent(x);
		if (transform_selected != nullptr)
		{
			XMFLOAT4X4 m;
			XMStoreFloat4x4(&m, XMLoadFloat4x4(&transform_selected->world) * B);
			matrices_current.push_back(m);
		}
	}
}
void Translator::PostTranslate()
{
	Scene& scene = *this->scene;

	int i = 0;
	for (auto& x : selectedEntitiesNonRecursive)
	{
		TransformComponent* transform_selected = scene.transforms.GetComponent(x);
		if (transform_selected != nullptr)
		{
			const XMFLOAT4X4& m = matrices_current[i++];

			XMMATRIX W = XMLoadFloat4x4(&m);
			XMMATRIX W_parent = XMLoadFloat4x4(&transform.world);
			W = W * W_parent;
			XMStoreFloat4x4(&transform_selected->world, W);

			// selected to world space:
			transform_selected->ApplyTransform();

			// selected to parent local space (if has parent):
			const HierarchyComponent* hier = scene.hierarchy.GetComponent(x);
			if (hier != nullptr)
			{
				const TransformComponent* transform_parent = scene.transforms.GetComponent(hier->parentID);
				if (transform_parent != nullptr)
				{
					transform_selected->MatrixTransform(XMMatrixInverse(nullptr, XMLoadFloat4x4(&transform_parent->world)));
				}
			}
		}
	}
}

XMMATRIX Translator::GetMirrorMatrix(TRANSLATOR_STATE state, const CameraComponent& camera) const
{
	XMMATRIX mirror = XMMatrixIdentity();

	if (isRotator)
		return mirror;

	switch (state)
	{
	case Translator::TRANSLATOR_X:
		if (camera.Eye.x < transform.translation_local.x)
		{
			mirror *= XMMatrixScaling(-1, 1, 1);
		}
		break;
	case Translator::TRANSLATOR_Y:
		if (camera.Eye.y < transform.translation_local.y)
		{
			mirror *= XMMatrixScaling(1, -1, 1);
		}
		break;
	case Translator::TRANSLATOR_Z:
		if (camera.Eye.z < transform.translation_local.z)
		{
			mirror *= XMMatrixScaling(1, 1, -1);
		}
		break;
	case Translator::TRANSLATOR_XY:
		if (camera.Eye.x < transform.translation_local.x)
		{
			mirror *= XMMatrixScaling(-1, 1, 1);
		}
		if (camera.Eye.y < transform.translation_local.y)
		{
			mirror *= XMMatrixScaling(1, -1, 1);
		}
		break;
	case Translator::TRANSLATOR_XZ:
		if (camera.Eye.x < transform.translation_local.x)
		{
			mirror *= XMMatrixScaling(-1, 1, 1);
		}
		if (camera.Eye.z < transform.translation_local.z)
		{
			mirror *= XMMatrixScaling(1, 1, -1);
		}
		break;
	case Translator::TRANSLATOR_YZ:
		if (camera.Eye.y < transform.translation_local.y)
		{
			mirror *= XMMatrixScaling(1, -1, 1);
		}
		if (camera.Eye.z < transform.translation_local.z)
		{
			mirror *= XMMatrixScaling(1, 1, -1);
		}
		break;
	default:
		break;
	}

	return mirror;
}
void Translator::WriteAxisText(TRANSLATOR_STATE axis, const wi::scene::CameraComponent& camera, char* text) const
{
	switch (axis)
	{
	case Translator::TRANSLATOR_X:
		if (camera.Eye.x < transform.translation_local.x)
		{
			std::memcpy(text, "-X", 2);
		}
		else
		{
			std::memcpy(text, "X", 1);
		}
		break;
	case Translator::TRANSLATOR_Y:
		if (camera.Eye.y < transform.translation_local.y)
		{
			std::memcpy(text, "-Y", 2);
		}
		else
		{
			std::memcpy(text, "Y", 1);
		}
		break;
	case Translator::TRANSLATOR_Z:
		if (camera.Eye.z < transform.translation_local.z)
		{
			std::memcpy(text, "-Z", 2);
		}
		else
		{
			std::memcpy(text, "Z", 1);
		}
		break;
	default:
		break;
	}
}

