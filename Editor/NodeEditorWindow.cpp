
#include "stdafx.h"
#include "NodeEditorWindow.h"
#include "Editor.h"
#include "wiImage.h"
#include "wiScene.h"
#include <unordered_set>
#include <functional>
#include <fstream>
#include "json.hpp"


using namespace wi::graphics;
using namespace wi::gui;

// Internal helpers for link rendering de-duplication:
namespace {
  struct SegmentKey { uint64_t a; uint64_t b; };
  struct SegmentKeyHasher {
    size_t operator()(const SegmentKey& k) const noexcept {
      uint64_t x = k.a;
      x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
      uint64_t y = k.b;
      y ^= y >> 33; y *= 0xff51afd7ed558ccdULL; y ^= y >> 33; y *= 0xc4ceb9fe1a85ec53ULL; y ^= y >> 33;
      uint64_t h = x ^ (y + 0x9e3779b97f4a7c15ULL + (x<<6) + (x>>2));
      return (size_t)h;
    }
  };
  struct SegmentKeyEq {
    bool operator()(const SegmentKey& a, const SegmentKey& b) const noexcept { return a.a == b.a && a.b == b.b; }
  };
}

// Helper: read text from TextInputField, prefer current typing if any
static std::string GetFieldText(const wi::gui::TextInputField& f)
{
    // TextInputField getters are non-const, so cast away constness for read-only access
    wi::gui::TextInputField& nc = const_cast<wi::gui::TextInputField&>(f);
    std::string s = nc.GetCurrentInputValue();
    if (s.empty()) s = nc.GetValue();
    return s;
}

static const float separator = 140.0f;
// Autosize configuration
static const float NODE_AUTOSIZE_SHRINK_DELAY = 0.4f;     
static const float NODE_MAX_CONTENT_W = 900.0f;          
static const float NODE_MIN_CONTENT_W = 200.0f;          
static const float NODE_AUTOSIZE_THROTTLE = 1.0f / 15.0f;

void NodeEditorWindow::Create(EditorComponent *_editor) {
  editor = _editor;
  control_size = 30;

  wi::gui::Window::Create("Node Editor");


  addNodeButton.Create("Add Node");
  addNodeButton.SetLocalizationEnabled(false);
  addNodeButton.SetSize(XMFLOAT2(120, 25));
  addNodeButton.OnClick([this](wi::gui::EventArgs) { AddNode(); });
  AddWidget(&addNodeButton, wi::gui::Window::AttachmentOptions::NONE);

  importFromSceneButton.Create("Import Scene I/O");
  importFromSceneButton.SetLocalizationEnabled(false);
  importFromSceneButton.SetSize(XMFLOAT2(120, 25));
  importFromSceneButton.OnClick([this](wi::gui::EventArgs) { BuildNodesFromSceneMetadata(); });
  AddWidget(&importFromSceneButton, wi::gui::Window::AttachmentOptions::NONE);

  addTimerButton.Create("Add Timer");
  addTimerButton.SetLocalizationEnabled(false);
  addTimerButton.SetSize(XMFLOAT2(120, 25));
  addTimerButton.OnClick([this](wi::gui::EventArgs) { AddTimerNode(); });
  AddWidget(&addTimerButton, wi::gui::Window::AttachmentOptions::NONE);

  addSequenceButton.Create("Add Sequence");
  addSequenceButton.SetLocalizationEnabled(false);
  addSequenceButton.SetSize(XMFLOAT2(120, 25));
  addSequenceButton.OnClick([this](wi::gui::EventArgs) { AddSequenceNode(); });
  AddWidget(&addSequenceButton, wi::gui::Window::AttachmentOptions::NONE);

  saveGraphButton.Create("Save Graph");
  saveGraphButton.SetLocalizationEnabled(false);
  saveGraphButton.SetSize(XMFLOAT2(120, 25));
  saveGraphButton.OnClick([this](wi::gui::EventArgs) {
    // TODO: integrate file picker; default to nodegraph.json
    SaveGraph("nodegraph.json");
  });
  AddWidget(&saveGraphButton, wi::gui::Window::AttachmentOptions::NONE);

  loadGraphButton.Create("Load Graph");
  loadGraphButton.SetLocalizationEnabled(false);
  loadGraphButton.SetSize(XMFLOAT2(120, 25));
  loadGraphButton.OnClick([this](wi::gui::EventArgs) {
    LoadGraph("nodegraph.json");
  });
  AddWidget(&loadGraphButton, wi::gui::Window::AttachmentOptions::NONE);

  SetVisible(false);
  nextNodeUid = 1;
  nextConnUid = 1;
}

void NodeEditorWindow::BuildNodesFromSceneMetadata() {
  if (!editor) return;
  wi::scene::Scene& scene = editor->GetCurrentScene();
  const auto& entities = scene.metadatas.GetEntityArray();
  for (wi::ecs::Entity e : entities) {
    if (entityIndex.find(e) != entityIndex.end()) continue;
    std::string nm;
    if (auto* nc = scene.names.GetComponent(e)) nm = nc->name; else nm = std::to_string((uint64_t)e);
    AddNodeForEntity(e, nm);
  }
  recentlyAddedNewNode = true;
  layoutDirty = true;
}

void NodeEditorWindow::Render(const wi::Canvas &canvas, CommandList cmd) const {
  wi::gui::Window::Render(canvas, cmd);

  if (IsVisible() && !IsCollapsed()) {
    ApplyScissor(canvas, scissorRect, cmd);
    wi::image::Params params;
    params.pos =
        XMFLOAT3(translation.x + separator, translation.y + control_size, 0);
    params.siz = XMFLOAT2(2, scale.y - control_size);
    params.color = shadow_color;
    wi::image::Draw(nullptr, params, cmd);

    // Draw IO pins and self-target highlights per node (for visualization)
    for (auto& n : nodes) {
      const float pinR = 4.0f;
      // Inputs on left (use cached positions to avoid transient jitter)
      for (size_t i = 0; i < n->inputLabels.size(); ++i) {
        auto& il = n->inputLabels[i];
        XMFLOAT2 pinpos = (i < n->cachedInputPins.size()) ? n->cachedInputPins[i] : XMFLOAT2(il->translation.x - 6.0f, il->translation.y + il->scale.y * 0.5f);
        float cx = pinpos.x;
        float cy = pinpos.y;
        bool isHover = drag.active && drag.hoverNode == n.get() && drag.hoverInput == il.get();
        wi::image::Params pin;
        pin.pos = XMFLOAT3(cx, cy, 0);
        float r = isHover ? pinR + 2.0f : pinR;
        pin.siz = XMFLOAT2(r * 2, r * 2);
        pin.pivot = XMFLOAT2(0.5f, 0.5f);
        pin.color = isHover ? XMFLOAT4(0.4f, 1.0f, 0.6f, 1) : XMFLOAT4(0.7f, 0.9f, 1.0f, 1);
        pin.enableCornerRounding();
        for (int i = 0; i < arraysize(pin.corners_rounding); ++i) pin.corners_rounding[i].radius = r;
        wi::image::Draw(nullptr, pin, cmd);
        if (isHover) {
          wi::image::Params ring = pin;
          ring.siz = XMFLOAT2((r + 4.0f) * 2, (r + 4.0f) * 2);
          ring.color = XMFLOAT4(0.4f, 1.0f, 0.6f, 0.25f);
          wi::image::Draw(nullptr, ring, cmd);
        }
      }
      // Outputs on right: one pin per output header row, with self-target highlight (anchor to header widgets)
      for (auto& orow : n->outputRows) {
        
        bool self_target = false;
        for (auto& crow : n->connectionRows) {
          if (crow->outputName != orow->name) continue;
          std::string tgt = crow->target.GetText();
          if (tgt.empty() || tgt == "!self") { self_target = true; break; }
        }
        if (self_target) {
          // draw subtle background under output label to indicate self target
          wi::image::Params bg;
          bg.pos = XMFLOAT3(orow->label.translation.x + orow->label.scale.x * 0.5f, orow->label.translation.y + orow->label.scale.y * 0.5f, 0);
          bg.siz = XMFLOAT2(orow->label.scale.x, orow->label.scale.y);
          bg.pivot = XMFLOAT2(0.5f, 0.5f);
          bg.color = XMFLOAT4(0.35f, 0.55f, 0.35f, 0.35f);
          wi::image::Draw(nullptr, bg, cmd);
        }

        XMFLOAT2 pinpos = {};
        bool foundCache = false;
        for (auto& co : n->cachedOutputPins) {
          if (co.row == orow.get()) { pinpos = co.pos; foundCache = true; break; }
        }
        float cx, cy;
        if (foundCache) {
          cx = pinpos.x; cy = pinpos.y;
        } else {
          float headerRight = std::max(orow->label.translation.x + orow->label.scale.x,
                                       orow->addButton.translation.x + orow->addButton.scale.x);
          cx = headerRight + 6.0f;
          cy = orow->label.translation.y + orow->label.scale.y * 0.5f;
        }
        bool isSrc = drag.active && drag.srcNode == n.get() && drag.srcOutput == orow.get();
        wi::image::Params pin;
        pin.pos = XMFLOAT3(cx, cy, 0);
        float r = isSrc ? pinR + 2.0f : pinR;
        pin.siz = XMFLOAT2(r * 2, r * 2);
        pin.pivot = XMFLOAT2(0.5f, 0.5f);
        pin.color = isSrc ? XMFLOAT4(1.0f, 0.95f, 0.5f, 1) : XMFLOAT4(1.0f, 0.8f, 0.3f, 1);
        pin.enableCornerRounding();
        for (int i = 0; i < arraysize(pin.corners_rounding); ++i) pin.corners_rounding[i].radius = r;
        wi::image::Draw(nullptr, pin, cmd);
        if (isSrc) {
          wi::image::Params ring = pin;
          ring.siz = XMFLOAT2((r + 4.0f) * 2, (r + 4.0f) * 2);
          ring.color = XMFLOAT4(1.0f, 0.95f, 0.5f, 0.25f);
          wi::image::Draw(nullptr, ring, cmd);
        }
      }
    }

    // Draw connection wires
    // Deduplicate shared early segments across fanouts to avoid double-drawing before the last hub
    std::unordered_set<SegmentKey, SegmentKeyHasher, SegmentKeyEq> drawnSegments;
    // Reserve a rough upper bound to limit rehashing: sum of (anchors + 1) per connection
    {
      size_t approx = 0;
      for (const auto& n : nodes) {
        for (const auto& cr : n->connectionRows) {
          approx += (size_t)cr->anchorHubIds.size() + 1;
        }
      }
      // Targets can multiply segments, so give some headroom
      drawnSegments.reserve(approx * 2 + 16);
    }
    // Begin batched wire drawing
    wi::gui::BeginWireBatch(canvas, cmd);

    for (const auto& n : nodes) {
      for (const auto& crow_uptr : n->connectionRows) {
        const auto* crow = crow_uptr.get();
        const Node::OutputUI* src_orow = n->FindOutputRow(crow->outputName);
        if (!src_orow) continue;

        float sx, sy;
        bool foundCache = false;
        for (auto& co : n->cachedOutputPins) {
          if (co.row == src_orow) { sx = co.pos.x; sy = co.pos.y; foundCache = true; break; }
        }
        if (!foundCache) {
          const float headerRight = std::max(src_orow->label.translation.x + src_orow->label.scale.x,
                                             src_orow->addButton.translation.x + src_orow->addButton.scale.x);
          sx = headerRight + 6.0f;
          sy = src_orow->label.translation.y + src_orow->label.scale.y * 0.5f;
        }

        // Determine target nodes by target field (can be multiple) with entity binding support
        std::string target_text = GetFieldText(crow->target);
        if (target_text == "!self" || target_text.empty()) {
          continue;
        }

        wi::vector<const Node*> targets;
        if (crow->targetEntity != wi::ecs::INVALID_ENTITY) {
          auto itent = entityIndex.find(crow->targetEntity);
          if (itent != entityIndex.end() && itent->second != n.get()) {
            targets.push_back(itent->second);
          }
        } else {
          if (auto itnode = nodeIndex.find(target_text); itnode != nodeIndex.end()) {
            for (auto* cand : itnode->second) if (cand != n.get()) targets.push_back(cand);
          }
        }
        if (targets.empty()) continue;

        const std::string input_name = GetFieldText(crow->input);

        // Build shared path (source -> hubs) and draw it once
        // Build stable endpoint tokens for de-dup across connections
        constexpr uint64_t TAG_SRC = 0x0000000000000000ULL;
        constexpr uint64_t TAG_HUB = 0x4000000000000000ULL;
        constexpr uint64_t TAG_TIL = 0x8000000000000000ULL; // target input label
        constexpr uint64_t TAG_TNO = 0xC000000000000000ULL; // target node

        wi::vector<XMFLOAT2> shared_pts;
        wi::vector<uint64_t> shared_tokens;
        shared_pts.push_back(XMFLOAT2(sx, sy));
        shared_tokens.push_back(TAG_SRC | (uint64_t)(uintptr_t)src_orow);
        for (size_t i = 0; i < crow->anchorHubIds.size(); ++i) {
          const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
          shared_pts.push_back(XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y));
          shared_tokens.push_back(TAG_HUB | (uint64_t)crow->anchorHubIds[i]);
        }

        // Compact near-duplicates on the shared path
        if (shared_pts.size() >= 2) {
          wi::vector<XMFLOAT2> compact;
          compact.reserve(shared_pts.size());
          auto near_eq = [](const XMFLOAT2& a, const XMFLOAT2& b){ float dx=a.x-b.x, dy=a.y-b.y; return dx*dx+dy*dy <= 1.0f; };
          for (const auto& p : shared_pts) { if (compact.empty() || !near_eq(compact.back(), p)) compact.push_back(p); }
          if (compact.size() >= 2) shared_pts = std::move(compact);
        }

        auto sub = [](const XMFLOAT2& a, const XMFLOAT2& b) { return XMFLOAT2(a.x - b.x, a.y - b.y); };
        auto mul = [](const XMFLOAT2& a, float s) { return XMFLOAT2(a.x * s, a.y * s); };
        auto len = [](const XMFLOAT2& v) { return std::sqrt(v.x * v.x + v.y * v.y); };
        auto clamp_handle = [&](const XMFLOAT2& v, float minl, float maxl, float fallback_dirx) {
          float L = len(v);
          if (L < 1e-5f) {
            float Lc = std::max(minl, std::min(maxl, minl));
            return XMFLOAT2((fallback_dirx >= 0 ? 1.0f : -1.0f) * Lc, 0);
          }
          float Lc = std::max(minl, std::min(maxl, L));
          float s = Lc / L;
          return XMFLOAT2(v.x * s, v.y * s);
        };

        const float endpointBias = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
        const float minHandle = NodeEditorWindow::WIRE_MIN_HANDLE;
        const float clampK = NodeEditorWindow::WIRE_CLAMP_K;

        XMFLOAT4 col = (selectedConnection == crow) ? XMFLOAT4(1.0f, 0.9f, 0.6f, 1) : XMFLOAT4(0.6f, 1.0f, 0.6f, 1);

        // Draw shared segments once
        if (shared_pts.size() >= 2) {
          for (size_t i = 0; i + 1 < shared_pts.size(); ++i) {
            SegmentKey segkey{ shared_tokens[i], shared_tokens[i + 1] };
            if (drawnSegments.find(segkey) != drawnSegments.end()) continue;
            const XMFLOAT2& P0 = shared_pts[i];
            const XMFLOAT2& P1 = shared_pts[i + 1];
            XMFLOAT2 T0, T1;
            if (i == 0) {
              T0 = XMFLOAT2(+endpointBias, 0);
            } else {
              const XMFLOAT2& Pm1 = shared_pts[i - 1];
              T0 = mul(sub(shared_pts[i + 1], Pm1), 0.5f);
            }
            if (i + 1 < shared_pts.size() - 1) {
              const XMFLOAT2& Pp2 = shared_pts[i + 2];
              T1 = mul(sub(Pp2, P0), 0.5f);
            } else {
              // Last shared segment: approximate forward tangent by the segment direction
              T1 = mul(sub(P1, P0), 0.5f);
            }
            float seglen = len(sub(P1, P0));
            float maxHandle = std::max(minHandle, seglen * clampK);
            float dirx0 = (P1.x - P0.x) >= 0 ? 1.0f : -1.0f;
            float dirx1 = -dirx0;
            T0 = clamp_handle(T0, std::min(minHandle, maxHandle), maxHandle, dirx0);
            T1 = clamp_handle(T1, std::min(minHandle, maxHandle), maxHandle, dirx1);
            wi::gui::AddWireBezierStripTangent(P0, P1, T0, T1, 2.0f, col);
            drawnSegments.insert(segkey);
          }
        }

        // Prepare for final leg per target
        const bool has_prev = shared_pts.size() >= 2;
        const XMFLOAT2 shared_last = shared_pts.empty() ? XMFLOAT2(sx, sy) : shared_pts.back();
        const XMFLOAT2 shared_prev = has_prev ? shared_pts[shared_pts.size() - 2] : shared_last;
        const uint64_t shared_last_token = shared_tokens.empty() ? (TAG_SRC | (uint64_t)(uintptr_t)src_orow) : shared_tokens.back();

        for (const Node* target_node : targets) {
          const wi::gui::Label* target_input_label = nullptr;
          size_t target_input_index = (size_t)-1;
          if (!input_name.empty()) {
            if (target_node->GetInputIndex(input_name, target_input_index)) {
              target_input_label = target_node->inputLabels[target_input_index].get();
            }
          }
          float ex, ey;
          if (target_input_label) {
            if (target_input_index < target_node->cachedInputPins.size()) {
              ex = target_node->cachedInputPins[target_input_index].x; ey = target_node->cachedInputPins[target_input_index].y;
            } else {
              ex = target_node->window.translation.x + 6.0f;
              ey = target_input_label->translation.y + target_input_label->scale.y * 0.5f;
            }
          } else {
            ex = target_node->window.translation.x + 6.0f;
            ey = target_node->window.translation.y + target_node->window.scale.y * 0.5f;
          }

          uint64_t target_token = (uint64_t)(uintptr_t)(target_input_label ? (const void*)target_input_label : (const void*)target_node);
          uint64_t tagged_target = (target_input_label ? TAG_TIL : TAG_TNO) | target_token;
          SegmentKey lastseg{ shared_last_token, tagged_target };
          if (drawnSegments.find(lastseg) != drawnSegments.end()) continue;

          const XMFLOAT2 P0 = shared_last;
          const XMFLOAT2 P1 = XMFLOAT2(ex, ey);
          XMFLOAT2 T0, T1;
          if (has_prev) {
            // Catmull-Rom style at hub/source using previous point and target
            T0 = mul(sub(P1, shared_prev), 0.5f);
          } else {
            T0 = XMFLOAT2(+endpointBias, 0);
          }
          T1 = XMFLOAT2(+endpointBias, 0);
          float seglen = len(sub(P1, P0));
          float maxHandle = std::max(minHandle, seglen * clampK);
          float dirx0 = (P1.x - P0.x) >= 0 ? 1.0f : -1.0f;
          float dirx1 = -dirx0;
          T0 = clamp_handle(T0, std::min(minHandle, maxHandle), maxHandle, dirx0);
          T1 = clamp_handle(T1, std::min(minHandle, maxHandle), maxHandle, dirx1);
          wi::gui::AddWireBezierStripTangent(P0, P1, T0, T1, 2.0f, col);
          drawnSegments.insert(lastseg);
        }
        
      }
    }

    // Flush batched wires before other draws
    wi::gui::FlushWireBatch();

    // Hover overlay: draw hovered connection thicker on top for discoverability
    // Skip when it is already selected to avoid double-highlighting
    if (hoveredConnection != nullptr && hoveredConnection != selectedConnection)
    {
      // Find owner node and source output row
      const Node* owner = nullptr;
      for (const auto& n : nodes) {
        for (const auto& cr : n->connectionRows) {
          if (cr.get() == hoveredConnection) { owner = n.get(); break; }
        }
        if (owner) break;
      }
      if (owner) {
        const Node::OutputUI* src_orow = owner->FindOutputRow(hoveredConnection->outputName);
        if (src_orow) {
          float sx, sy;
          bool foundCache = false;
          for (auto& co : owner->cachedOutputPins) {
            if (co.row == src_orow) { sx = co.pos.x; sy = co.pos.y; foundCache = true; break; }
          }
          if (!foundCache) {
            const float headerRight = std::max(src_orow->label.translation.x + src_orow->label.scale.x,
                                              src_orow->addButton.translation.x + src_orow->addButton.scale.x);
            sx = headerRight + 6.0f;
            sy = src_orow->label.translation.y + src_orow->label.scale.y * 0.5f;
          }

          // Resolve targets similarly to render
          wi::vector<const Node*> targets;
          std::string target_text = GetFieldText(hoveredConnection->target);
          if (!(target_text == "!self" || target_text.empty())) {
            if (hoveredConnection->targetEntity != wi::ecs::INVALID_ENTITY) {
              auto itent = entityIndex.find(hoveredConnection->targetEntity);
              if (itent != entityIndex.end() && itent->second != owner) targets.push_back(itent->second);
            } else {
              if (auto itnode = nodeIndex.find(target_text); itnode != nodeIndex.end()) {
                for (auto* cand : itnode->second) if (cand != owner) targets.push_back(cand);
              }
            }
          }

          const float endpointBias = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
          const float minHandle = NodeEditorWindow::WIRE_MIN_HANDLE;
          const float clampK = NodeEditorWindow::WIRE_CLAMP_K;
          auto sub = [](const XMFLOAT2& a, const XMFLOAT2& b) { return XMFLOAT2(a.x - b.x, a.y - b.y); };
          auto mul = [](const XMFLOAT2& a, float s) { return XMFLOAT2(a.x * s, a.y * s); };
          auto len = [](const XMFLOAT2& v) { return std::sqrt(v.x * v.x + v.y * v.y); };
          auto clamp_handle = [&](const XMFLOAT2& v, float minl, float maxl, float fallback_dirx) {
            float L = len(v);
            if (L < 1e-5f) {
              float Lc = std::max(minl, std::min(maxl, minl));
              return XMFLOAT2((fallback_dirx >= 0 ? 1.0f : -1.0f) * Lc, 0);
            }
            float Lc = std::max(minl, std::min(maxl, L));
            float s = Lc / L;
            return XMFLOAT2(v.x * s, v.y * s);
          };

          // Build shared path
          wi::vector<XMFLOAT2> shared_pts;
          shared_pts.push_back(XMFLOAT2(sx, sy));
          for (size_t i = 0; i < hoveredConnection->anchorHubIds.size(); ++i) {
            const auto* hub = GetHub(hoveredConnection->anchorHubIds[i]); if (!hub) continue;
            shared_pts.push_back(XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y));
          }
          if (shared_pts.size() >= 2) {
            wi::vector<XMFLOAT2> compact;
            compact.reserve(shared_pts.size());
            auto near_eq = [](const XMFLOAT2& a, const XMFLOAT2& b){ float dx=a.x-b.x, dy=a.y-b.y; return dx*dx+dy*dy <= 1.0f; };
            for (const auto& p : shared_pts) { if (compact.empty() || !near_eq(compact.back(), p)) compact.push_back(p); }
            if (compact.size() >= 2) shared_pts = std::move(compact);
          }

          // Overlay draw: mild orange glow (thicker, semi-transparent)
          const float glow_thick = 8.0f;
          XMFLOAT4 glow_col = XMFLOAT4(1.0f, 0.6f, 0.2f, 0.35f);
          wi::gui::BeginWireBatch(canvas, cmd);
          if (shared_pts.size() >= 2) {
            for (size_t i = 0; i + 1 < shared_pts.size(); ++i) {
              const XMFLOAT2& P0 = shared_pts[i];
              const XMFLOAT2& P1 = shared_pts[i + 1];
              XMFLOAT2 T0, T1;
              if (i == 0) {
                T0 = XMFLOAT2(+endpointBias, 0);
              } else {
                const XMFLOAT2& Pm1 = shared_pts[i - 1];
                T0 = mul(sub(shared_pts[i + 1], Pm1), 0.5f);
              }
              if (i + 1 < shared_pts.size() - 1) {
                const XMFLOAT2& Pp2 = shared_pts[i + 2];
                T1 = mul(sub(Pp2, P0), 0.5f);
              } else {
                T1 = mul(sub(P1, P0), 0.5f);
              }
              float seglen = len(sub(P1, P0));
              float maxHandle = std::max(minHandle, seglen * clampK);
              float dirx0 = (P1.x - P0.x) >= 0 ? 1.0f : -1.0f;
              float dirx1 = -dirx0;
              T0 = clamp_handle(T0, std::min(minHandle, maxHandle), maxHandle, dirx0);
              T1 = clamp_handle(T1, std::min(minHandle, maxHandle), maxHandle, dirx1);
              wi::gui::AddWireBezierStripTangent(P0, P1, T0, T1, glow_thick, glow_col);
            }
          }

          // Final legs to targets
          const bool has_prev = shared_pts.size() >= 2;
          const XMFLOAT2 shared_last = shared_pts.empty() ? XMFLOAT2(sx, sy) : shared_pts.back();
          const XMFLOAT2 shared_prev = has_prev ? shared_pts[shared_pts.size() - 2] : shared_last;
          const std::string input_name = GetFieldText(hoveredConnection->input);
          for (const Node* target_node : targets) {
            const wi::gui::Label* target_input_label = nullptr;
            size_t target_input_index = (size_t)-1;
            if (!input_name.empty()) {
              if (target_node->GetInputIndex(input_name, target_input_index)) {
                target_input_label = target_node->inputLabels[target_input_index].get();
              }
            }
            float ex, ey;
            if (target_input_label) {
              if (target_input_index < target_node->cachedInputPins.size()) {
                ex = target_node->cachedInputPins[target_input_index].x; ey = target_node->cachedInputPins[target_input_index].y;
              } else {
                ex = target_node->window.translation.x + 6.0f;
                ey = target_input_label->translation.y + target_input_label->scale.y * 0.5f;
              }
            } else {
              ex = target_node->window.translation.x + 6.0f;
              ey = target_node->window.translation.y + target_node->window.scale.y * 0.5f;
            }
            const XMFLOAT2 P0 = shared_last;
            const XMFLOAT2 P1 = XMFLOAT2(ex, ey);
            XMFLOAT2 T0, T1;
            if (has_prev) {
              T0 = mul(sub(P1, shared_prev), 0.5f);
            } else {
              T0 = XMFLOAT2(+endpointBias, 0);
            }
            T1 = XMFLOAT2(+endpointBias, 0);
            float seglen = len(sub(P1, P0));
            float maxHandle = std::max(minHandle, seglen * clampK);
            float dirx0 = (P1.x - P0.x) >= 0 ? 1.0f : -1.0f;
            float dirx1 = -dirx0;
            T0 = clamp_handle(T0, std::min(minHandle, maxHandle), maxHandle, dirx0);
            T1 = clamp_handle(T1, std::min(minHandle, maxHandle), maxHandle, dirx1);
            wi::gui::AddWireBezierStripTangent(P0, P1, T0, T1, glow_thick, glow_col);
          }
          wi::gui::FlushWireBatch();
        }
      }
    }

    // Draw preview wire while dragging (compute live source position to avoid 1-frame mismatch)
    if (drag.active && drag.srcNode && drag.srcOutput) {
      wi::gui::BeginWireBatch(canvas, cmd);
      float sx = 0.0f;
      float sy = 0.0f;
      if (drag.fromAnchor) {
        if (anchorRight.active && anchorRight.conn && anchorRight.index >= 0 && anchorRight.index < (int)anchorRight.conn->anchorHubIds.size()) {
          const auto* hub = GetHub(anchorRight.conn->anchorHubIds[anchorRight.index]);
          if (hub) {
            sx = scrollable_area.translation.x + hub->pos.x;
            sy = scrollable_area.translation.y + hub->pos.y;
          }
        }
      }
      if (sx == 0.0f && sy == 0.0f) {
        float headerRight = std::max(drag.srcOutput->label.translation.x + drag.srcOutput->label.scale.x,
                                     drag.srcOutput->addButton.translation.x + drag.srcOutput->addButton.scale.x);
        sx = headerRight + 6.0f;
        sy = drag.srcOutput->label.translation.y + drag.srcOutput->label.scale.y * 0.5f;
      }

      float ex = drag.cursor.x;
      float ey = drag.cursor.y;
      if (drag.hoverNode && drag.hoverInput) {
        ex = drag.hoverNode->window.translation.x + 6.0f;
        ey = drag.hoverInput->translation.y + drag.hoverInput->scale.y * 0.5f;
      }
      // Tangent-aware preview: leave source horizontally, enter input horizontally if snapping
      const float endpointBias = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
      XMFLOAT2 P0(sx, sy);
      XMFLOAT2 P1(ex, ey);
      // Always leave source/output horizontally to the right
      XMFLOAT2 T0(+endpointBias, 0);
      XMFLOAT2 T1;
      if (drag.hoverNode && drag.hoverInput) {
        // Approach target/input horizontally from the left (opposite)
        T1 = XMFLOAT2(+endpointBias, 0);
      } else {
        // free-end towards cursor: aim along delta, clamp to sensible length
        XMFLOAT2 delta(P1.x - P0.x, P1.y - P0.y);
        float L = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        float maxHandle = std::max(10.0f, L * 0.45f);
        if (L > 1e-4f) {
          float s = std::min(maxHandle, L * 0.5f) / L;
          T1 = XMFLOAT2(delta.x * s, delta.y * s);
        } else {
          T1 = XMFLOAT2(-T0.x, 0);
        }
      }
      wi::gui::AddWireBezierStripTangent(P0, P1, T0, T1, 2.0f, XMFLOAT4(0.8f, 0.9f, 1.0f, 0.9f));
      wi::gui::FlushWireBatch();
    }

    // Draw anchor handles for the selected connection on top
    if (selectedConnection != nullptr) {
      for (size_t i = 0; i < selectedConnection->anchorHubIds.size(); ++i) {
        const auto* hub = GetHub(selectedConnection->anchorHubIds[i]); if (!hub) continue;
        XMFLOAT2 ap(XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y));
        wi::image::Params a;
        a.pos = XMFLOAT3(ap.x, ap.y, 0);
        a.siz = XMFLOAT2(10, 10);
        a.pivot = XMFLOAT2(0.5f, 0.5f);
        a.color = XMFLOAT4(1, 1, 1, 0.9f);
        a.enableCornerRounding();
        for (int k = 0; k < arraysize(a.corners_rounding); ++k) a.corners_rounding[k].radius = 5;
        wi::image::Draw(nullptr, a, cmd);
      }
    }
  }
}

void NodeEditorWindow::Update(const wi::Canvas &canvas, float dt) {
  wi::gui::Window::Update(canvas, dt);

  SetShadowRadius(6);

  static const float radius = 15;

  for (int i = 0; i < arraysize(wi::gui::Widget::sprites); ++i) {
    sprites[i].params.enableCornerRounding();
    sprites[i].params.corners_rounding[0].radius = radius;
    sprites[i].params.corners_rounding[1].radius = radius;
    sprites[i].params.corners_rounding[2].radius = radius;
    sprites[i].params.corners_rounding[3].radius = radius;

    addNodeButton.sprites[i].params.enableCornerRounding();
    addNodeButton.sprites[i].params.corners_rounding[0].radius = radius;
    addNodeButton.sprites[i].params.corners_rounding[1].radius = radius;
    addNodeButton.sprites[i].params.corners_rounding[2].radius = radius;
    addNodeButton.sprites[i].params.corners_rounding[3].radius = radius;

    importFromSceneButton.sprites[i].params.enableCornerRounding();
    importFromSceneButton.sprites[i].params.corners_rounding[0].radius = radius;
    importFromSceneButton.sprites[i].params.corners_rounding[1].radius = radius;
    importFromSceneButton.sprites[i].params.corners_rounding[2].radius = radius;
    importFromSceneButton.sprites[i].params.corners_rounding[3].radius = radius;

    addTimerButton.sprites[i].params.enableCornerRounding();
    addTimerButton.sprites[i].params.corners_rounding[0].radius = radius;
    addTimerButton.sprites[i].params.corners_rounding[1].radius = radius;
    addTimerButton.sprites[i].params.corners_rounding[2].radius = radius;
    addTimerButton.sprites[i].params.corners_rounding[3].radius = radius;

    addSequenceButton.sprites[i].params.enableCornerRounding();
    addSequenceButton.sprites[i].params.corners_rounding[0].radius = radius;
    addSequenceButton.sprites[i].params.corners_rounding[1].radius = radius;
    addSequenceButton.sprites[i].params.corners_rounding[2].radius = radius;
    addSequenceButton.sprites[i].params.corners_rounding[3].radius = radius;
  }

  for (auto &node : nodes) {
    node->window.SetShadowRadius(2);
    for (int i = 0; i < arraysize(wi::gui::Widget::sprites); ++i) {
      node->window.sprites[i].params.enableCornerRounding();
      node->window.sprites[i].params.corners_rounding[0].radius = radius;
      node->window.sprites[i].params.corners_rounding[1].radius = radius;
      node->window.sprites[i].params.corners_rounding[2].radius = radius;
      node->window.sprites[i].params.corners_rounding[3].radius = radius;
    }
  }

  addNodeButton.SetShadowRadius(0);
  importFromSceneButton.SetShadowRadius(0);
  addTimerButton.SetShadowRadius(0);
  addSequenceButton.SetShadowRadius(0);

  // Drag & drop from output pin to input pin:
  {
    XMFLOAT4 pointer = wi::input::GetPointer();
    XMFLOAT2 p = XMFLOAT2(pointer.x, pointer.y);
    const float pinDetectR = 8.0f;

    if (!drag.active && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
      for (auto& n : nodes) {
        const float cx_base = n->window.translation.x + n->window.scale.x - 6.0f;
        for (auto& orow : n->outputRows) {
          float cy = orow->label.translation.y + orow->label.scale.y * 0.5f;
          float dx = p.x - cx_base;
          float dy = p.y - cy;
          if (dx * dx + dy * dy <= pinDetectR * pinDetectR) {
            drag.active = true;
            drag.rightButton = false;
            drag.srcNode = n.get();
            drag.srcOutput = orow.get();
            drag.srcPos = XMFLOAT2(cx_base, cy);
            drag.cursor = p;
            drag.hoverNode = nullptr;
            drag.hoverInput = nullptr;
            break;
          }
        }
        if (drag.active) break;
      }
    }

    // Right-click on output pin: remove all connections from that output
    if (!drag.active && wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT)) {
      bool removed_any = false;
      for (auto& n : nodes) {
        const float cx_base = n->window.translation.x + n->window.scale.x - 6.0f;
        for (auto& orow : n->outputRows) {
          float cy = orow->label.translation.y + orow->label.scale.y * 0.5f;
          float dx = p.x - cx_base;
          float dy = p.y - cy;
          if (dx * dx + dy * dy <= pinDetectR * pinDetectR) {
            // collect and remove (with undo macro)
            wi::vector<Node::ConnectionUI*> toremove;
            for (auto& cr : n->connectionRows) if (cr->outputName == orow->name) toremove.push_back(cr.get());
            NodeEditorWindow::UndoCommand macro; macro.type = NodeEditorWindow::UndoType::Macro; macro.label = "Remove all from output";
            for (auto* r : toremove) {
              NodeEditorWindow::ConnectionSnapshot s = MakeSnapshot(n.get(), r);
              NodeEditorWindow::UndoCommand sub; sub.type = NodeEditorWindow::UndoType::RemoveConnection; sub.snap = s;
              macro.macro.push_back(std::move(sub));
            }
            if (!toremove.empty()) PushCommand(std::move(macro));
            for (auto* r : toremove) {
              if (selectedConnection == r) selectedConnection = nullptr;
              n->RemoveConnectionRow(this, r);
            }
            layoutDirty = true;
            removed_any = true;
            break;
          }
        }
        if (removed_any) break;
      }
      if (removed_any) {
        // consume this press; avoid starting other right-click ops this frame
      }
    }

    // Begin drag from input pin to move existing connection (left button)
    if (!drag.active && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
      for (auto& n_target : nodes) {
        float ix = n_target->window.translation.x + 6.0f;
        for (auto& il : n_target->inputLabels) {
          float iy = il->translation.y + il->scale.y * 0.5f;
          float dx = p.x - ix;
          float dy = p.y - iy;
          if (dx * dx + dy * dy <= pinDetectR * pinDetectR) {
            // find a connection that targets this input
            Node::ConnectionUI* found = nullptr;
            Node* owner = nullptr;
            const std::string target_name = n_target->name;
            const std::string input_name = il->GetText();
            // prefer selectedConnection if matches
            if (selectedConnection && GetFieldText(selectedConnection->target) == target_name && GetFieldText(selectedConnection->input) == input_name) {
              // find owner
              for (auto& nsrc : nodes) {
                for (auto& cr : nsrc->connectionRows) if (cr.get() == selectedConnection) { owner = nsrc.get(); break; }
                if (owner) break;
              }
              found = selectedConnection;
            }
            if (!found) {
              for (auto& nsrc : nodes) {
                for (auto& cr : nsrc->connectionRows) {
                  if (GetFieldText(cr->target) == target_name && GetFieldText(cr->input) == input_name) { found = cr.get(); owner = nsrc.get(); break; }
                }
                if (found) break;
              }
            }
            if (found && owner) {
              const Node::OutputUI* src_orow = owner->FindOutputRow(found->outputName);
              if (src_orow) {
                float sx = owner->window.translation.x + owner->window.scale.x - 6.0f;
                float sy = src_orow->label.translation.y + src_orow->label.scale.y * 0.5f;
                drag.active = true;
                drag.rightButton = false;
                drag.fromAnchor = false;
                drag.srcNode = owner;
                drag.srcOutput = const_cast<Node::OutputUI*>(src_orow);
                drag.srcPos = XMFLOAT2(sx, sy);
                drag.cursor = p;
                drag.hoverNode = nullptr;
                drag.hoverInput = nullptr;
                drag.movingConnection = found;
                drag.movingOwner = owner;
                selectedConnection = found;
              }
            }
            break;
          }
        }
        if (drag.active) break;
      }
    }

    // Update drag
    if (drag.active) {
      drag.cursor = p;
      drag.hoverNode = nullptr;
      drag.hoverInput = nullptr;
      float bestDist2 = pinDetectR * pinDetectR;
      for (auto& n : nodes) {
        float ix = n->window.translation.x + 6.0f;
        for (auto& il : n->inputLabels) {
          float iy = il->translation.y + il->scale.y * 0.5f;
          float dx = p.x - ix;
          float dy = p.y - iy;
          float d2 = dx * dx + dy * dy;
          if (d2 <= bestDist2) {
            bestDist2 = d2;
            drag.hoverNode = n.get();
            drag.hoverInput = il.get();
          }
        }
      }
    }

    // Drop
    if (drag.active && ((drag.rightButton && !wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT)) || (!drag.rightButton && !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)))) {
      if (drag.hoverNode && drag.hoverInput && drag.srcNode && drag.srcOutput) {
        const std::string newTarget = drag.hoverNode->name;
        const std::string newInput = drag.hoverInput->GetText();
        std::string newParam;
        std::string newDelay;
        if (drag.movingConnection)
        {
          newParam = GetFieldText(drag.movingConnection->param);
          newDelay = GetFieldText(drag.movingConnection->delay);
        }
        else
        {
          // Defaults for a brand new connection row
          newParam = "";
          newDelay = "0.0";
        }
        bool exists = false;
        // For grouping into a macro when dragging from hub:
        bool deferBaseAdd = false; UndoCommand deferredBaseAdd;
        for (auto& cr : drag.srcNode->connectionRows) {
          if (cr->outputName != drag.srcOutput->name) continue;
          if (GetFieldText(cr->target) != newTarget) continue;
          if (GetFieldText(cr->input) != newInput) continue;
          if (GetFieldText(cr->param) != newParam) continue;
          if (GetFieldText(cr->delay) != newDelay) continue;
          exists = true; selectedConnection = cr.get(); break;
        }
        Node::ConnectionUI* baseRow = nullptr;
        if (drag.movingConnection) {
          // update existing connection instead of creating a new one
          if (!exists) {
            drag.movingConnection->target.SetValue(newTarget);
            drag.movingConnection->input.SetValue(newInput);
            // Treat programmatic changes as committed state for edit-undo baselines
            drag.movingConnection->lastTarget = newTarget;
            drag.movingConnection->lastInput = newInput;
            drag.movingOwner->LayoutRows();
            selectedConnection = drag.movingConnection;
          }
          baseRow = drag.movingConnection;
          layoutDirty = true;
        } else if (!exists) {
          auto* crow = drag.srcNode->AddConnectionRow(this, drag.srcOutput->name);
          if (crow) {
            crow->target.SetValue(newTarget);
            crow->input.SetValue(newInput);
            crow->param.SetValue("");
            crow->delay.SetValue("0.0");
            // Initialize last-committed mirrors to these values so next edit undo works
            crow->lastTarget = newTarget;
            crow->lastInput = newInput;
            crow->lastParam = "";
            crow->lastDelay = "0.0";
            selectedConnection = crow;
            drag.srcNode->LayoutRows();
            // Queue AddConnection for base row; if not from hub, push immediately
            deferredBaseAdd.type = UndoType::AddConnection; deferredBaseAdd.snap = MakeSnapshot(drag.srcNode, crow);
            if (drag.fromAnchor && anchorRight.conn && anchorRight.index >= 0 && anchorRight.index < (int)anchorRight.conn->anchorHubIds.size()) {
              deferBaseAdd = true; // will be appended to macro later
            } else {
              PushCommand(std::move(deferredBaseAdd));
              deferBaseAdd = false;
            }
          }
          baseRow = selectedConnection;
          layoutDirty = true;
        } else {
          // exists already
          baseRow = selectedConnection;
        }

        // If this connection was initiated from a shared hub, propagate to all owners of that hub
        if (drag.fromAnchor && anchorRight.conn && anchorRight.index >= 0 && anchorRight.index < (int)anchorRight.conn->anchorHubIds.size()) {
          // Begin macro transaction to group the whole gesture into one undo step
          UndoCommand macro; macro.type = UndoType::Macro; macro.label = "Connect from hub";
          if (deferBaseAdd) { macro.macro.push_back(std::move(deferredBaseAdd)); deferBaseAdd = false; }
          uint32_t groupHub = anchorRight.conn->anchorHubIds[anchorRight.index];
          const std::string outname = anchorRight.conn->outputName;
          // Ensure the base row on the source owner is bound to the shared hub
          if (baseRow) {
            bool hasHub = false;
            for (auto hid : baseRow->anchorHubIds) { if (hid == groupHub) { hasHub = true; break; } }
            if (!hasHub) {
              UndoCommand cmd; cmd.type = UndoType::SetConnectionHubs; cmd.before = MakeSnapshot(drag.srcNode, baseRow); cmd.before.hubIds = baseRow->anchorHubIds;
              baseRow->anchorHubIds.push_back(groupHub); if (auto* h = GetHub(groupHub)) { h->refcount++; }
              cmd.after = MakeSnapshot(drag.srcNode, baseRow); cmd.after.hubIds = baseRow->anchorHubIds;
              macro.macro.push_back(std::move(cmd));
            }
          }
          for (auto& n2 : nodes) {
            Node* owner2 = n2.get();
            if (owner2 == drag.srcNode) continue; // source handled as baseRow above
            bool owner_has_hub = false;
            for (auto& cr2 : owner2->connectionRows) {
              if (cr2->outputName != outname) continue;
              for (auto hid : cr2->anchorHubIds) { if (hid == groupHub) { owner_has_hub = true; break; } }
              if (owner_has_hub) break;
            }
            if (!owner_has_hub) continue;
            // Avoid duplicate connection rows
            bool exists2 = false;
            Node::ConnectionUI* row2 = nullptr;
            for (auto& crx : owner2->connectionRows) {
              if (crx->outputName != outname) continue;
              if (GetFieldText(crx->target) != newTarget) continue;
              if (GetFieldText(crx->input) != newInput) continue;
              if (GetFieldText(crx->param) != newParam) continue;
              if (GetFieldText(crx->delay) != newDelay) continue;
              exists2 = true; row2 = crx.get(); break;
            }
            if (!exists2) {
              auto* crow2 = owner2->AddConnectionRow(this, outname);
              if (crow2) {
                crow2->target.SetValue(newTarget);
                crow2->input.SetValue(newInput);
                crow2->param.SetValue(newParam);
                crow2->delay.SetValue(newDelay);
                crow2->lastTarget = newTarget;
                crow2->lastInput = newInput;
                crow2->lastParam = newParam;
                crow2->lastDelay = newDelay;
                // Bind to shared hub
                crow2->anchorHubIds.push_back(groupHub);
                if (auto* h = GetHub(groupHub)) { h->refcount++; }
                owner2->LayoutRows();
                // Queue AddConnection on owner2 into macro
                UndoCommand cmd; cmd.type = UndoType::AddConnection; cmd.snap = MakeSnapshot(owner2, crow2);
                macro.macro.push_back(std::move(cmd));
              }
              layoutDirty = true;
            } else if (row2) {
              // Ensure existing row is bound to shared hub
              bool hasHub2 = false;
              for (auto hid : row2->anchorHubIds) { if (hid == groupHub) { hasHub2 = true; break; } }
              if (!hasHub2) {
                UndoCommand cmd; cmd.type = UndoType::SetConnectionHubs; cmd.before = MakeSnapshot(owner2, row2); cmd.before.hubIds = row2->anchorHubIds;
                row2->anchorHubIds.push_back(groupHub); if (auto* h = GetHub(groupHub)) { h->refcount++; }
                cmd.after = MakeSnapshot(owner2, row2); cmd.after.hubIds = row2->anchorHubIds;
                macro.macro.push_back(std::move(cmd));
              }
            }
          }
          if (!macro.macro.empty()) {
            PushCommand(std::move(macro));
          }
        }
      }
      else if (drag.movingConnection && drag.movingOwner) {
        // dropped to nowhere: delete the moving connection
        drag.movingOwner->RemoveConnectionRow(this, drag.movingConnection);
        selectedConnection = nullptr;
        layoutDirty = true;
      }
      drag = DragState{};
    }
  }

  // Wire selection and reroute anchors:
  {
    if (!drag.active) {
      XMFLOAT4 pointer = wi::input::GetPointer();
      XMFLOAT2 p = XMFLOAT2(pointer.x, pointer.y);

      // Hover preselection (closest wire under cursor):
      {
        const float baseThreshold = 12.0f; // logical pixels at 100% DPI
        const float threshold = baseThreshold * canvas.GetDPIScaling();
        const float thr2 = threshold * threshold;
        Node::ConnectionUI* best = nullptr;
        float best_d2 = thr2;
        for (auto& n : nodes) {
          for (auto& crow_uptr : n->connectionRows) {
            auto* crow = crow_uptr.get();
            std::string target_text = crow->target.GetText();
            if (target_text == "!self" || target_text.empty()) continue;
            wi::vector<const Node*> targets;
            if (auto itnode = nodeIndex.find(target_text); itnode != nodeIndex.end()) {
              for (auto* cand : itnode->second) if (cand != n.get()) targets.push_back(cand);
            }
            if (targets.empty()) continue;

            // Source pin pos
            const Node::OutputUI* src_orow = n->FindOutputRow(crow->outputName);
            if (!src_orow) continue;
            float sx = 0, sy = 0; bool foundCache = false;
            for (auto& co : n->cachedOutputPins) { if (co.row == src_orow) { sx = co.pos.x; sy = co.pos.y; foundCache = true; break; } }
            if (!foundCache) {
              float headerRight = std::max(src_orow->label.translation.x + src_orow->label.scale.x, src_orow->addButton.translation.x + src_orow->addButton.scale.x);
              sx = headerRight + 6.0f; sy = src_orow->label.translation.y + src_orow->label.scale.y * 0.5f;
            }
            XMFLOAT2 srcpos(sx, sy);

            auto testSegment = [&](const XMFLOAT2& A, const XMFLOAT2& B) {
              XMFLOAT2 AB(B.x - A.x, B.y - A.y);
              float ab2 = AB.x * AB.x + AB.y * AB.y;
              float t = 0;
              if (ab2 > 0) t = std::max(0.f, std::min(1.f, ((p.x - A.x) * AB.x + (p.y - A.y) * AB.y) / ab2));
              XMFLOAT2 H(A.x + AB.x * t, A.y + AB.y * t);
              float dx = p.x - H.x; float dy = p.y - H.y; return dx * dx + dy * dy;
            };
            auto sub = [](const XMFLOAT2& a, const XMFLOAT2& b) { return XMFLOAT2(a.x - b.x, a.y - b.y); };
            auto addv = [](const XMFLOAT2& a, const XMFLOAT2& b) { return XMFLOAT2(a.x + b.x, a.y + b.y); };
            auto mul = [](const XMFLOAT2& a, float s) { return XMFLOAT2(a.x * s, a.y * s); };
            auto len = [](const XMFLOAT2& v) { return std::sqrt(v.x * v.x + v.y * v.y); };
            auto clamp_handle = [&](const XMFLOAT2& v, float minl, float maxl, float fallback_dirx) {
              float L = len(v);
              if (L < 1e-5f) {
                float Lc = std::max(minl, std::min(maxl, minl));
                return XMFLOAT2((fallback_dirx >= 0 ? 1.0f : -1.0f) * Lc, 0);
              }
              float Lc = std::max(minl, std::min(maxl, L));
              float s = Lc / L;
              return XMFLOAT2(v.x * s, v.y * s);
            };
            auto bezier_eval = [](const XMFLOAT2& p0, const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3, float t) -> XMFLOAT2 {
              const float it = 1.0f - t;
              const float a = it * it * it;
              const float b = 3.0f * it * it * t;
              const float c = 3.0f * it * t * t;
              const float d = t * t * t;
              return XMFLOAT2(
                a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                a * p0.y + b * p1.y + c * p2.y + d * p3.y);
            };
            auto segment_min_d2 = [&](const XMFLOAT2& P0, const XMFLOAT2& P1, XMFLOAT2 T0, XMFLOAT2 T1) -> float {
              const float minHandle = NodeEditorWindow::WIRE_MIN_HANDLE;
              const float clampK = NodeEditorWindow::WIRE_CLAMP_K;
              float seglen = len(sub(P1, P0));
              float maxHandle = std::max(minHandle, seglen * clampK);
              float dirx0 = (P1.x - P0.x) >= 0 ? 1.0f : -1.0f;
              float dirx1 = -dirx0;
              T0 = clamp_handle(T0, std::min(minHandle, maxHandle), maxHandle, dirx0);
              T1 = clamp_handle(T1, std::min(minHandle, maxHandle), maxHandle, dirx1);
              XMFLOAT2 b0 = P0;
              XMFLOAT2 b1 = addv(P0, mul(T0, 1.0f / 3.0f));
              XMFLOAT2 b2 = sub(P1, mul(T1, 1.0f / 3.0f));
              XMFLOAT2 b3 = P1;
              const int samples = 24;
              XMFLOAT2 prev = b0;
              float local_best2 = FLT_MAX;
              for (int s = 1; s <= samples; ++s) {
                float t = (float)s / (float)samples;
                XMFLOAT2 cur = bezier_eval(b0, b1, b2, b3, t);
                float d2 = testSegment(prev, cur);
                if (d2 < local_best2) local_best2 = d2;
                prev = cur;
              }
              return local_best2;
            };

            // Shared path
            wi::vector<XMFLOAT2> shared_pts;
            shared_pts.push_back(srcpos);
            for (size_t i = 0; i < crow->anchorHubIds.size(); ++i) {
              const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
              shared_pts.push_back(XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y));
            }
            if (shared_pts.size() >= 2) {
              wi::vector<XMFLOAT2> compact;
              compact.reserve(shared_pts.size());
              auto near_eq = [](const XMFLOAT2& a, const XMFLOAT2& b){ float dx=a.x-b.x, dy=a.y-b.y; return dx*dx+dy*dy <= 1.0f; };
              for (const auto& sp : shared_pts) { if (compact.empty() || !near_eq(compact.back(), sp)) compact.push_back(sp); }
              if (compact.size() >= 2) shared_pts = std::move(compact);
            }

            const float endpointBias = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
            float conn_best2 = FLT_MAX;
            for (size_t i = 0; i + 1 < shared_pts.size(); ++i) {
              const XMFLOAT2& P0 = shared_pts[i];
              const XMFLOAT2& P1 = shared_pts[i + 1];
              XMFLOAT2 T0, T1;
              if (i == 0) { T0 = XMFLOAT2(+endpointBias, 0); }
              else { const XMFLOAT2& Pm1 = shared_pts[i - 1]; T0 = mul(sub(shared_pts[i + 1], Pm1), 0.5f); }
              if (i + 1 < shared_pts.size() - 1) { const XMFLOAT2& Pp2 = shared_pts[i + 2]; T1 = mul(sub(Pp2, P0), 0.5f); }
              else { T1 = mul(sub(P1, P0), 0.5f); }
              float d2 = segment_min_d2(P0, P1, T0, T1);
              if (d2 < conn_best2) conn_best2 = d2;
            }

            // Final legs to targets
            const bool has_prev = shared_pts.size() >= 2;
            const XMFLOAT2 shared_last = shared_pts.empty() ? srcpos : shared_pts.back();
            const XMFLOAT2 shared_prev = has_prev ? shared_pts[shared_pts.size() - 2] : shared_last;
            const std::string input_name = crow->input.GetText();
            for (const Node* target_node : targets) {
              const wi::gui::Label* target_input_label = nullptr;
              size_t target_input_index = (size_t)-1;
              if (!input_name.empty()) {
                if (target_node->GetInputIndex(input_name, target_input_index)) {
                  target_input_label = target_node->inputLabels[target_input_index].get();
                }
              }
              float ex, ey;
              if (target_input_label) {
                if (target_input_index < target_node->cachedInputPins.size()) { ex = target_node->cachedInputPins[target_input_index].x; ey = target_node->cachedInputPins[target_input_index].y; }
                else { ex = target_node->window.translation.x + 6.0f; ey = target_input_label->translation.y + target_input_label->scale.y * 0.5f; }
              } else { ex = target_node->window.translation.x + 6.0f; ey = target_node->window.translation.y + target_node->window.scale.y * 0.5f; }
              const float endpointBias2 = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
              const XMFLOAT2 P0 = shared_last; const XMFLOAT2 P1 = XMFLOAT2(ex, ey);
              XMFLOAT2 T0 = has_prev ? mul(sub(P1, shared_prev), 0.5f) : XMFLOAT2(+endpointBias2, 0);
              XMFLOAT2 T1 = XMFLOAT2(+endpointBias2, 0);
              float d2 = segment_min_d2(P0, P1, T0, T1);
              if (d2 < conn_best2) conn_best2 = d2;
            }
            if (conn_best2 < best_d2) { best_d2 = conn_best2; best = crow; }
          }
        }
        hoveredConnection = (best && best_d2 <= thr2) ? best : nullptr;
      }

      // Anchor drag begin (left-click) and anchor right operations (right-click)
      if (!anchorDrag.active && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
        for (auto& n : nodes) {
          for (auto& crow_uptr : n->connectionRows) {
            auto* crow = crow_uptr.get();
            for (int i = 0; i < (int)crow->anchorHubIds.size(); ++i) {
              const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
              XMFLOAT2 ap(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y);
              float dx = p.x - ap.x;
              float dy = p.y - ap.y;
              if (dx * dx + dy * dy <= 8.0f * 8.0f) {
                selectedConnection = crow;
                anchorDrag.active = true;
                anchorDrag.conn = crow;
                anchorDrag.index = i;
                anchorDrag.oldPos = hub->pos;
                break;
              }
            }
            if (anchorDrag.active) break;
          }
          if (anchorDrag.active) break;
        }
      }

      if (!anchorRight.active && wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT)) {
        // detect right press over anchor to start right op (drag to connect, click to delete)
        for (auto& n : nodes) {
          for (auto& crow_uptr : n->connectionRows) {
            auto* crow = crow_uptr.get();
            for (int i = 0; i < (int)crow->anchorHubIds.size(); ++i) {
              const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
              XMFLOAT2 ap(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y);
              float dx = p.x - ap.x;
              float dy = p.y - ap.y;
              if (dx * dx + dy * dy <= 8.0f * 8.0f) {
                selectedConnection = crow;
                anchorRight.active = true;
                anchorRight.moved = false;
                anchorRight.conn = crow;
                anchorRight.node = nullptr;
                // find owning node pointer 'n' for this crow
                anchorRight.node = nullptr;
                for (auto& n2 : nodes) {
                  for (auto& cr2 : n2->connectionRows) if (cr2.get() == crow) { anchorRight.node = n2.get(); break; }
                  if (anchorRight.node) break;
                }
                anchorRight.index = i;
                anchorRight.start = p;
                break;
              }
            }
            if (anchorRight.active) break;
          }
          if (anchorRight.active) break;
        }
      }

      // Anchor drag update
      if (anchorDrag.active) {
        XMFLOAT2 local = XMFLOAT2(p.x - scrollable_area.translation.x, p.y - scrollable_area.translation.y);
        if (anchorDrag.conn && anchorDrag.index >= 0 && anchorDrag.index < (int)anchorDrag.conn->anchorHubIds.size()) {
          auto* hub = GetHub(anchorDrag.conn->anchorHubIds[anchorDrag.index]);
          if (hub) hub->pos = local;
        }
        if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)) {
          // Left release: attempt merge with other anchors of same output across all nodes (manual combine)
          if (anchorDrag.conn) {
            const std::string outname = anchorDrag.conn->outputName;
            auto* myhub = GetHub(anchorDrag.conn->anchorHubIds[anchorDrag.index]);
            XMFLOAT2 my = myhub ? myhub->pos : XMFLOAT2(0,0);
            const float mergeThr2 = 10.0f * 10.0f;
            bool merged = false;
            for (auto& n2 : nodes) {
              for (auto& cr2_uptr : n2->connectionRows) {
                auto* cr2 = cr2_uptr.get();
                if (cr2 == anchorDrag.conn) continue;
                if (cr2->outputName != outname) continue;
                for (size_t j = 0; j < cr2->anchorHubIds.size(); ++j) {
                  auto* otherhub = GetHub(cr2->anchorHubIds[j]); if (!otherhub) continue;
                  XMFLOAT2 other = otherhub->pos;
                  float dx = my.x - other.x; float dy = my.y - other.y;
                  if (dx * dx + dy * dy <= mergeThr2) {
                    // rebind to shared hub id: dec old, inc new (with undo)
                    uint32_t old = anchorDrag.conn->anchorHubIds[anchorDrag.index];
                    UndoCommand cmd; cmd.type = UndoType::SetConnectionHubs; cmd.before = MakeSnapshot(nullptr, anchorDrag.conn); cmd.before.node_uid = 0; // will be fixed below
                    // fill node uid
                    for (auto& nfill : nodes) { for (auto& crfill : nfill->connectionRows) if (crfill.get() == anchorDrag.conn) { cmd.before.node_uid = nfill->uid; break; } if (cmd.before.node_uid) break; }
                    if (auto* hpre = GetHub(old)) { if (hpre->refcount == 1) { UndoCommand::HubCreate hc; hc.id = old; hc.pos = hpre->pos; cmd.hubs_undo_create.push_back(hc); } }
                    if (auto* h = GetHub(old)) { if (h->refcount > 0) h->refcount--; }
                    uint32_t newid = cr2->anchorHubIds[j];
                    anchorDrag.conn->anchorHubIds[anchorDrag.index] = newid;
                    if (auto* h2 = GetHub(newid)) { h2->refcount++; }
                    DeleteHubIfUnreferenced(old);
                    cmd.after = cmd.before; cmd.after.hubIds = anchorDrag.conn->anchorHubIds;
                    PushCommand(std::move(cmd));
                    merged = true; break;
                  }
                }
                if (merged) break;
              }
              if (merged) break;
            }
          }
          // Also register hub move as an undoable command when position changed
          if (anchorDrag.conn && anchorDrag.index >= 0 && anchorDrag.index < (int)anchorDrag.conn->anchorHubIds.size()) {
            auto* hub = GetHub(anchorDrag.conn->anchorHubIds[anchorDrag.index]);
            if (hub && (hub->pos.x != anchorDrag.oldPos.x || hub->pos.y != anchorDrag.oldPos.y)) {
              UndoCommand cmd; cmd.type = UndoType::MoveHub; cmd.movehub.hubId = anchorDrag.conn->anchorHubIds[anchorDrag.index]; cmd.movehub.from = anchorDrag.oldPos; cmd.movehub.to = hub->pos; PushCommand(std::move(cmd));
            }
          }
          anchorDrag = {};
        }
      }

      // Right op update: drag to connect or click to delete
      if (anchorRight.active) {
        if (wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT)) {
          XMFLOAT2 dp = XMFLOAT2(p.x - anchorRight.start.x, p.y - anchorRight.start.y);
          float d2 = dp.x * dp.x + dp.y * dp.y;
          if (!anchorRight.moved && d2 > 9.0f) {
            // begin connection drag from anchor
            if (anchorRight.node && anchorRight.conn) {
              drag.active = true;
              drag.fromAnchor = true;
              drag.rightButton = true;
              drag.srcNode = anchorRight.node;
              drag.srcOutput = anchorRight.node->FindOutputRow(anchorRight.conn->outputName);
              auto* hub = GetHub(anchorRight.conn->anchorHubIds[anchorRight.index]);
              XMFLOAT2 ap = hub ? XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y) : p;
              drag.srcPos = ap;
              drag.cursor = p;
              anchorRight.moved = true;
            }
          }
        } else {
          // right released
          if (!anchorRight.moved && anchorRight.conn && anchorRight.index >= 0 && anchorRight.index < (int)anchorRight.conn->anchorHubIds.size()) {
            // treat as delete anchor
            uint32_t hid = anchorRight.conn->anchorHubIds[anchorRight.index];
            // dec ref before removing
            // Undo: capture hub list change
            UndoCommand cmd; cmd.type = UndoType::SetConnectionHubs; cmd.before = MakeSnapshot(anchorRight.node, anchorRight.conn); cmd.before.hubIds = anchorRight.conn->anchorHubIds;
            if (auto* hcheck = GetHub(hid)) { if (hcheck->refcount == 1) { UndoCommand::HubCreate hc; hc.id = hid; hc.pos = hcheck->pos; cmd.hubs_undo_create.push_back(hc); } }
            if (auto* h = GetHub(hid)) { if (h->refcount > 0) h->refcount--; }
            anchorRight.conn->anchorHubIds.erase(anchorRight.conn->anchorHubIds.begin() + anchorRight.index);
            DeleteHubIfUnreferenced(hid);
            cmd.after = MakeSnapshot(anchorRight.node, anchorRight.conn); cmd.after.hubIds = anchorRight.conn->anchorHubIds; PushCommand(std::move(cmd));
          }
          anchorRight = {};
        }
      }

      // Wire selection or anchor add
      if (!anchorDrag.active && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
        // Hit-test wires by sampling the same tangent-aware bezier chain used for rendering
        // Use closest-connection selection with DPI-aware threshold
        const float baseThreshold = 9.0f; // logical pixels at 100% DPI
        const float threshold = baseThreshold * canvas.GetDPIScaling();
        const float thr2 = threshold * threshold;
        Node::ConnectionUI* best = nullptr;
        float best_d2 = thr2;
        for (auto& n : nodes) {
          for (auto& crow_uptr : n->connectionRows) {
            auto* crow = crow_uptr.get();
            // Determine targets by index (support duplicates)
            std::string target_text = crow->target.GetText();
            if (target_text == "!self" || target_text.empty()) continue;
            wi::vector<const Node*> targets;
            if (auto itnode = nodeIndex.find(target_text); itnode != nodeIndex.end()) {
              for (auto* cand : itnode->second) if (cand != n.get()) targets.push_back(cand);
            }
            if (targets.empty()) continue;

            // Resolve source pin position (prefer cache)
            const Node::OutputUI* src_orow = n->FindOutputRow(crow->outputName);
            if (!src_orow) continue;
            float sx = 0, sy = 0; bool foundCache = false;
            for (auto& co : n->cachedOutputPins) { if (co.row == src_orow) { sx = co.pos.x; sy = co.pos.y; foundCache = true; break; } }
            if (!foundCache) {
              float headerRight = std::max(src_orow->label.translation.x + src_orow->label.scale.x, src_orow->addButton.translation.x + src_orow->addButton.scale.x);
              sx = headerRight + 6.0f;
              sy = src_orow->label.translation.y + src_orow->label.scale.y * 0.5f;
            }
            XMFLOAT2 srcpos(sx, sy);

            auto testSegment = [&](const XMFLOAT2& A, const XMFLOAT2& B) {
              XMFLOAT2 AB(B.x - A.x, B.y - A.y);
              float ab2 = AB.x * AB.x + AB.y * AB.y;
              float t = 0;
              if (ab2 > 0) t = std::max(0.f, std::min(1.f, ((p.x - A.x) * AB.x + (p.y - A.y) * AB.y) / ab2));
              XMFLOAT2 H(A.x + AB.x * t, A.y + AB.y * t);
              float dx = p.x - H.x; float dy = p.y - H.y; return dx * dx + dy * dy;
            };
            auto sub = [](const XMFLOAT2& a, const XMFLOAT2& b) { return XMFLOAT2(a.x - b.x, a.y - b.y); };
            auto addv = [](const XMFLOAT2& a, const XMFLOAT2& b) { return XMFLOAT2(a.x + b.x, a.y + b.y); };
            auto mul = [](const XMFLOAT2& a, float s) { return XMFLOAT2(a.x * s, a.y * s); };
            auto len = [](const XMFLOAT2& v) { return std::sqrt(v.x * v.x + v.y * v.y); };
            auto clamp_handle = [&](const XMFLOAT2& v, float minl, float maxl, float fallback_dirx) {
              float L = len(v);
              if (L < 1e-5f) {
                float Lc = std::max(minl, std::min(maxl, minl));
                return XMFLOAT2((fallback_dirx >= 0 ? 1.0f : -1.0f) * Lc, 0);
              }
              float Lc = std::max(minl, std::min(maxl, L));
              float s = Lc / L;
              return XMFLOAT2(v.x * s, v.y * s);
            };
            auto bezier_eval = [](const XMFLOAT2& p0, const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3, float t) -> XMFLOAT2 {
              const float it = 1.0f - t;
              const float a = it * it * it;
              const float b = 3.0f * it * it * t;
              const float c = 3.0f * it * t * t;
              const float d = t * t * t;
              return XMFLOAT2(
                a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                a * p0.y + b * p1.y + c * p2.y + d * p3.y);
            };
            auto segment_min_d2 = [&](const XMFLOAT2& P0, const XMFLOAT2& P1, XMFLOAT2 T0, XMFLOAT2 T1) -> float {
              const float minHandle = NodeEditorWindow::WIRE_MIN_HANDLE;
              const float clampK = NodeEditorWindow::WIRE_CLAMP_K;
              float seglen = len(sub(P1, P0));
              float maxHandle = std::max(minHandle, seglen * clampK);
              float dirx0 = (P1.x - P0.x) >= 0 ? 1.0f : -1.0f;
              float dirx1 = -dirx0;
              T0 = clamp_handle(T0, std::min(minHandle, maxHandle), maxHandle, dirx0);
              T1 = clamp_handle(T1, std::min(minHandle, maxHandle), maxHandle, dirx1);
              XMFLOAT2 b0 = P0;
              XMFLOAT2 b1 = addv(P0, mul(T0, 1.0f / 3.0f));
              XMFLOAT2 b2 = sub(P1, mul(T1, 1.0f / 3.0f));
              XMFLOAT2 b3 = P1;
              const int samples = 24;
              XMFLOAT2 prev = b0;
              float local_best2 = FLT_MAX;
              for (int s = 1; s <= samples; ++s) {
                float t = (float)s / (float)samples;
                XMFLOAT2 cur = bezier_eval(b0, b1, b2, b3, t);
                float d2 = testSegment(prev, cur);
                if (d2 < local_best2) local_best2 = d2;
                prev = cur;
              }
              return local_best2;
            };

            // Shared path (source -> hubs): build once and compute min distance once
            wi::vector<XMFLOAT2> shared_pts;
            shared_pts.push_back(srcpos);
            for (size_t i = 0; i < crow->anchorHubIds.size(); ++i) {
              const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
              shared_pts.push_back(XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y));
            }
            if (shared_pts.size() >= 2) {
              wi::vector<XMFLOAT2> compact;
              compact.reserve(shared_pts.size());
              auto near_eq = [](const XMFLOAT2& a, const XMFLOAT2& b){ float dx=a.x-b.x, dy=a.y-b.y; return dx*dx+dy*dy <= 1.0f; };
              for (const auto& sp : shared_pts) { if (compact.empty() || !near_eq(compact.back(), sp)) compact.push_back(sp); }
              if (compact.size() >= 2) shared_pts = std::move(compact);
            }

            const float endpointBias = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
            float conn_best2 = FLT_MAX;
            for (size_t i = 0; i + 1 < shared_pts.size(); ++i) {
              const XMFLOAT2& P0 = shared_pts[i];
              const XMFLOAT2& P1 = shared_pts[i + 1];
              XMFLOAT2 T0, T1;
              if (i == 0) {
                T0 = XMFLOAT2(+endpointBias, 0);
              } else {
                const XMFLOAT2& Pm1 = shared_pts[i - 1];
                T0 = mul(sub(shared_pts[i + 1], Pm1), 0.5f);
              }
              if (i + 1 < shared_pts.size() - 1) {
                const XMFLOAT2& Pp2 = shared_pts[i + 2];
                T1 = mul(sub(Pp2, P0), 0.5f);
              } else {
                T1 = mul(sub(P1, P0), 0.5f);
              }
              float d2 = segment_min_d2(P0, P1, T0, T1);
              if (d2 < conn_best2) conn_best2 = d2;
            }
            // Continue to test final legs per target and keep best distance
            const std::string input_name = crow->input.GetText();
            // Otherwise test only the final leg (last shared -> target) per target
            const bool has_prev = shared_pts.size() >= 2;
            const XMFLOAT2 shared_last = shared_pts.empty() ? srcpos : shared_pts.back();
            const XMFLOAT2 shared_prev = has_prev ? shared_pts[shared_pts.size() - 2] : shared_last;
            for (const Node* target_node : targets) {
              const wi::gui::Label* target_input_label = nullptr;
              size_t target_input_index = (size_t)-1;
              if (!input_name.empty()) {
                if (target_node->GetInputIndex(input_name, target_input_index)) {
                  target_input_label = target_node->inputLabels[target_input_index].get();
                }
              }
              float ex, ey;
              if (target_input_label) {
                if (target_input_index < target_node->cachedInputPins.size()) {
                  ex = target_node->cachedInputPins[target_input_index].x; ey = target_node->cachedInputPins[target_input_index].y;
                } else {
                  ex = target_node->window.translation.x + 6.0f;
                  ey = target_input_label->translation.y + target_input_label->scale.y * 0.5f;
                }
              } else {
                ex = target_node->window.translation.x + 6.0f;
                ey = target_node->window.translation.y + target_node->window.scale.y * 0.5f;
              }
              const float endpointBias2 = NodeEditorWindow::WIRE_ENDPOINT_BIAS;
              const XMFLOAT2 P0 = shared_last;
              const XMFLOAT2 P1 = XMFLOAT2(ex, ey);
              XMFLOAT2 T0, T1;
              if (has_prev) {
                T0 = mul(sub(P1, shared_prev), 0.5f);
              } else {
                T0 = XMFLOAT2(+endpointBias2, 0);
              }
              T1 = XMFLOAT2(+endpointBias2, 0);
              float d2 = segment_min_d2(P0, P1, T0, T1);
              if (d2 < conn_best2) conn_best2 = d2;
            }
            if (conn_best2 < best_d2) { best_d2 = conn_best2; best = crow; }
          }
          // don't break early; choose closest across all
        }
        selectedConnection = (best && best_d2 <= thr2) ? best : nullptr;
      }
      // Delete selected connection with Delete key (skip when editing its fields)
      if (selectedConnection && wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE)) {
        bool anyActive = false;
        if (selectedConnection->target.GetState() == wi::gui::ACTIVE) anyActive = true;
        if (selectedConnection->input.GetState() == wi::gui::ACTIVE) anyActive = true;
        if (selectedConnection->param.GetState() == wi::gui::ACTIVE) anyActive = true;
        if (selectedConnection->delay.GetState() == wi::gui::ACTIVE) anyActive = true;
        if (!anyActive) {
          for (auto& n : nodes) {
            for (auto& cr : n->connectionRows) {
              if (cr.get() == selectedConnection) {
                UndoCommand cmd; cmd.type = UndoType::RemoveConnection; cmd.snap = MakeSnapshot(n.get(), selectedConnection);
                PushCommand(std::move(cmd));
                n->RemoveConnectionRow(this, selectedConnection);
                selectedConnection = nullptr;
                break;
              }
            }
            if (!selectedConnection) break;
          }
        }
      }

      // Add anchor on right click when a wire is selected and not clicking an anchor
      if (!anchorRight.active && selectedConnection && wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT)) {
        XMFLOAT4 pointer2 = wi::input::GetPointer();
        XMFLOAT2 p2 = XMFLOAT2(pointer2.x, pointer2.y);
        XMFLOAT2 local = XMFLOAT2(p2.x - scrollable_area.translation.x, p2.y - scrollable_area.translation.y);
        uint32_t hid = CreateHub(local);
        if (auto* h = GetHub(hid)) { h->refcount++; }
        // Undoable add hub
        UndoCommand cmd; cmd.type = UndoType::SetConnectionHubs; cmd.before = MakeSnapshot(nullptr, selectedConnection);
        for (auto& nn : nodes) { for (auto& cr : nn->connectionRows) if (cr.get() == selectedConnection) { cmd.before.node_uid = nn->uid; break; } if (cmd.before.node_uid) break; }
        selectedConnection->anchorHubIds.push_back(hid);
        cmd.after = cmd.before; cmd.after.hubIds = selectedConnection->anchorHubIds;
        // ensure redo can recreate the hub after an undo
        UndoCommand::HubCreate hc; hc.id = hid; hc.pos = local; cmd.hubs_redo_create.push_back(hc);
        PushCommand(std::move(cmd));
      }
    }
  }

  // Unify duplicate connections per node only (no cross-node auto combine):
  // Ensure one representative per (output,target,input,param,delay) within a node and unify hubs accordingly.
  for (auto& n : nodes) {
    std::unordered_map<std::string, Node::ConnectionUI*> rep;
    for (auto& cr_uptr : n->connectionRows) {
      auto* cr = cr_uptr.get();
      const std::string key = cr->outputName + "\n" + GetFieldText(cr->target) + "\n" + GetFieldText(cr->input) + "\n" + GetFieldText(cr->param) + "\n" + GetFieldText(cr->delay);
      auto it = rep.find(key);
      if (it == rep.end()) {
        rep.emplace(key, cr);
      } else {
        Node::ConnectionUI* master = it->second;
        if (selectedConnection == cr) selectedConnection = master;
        if (cr->anchorHubIds != master->anchorHubIds) {
          for (auto hid : cr->anchorHubIds) {
            if (auto* h = GetHub(hid)) { if (h->refcount > 0) h->refcount--; }
            DeleteHubIfUnreferenced(hid);
          }
          cr->anchorHubIds = master->anchorHubIds;
          for (auto hid : cr->anchorHubIds) {
            if (auto* h = GetHub(hid)) { h->refcount++; }
          }
        }
      }
    }
  }

  // Live autosize: per-node dirty while fields are active, with throttling.
  // Also run shrink debounce logic when content allows shrinking
  {
    // Update activeEditing/needsLayout per node and throttle timers
    for (auto& n : nodes) {
      bool active = false;
      for (auto& cr : n->connectionRows) {
        if (cr->target.GetState() == wi::gui::ACTIVE) { active = true; break; }
        if (cr->input.GetState()  == wi::gui::ACTIVE) { active = true; break; }
        if (cr->param.GetState()  == wi::gui::ACTIVE) { active = true; break; }
        if (cr->delay.GetState()  == wi::gui::ACTIVE) { active = true; break; }
      }
      n->activeEditing = active;
      if (active) n->needsLayout = true;
      if (n->autosizeThrottleTimer > 0.0f) {
        n->autosizeThrottleTimer = std::max(0.0f, n->autosizeThrottleTimer - dt);
      }
    }

    // Per-node throttled relayout
    for (auto& n : nodes) {
      if (n->needsLayout && n->autosizeThrottleTimer <= 0.0f) {
        n->LayoutRows();
        n->pinCacheDirty = true;
        n->autosizeThrottleTimer = NODE_AUTOSIZE_THROTTLE;
        // keep needsLayout true while editing so it keeps reflowing on throttle
      }
    }

    // Debounced shrink per node
    const float start_x = 6.0f; // must match Node::LayoutRows padding
    for (auto& n : nodes) {
      const float current_content_w = n->window.GetSize().x - start_x * 2.0f;
      const float desired_w = std::max(NODE_MIN_CONTENT_W, std::min(n->measuredContentWidth, NODE_MAX_CONTENT_W));
      if (desired_w + 1.0f < current_content_w) { // early exit threshold
        n->autosizeShrinkTimer += dt;
        if (n->autosizeShrinkTimer >= NODE_AUTOSIZE_SHRINK_DELAY) {
          const XMFLOAT2 cur = n->window.GetSize();
          n->window.SetSize(XMFLOAT2(start_x * 2.0f + desired_w, cur.y));
          n->pinCacheDirty = true;
          layoutDirty = true;
          n->autosizeShrinkTimer = 0.0f;
        }
      } else {
        n->autosizeShrinkTimer = 0.0f;
      }
    }
  }

  // If any operation marked layout dirty, re-flow node UIs before pin caching
  if (layoutDirty) {
    for (auto& n : nodes) {
      n->LayoutRows();
      n->needsLayout = false;
      n->autosizeThrottleTimer = 0.0f; // allow immediate next relayout
    }
  }

  // Recompute pin caches after all updates/layouts for this frame
  for (auto& n : nodes) {
    n->ComputePinCache();
  }

  // Process any node removals queued during child updates (e.g., close button)
  if (!pendingRemoval.empty()) {
    // Copy to avoid side effects while removing
    auto toRemove = pendingRemoval;
    pendingRemoval.clear();
    for (auto* n : toRemove) {
      RemoveNode(n);
    }
  }

  // Ensure child node windows finalize layout and recompute pin caches for nodes that need it
  for (auto& n : nodes) {
    // mark dirty if window moved
    if (n->lastWindowPos.x != n->window.translation.x || n->lastWindowPos.y != n->window.translation.y) {
      n->pinCacheDirty = true;
      n->lastWindowPos = XMFLOAT2(n->window.translation.x, n->window.translation.y);
    }
    if (n->pinCacheDirty) {
      n->window.Update(canvas, 0);
      n->ComputePinCache();
      n->pinCacheDirty = false;
    }
  }
  layoutDirty = false;
}

void NodeEditorWindow::ResizeLayout() {
  wi::gui::Window::ResizeLayout();
  const float padding = 4;
  // No automatic relayout on scroll/resize; layout only when explicitly requested

  addNodeButton.Detach();
  addNodeButton.SetPos(
      XMFLOAT2(translation.x + padding,
               translation.y + scale.y - addNodeButton.GetSize().y - padding));
  addNodeButton.SetSize(
      XMFLOAT2(separator - padding * 2, addNodeButton.GetSize().y));
  addNodeButton.AttachTo(this);

  importFromSceneButton.Detach();
  importFromSceneButton.SetSize(XMFLOAT2(separator - padding * 2, importFromSceneButton.GetSize().y));
  importFromSceneButton.SetPos(XMFLOAT2(
      translation.x + padding,
      addNodeButton.GetPos().y - importFromSceneButton.GetSize().y - padding));
  importFromSceneButton.AttachTo(this);

  addSequenceButton.Detach();
  addSequenceButton.SetSize(XMFLOAT2(separator - padding * 2, addSequenceButton.GetSize().y));
  addSequenceButton.SetPos(XMFLOAT2(
      translation.x + padding,
      importFromSceneButton.GetPos().y - addSequenceButton.GetSize().y - padding));
  addSequenceButton.AttachTo(this);

  addTimerButton.Detach();
  addTimerButton.SetSize(XMFLOAT2(separator - padding * 2, addTimerButton.GetSize().y));
  addTimerButton.SetPos(XMFLOAT2(
      translation.x + padding,
      addSequenceButton.GetPos().y - addTimerButton.GetSize().y - padding));
  addTimerButton.AttachTo(this);

  saveGraphButton.Detach();
  saveGraphButton.SetSize(XMFLOAT2(separator - padding * 2, saveGraphButton.GetSize().y));
  saveGraphButton.SetPos(XMFLOAT2(
      translation.x + padding,
      addTimerButton.GetPos().y - saveGraphButton.GetSize().y - padding));
  saveGraphButton.AttachTo(this);

  loadGraphButton.Detach();
  loadGraphButton.SetSize(XMFLOAT2(separator - padding * 2, loadGraphButton.GetSize().y));
  loadGraphButton.SetPos(XMFLOAT2(
      translation.x + padding,
      saveGraphButton.GetPos().y - loadGraphButton.GetSize().y - padding));
  loadGraphButton.AttachTo(this);

  if (recentlyAddedNewNode)
  {
      recentlyAddedNewNode = false;
      if (lastAddedNode != nullptr)
      {
          // Center the last added node within the scrollable area
          const float width = GetWidgetAreaSize().x;
          const float height = GetWidgetAreaSize().y;
          const float area_left = translation.x + separator + 10;
          const float area_top = translation.y + control_size + padding + 10;
          const float area_right = translation.x + width;
          const float area_bottom = translation.y + height;
          const float area_w = std::max(0.0f, area_right - area_left);
          const float area_h = std::max(0.0f, area_bottom - area_top);

          const XMFLOAT2 nsize = lastAddedNode->window.GetSize();
          const float nx = area_left + (area_w - nsize.x) * 0.5f;
          const float ny = area_top + (area_h - nsize.y) * 0.5f;

          lastAddedNode->window.Detach();
          lastAddedNode->window.SetPos(XMFLOAT2(nx, ny));
          lastAddedNode->window.AttachTo(&scrollable_area);
          lastAddedNode = nullptr;
      }
  }

  // Update per-node internal UI layouts to adapt to width changes
  for (auto& node : nodes) {
    node->LayoutRows();
  }

  // Handle local Undo/Redo shortcuts when Node Editor is visible
  if (IsVisible())
  {
    bool ctrl = wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) || wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
    if (ctrl)
    {
      // Avoid when typing into any GUI field
      if (!(editor && editor->GetGUI().IsTyping()))
      {
        if (wi::input::Press((wi::input::BUTTON)'Z')) { Undo(); }
        if (wi::input::Press((wi::input::BUTTON)'Y') || (wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) && wi::input::Press((wi::input::BUTTON)'Z'))) { Redo(); }
      }
    }
  }
  
}

void NodeEditorWindow::AddNode() {
  std::string name = "Node " + std::to_string(nodes.size() + 1);
  auto node = std::make_unique<Node>(name);
  node->uid = nextNodeUid++;
  node->type = Node::NodeType::LogicOnly;
  node->window.Create(
      name,
	  Window::WindowControls::MOVE |
	  Window::WindowControls::CLOSE |
	  Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL/* |
	  Window::WindowControls::RESIZE_LEFT |
	  Window::WindowControls::RESIZE_RIGHT*/);
  node->window.SetSize(XMFLOAT2(NODE_MIN_CONTENT_W, 110));

  node->label.Create(name);
  node->label.SetText(name);
  node->label.SetPos(XMFLOAT2(4, 4));
  node->label.SetSize(XMFLOAT2(112, 20));
  node->window.AddWidget(&node->label);

  // Seed example I/O sets (visual only). You can populate from Lua later.
  node->outputs.push_back("OnStart");
  // Example inputs (function names)
  node->inputs.push_back("Enable");
  node->inputs.push_back("Disable");

  for (auto& outname : node->outputs) {
    node->AddOutputRow(this, outname);
  }
  for (auto& inname : node->inputs) {
    auto lbl = std::make_unique<wi::gui::Label>();
    lbl->Create(inname);
    lbl->SetLocalizationEnabled(false);
    lbl->SetShadowRadius(0);
    lbl->SetText(inname);
    node->window.AddWidget(lbl.get());
    node->inputLabels.push_back(std::move(lbl));
    node->inputIndex[inname] = node->inputLabels.size() - 1;
  }
  node->LayoutRows();
  layoutDirty = true;

  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
  node->window.SetEnabled(true);
  node->window.SetVisible(true);
  if (editor) {
    editor->generalWnd.RefreshTheme();
  }

  // When the node window is closed, queue it for safe removal.
  Node* raw = node.get();
  node->window.OnClose([this, raw](wi::gui::EventArgs) {
    // Close button already hides the window; defer actual removal
    // to end of Update() to avoid mutating containers mid-iteration.
    pendingRemoval.push_back(raw);
  });

  nodes.push_back(std::move(node));
  RegisterNode(nodes.back().get());
  // index by name for fast lookup (support duplicates)
  nodeIndex[nodes.back()->name].push_back(nodes.back().get());
  nodes.back()->pinCacheDirty = true;
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
}

void NodeEditorWindow::AddNodeForEntity(wi::ecs::Entity ent, const std::string& name) {
  if (ent == wi::ecs::INVALID_ENTITY) return;
  if (entityIndex.find(ent) != entityIndex.end()) return;

  auto node = std::make_unique<Node>(name);
  node->entity = ent;
  node->type = Node::NodeType::EntityBound;
  node->uid = nextNodeUid++;
  node->window.Create(
      name,
	  Window::WindowControls::MOVE |
	  Window::WindowControls::CLOSE |
	  Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL/* |
	  Window::WindowControls::RESIZE_LEFT |
	  Window::WindowControls::RESIZE_RIGHT*/);
  node->window.SetSize(XMFLOAT2(NODE_MIN_CONTENT_W, 100));

  node->label.Create(name);
  node->label.SetText(name);
  node->label.SetPos(XMFLOAT2(4, 4));
  node->label.SetSize(XMFLOAT2(112, 20));
  node->window.AddWidget(&node->label);

  // Seed I/O sets (can be customized by Lua later)
  node->outputs.push_back("OnStart");
  node->inputs.push_back("Enable");
  node->inputs.push_back("Disable");
  for (auto& outname : node->outputs) node->AddOutputRow(this, outname);
  for (auto& inname : node->inputs) {
    auto lbl = std::make_unique<wi::gui::Label>();
    lbl->Create(inname);
    lbl->SetLocalizationEnabled(false);
    lbl->SetShadowRadius(0);
    lbl->SetText(inname);
    node->window.AddWidget(lbl.get());
    node->inputLabels.push_back(std::move(lbl));
    node->inputIndex[inname] = node->inputLabels.size() - 1;
  }
  node->LayoutRows();

  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
  node->window.SetEnabled(true);
  node->window.SetVisible(true);
  if (editor) {
    editor->generalWnd.RefreshTheme();
  }

  Node* raw = node.get();
  node->window.OnClose([this, raw](wi::gui::EventArgs) {
    pendingRemoval.push_back(raw);
  });

  entityIndex[ent] = raw;
  nodes.push_back(std::move(node));
  RegisterNode(nodes.back().get());
  nodeIndex[raw->name].push_back(raw);
  // Center the newly added node like AddNode()
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
  layoutDirty = true;
  raw->pinCacheDirty = true;
}

// ----- Node UI helpers -----
void NodeEditorWindow::Node::AddOutputRow(NodeEditorWindow* owner, const std::string& outputName) {
  auto row = std::make_unique<OutputUI>();
  row->name = outputName;
  row->label.Create(outputName);
  row->label.SetLocalizationEnabled(false);
  row->label.SetShadowRadius(0);
  row->label.font.params.size = 14;
  row->label.SetWrapEnabled(false);
  window.AddWidget(&row->label);

  row->addButton.Create("Add");
  row->addButton.SetLocalizationEnabled(false);
  row->addButton.SetShadowRadius(0);
  row->addButton.SetSize(XMFLOAT2(40, 20));
  row->addButton.OnClick([this, owner, outputName](wi::gui::EventArgs) {
    auto* cr = this->AddConnectionRow(owner, outputName);
    if (cr && owner) {
      NodeEditorWindow::UndoCommand cmd; cmd.type = NodeEditorWindow::UndoType::AddConnection; cmd.snap = owner->MakeSnapshot(this, cr); owner->PushCommand(std::move(cmd));
    }
    this->LayoutRows();
    if (owner && owner->editor) {
      owner->editor->generalWnd.RefreshTheme();
    }
  });
  window.AddWidget(&row->addButton);

  outputRows.push_back(std::move(row));
  // update fast output index
  outputIndex[outputName] = outputRows.back().get();
  if (owner) owner->layoutDirty = true;
  pinCacheDirty = true;
}

NodeEditorWindow::Node::ConnectionUI* NodeEditorWindow::Node::AddConnectionRow(NodeEditorWindow* owner, const std::string& outputName, uint64_t forced_uid) {
  auto row = std::make_unique<ConnectionUI>();
  row->outputName = outputName;

  row->outLabel.Create(outputName);
  row->outLabel.SetLocalizationEnabled(false);
  row->outLabel.SetShadowRadius(0);
  row->outLabel.font.params.size = 16;
  row->outLabel.SetWrapEnabled(false);
  window.AddWidget(&row->outLabel);

  row->target.Create("Target");
  row->target.SetLocalizationEnabled(false);
  row->target.SetShadowRadius(0);
  row->target.SetValue("!self");
  row->target.OnInputAccepted([this, owner, ptr=row.get()](wi::gui::EventArgs){
    if (owner) {
      NodeEditorWindow::UndoCommand cmd; cmd.type = NodeEditorWindow::UndoType::EditConnection;
      cmd.before = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->lastInput, ptr->lastParam, ptr->lastDelay, ptr->anchorHubIds };
      cmd.after  = { this->uid, ptr->uid, ptr->outputName, ptr->target.GetValue(), ptr->lastInput, ptr->lastParam, ptr->lastDelay, ptr->anchorHubIds };
      owner->PushCommand(std::move(cmd));
      ptr->lastTarget = ptr->target.GetValue();
    }
    if(owner) owner->layoutDirty = true; this->needsLayout = true; });
  window.AddWidget(&row->target);

  row->input.Create("Input");
  row->input.SetLocalizationEnabled(false);
  row->input.SetShadowRadius(0);
  row->input.SetValue("FunctionName");
  row->input.OnInputAccepted([this, owner, ptr=row.get()](wi::gui::EventArgs){
    if (owner) {
      NodeEditorWindow::UndoCommand cmd; cmd.type = NodeEditorWindow::UndoType::EditConnection;
      cmd.before = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->lastInput, ptr->lastParam, ptr->lastDelay, ptr->anchorHubIds };
      cmd.after  = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->input.GetValue(), ptr->lastParam, ptr->lastDelay, ptr->anchorHubIds };
      owner->PushCommand(std::move(cmd));
      ptr->lastInput = ptr->input.GetValue();
    }
    if(owner) owner->layoutDirty = true; this->needsLayout = true; });
  window.AddWidget(&row->input);

  row->param.Create("Param");
  row->param.SetLocalizationEnabled(false);
  row->param.SetShadowRadius(0);
  row->param.SetValue("");
  row->param.OnInputAccepted([this, owner, ptr=row.get()](wi::gui::EventArgs){
    if (owner) {
      NodeEditorWindow::UndoCommand cmd; cmd.type = NodeEditorWindow::UndoType::EditConnection;
      cmd.before = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->lastInput, ptr->lastParam, ptr->lastDelay, ptr->anchorHubIds };
      cmd.after  = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->lastInput, ptr->param.GetValue(), ptr->lastDelay, ptr->anchorHubIds };
      owner->PushCommand(std::move(cmd));
      ptr->lastParam = ptr->param.GetValue();
    }
    if(owner) owner->layoutDirty = true; this->needsLayout = true; });
  window.AddWidget(&row->param);

  row->delay.Create("Delay");
  row->delay.SetLocalizationEnabled(false);
  row->delay.SetShadowRadius(0);
  row->delay.SetValue("0.0");
  row->delay.OnInputAccepted([this, owner, ptr=row.get()](wi::gui::EventArgs){
    if (owner) {
      NodeEditorWindow::UndoCommand cmd; cmd.type = NodeEditorWindow::UndoType::EditConnection;
      cmd.before = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->lastInput, ptr->lastParam, ptr->lastDelay, ptr->anchorHubIds };
      cmd.after  = { this->uid, ptr->uid, ptr->outputName, ptr->lastTarget, ptr->lastInput, ptr->lastParam, ptr->delay.GetValue(), ptr->anchorHubIds };
      owner->PushCommand(std::move(cmd));
      ptr->lastDelay = ptr->delay.GetValue();
    }
    if(owner) owner->layoutDirty = true; this->needsLayout = true; });
  window.AddWidget(&row->delay);

  row->removeButton.Create("x");
  row->removeButton.SetLocalizationEnabled(false);
  row->removeButton.SetShadowRadius(0);
  row->removeButton.SetSize(XMFLOAT2(18, 20));
  row->removeButton.OnClick([this, owner, ptr=row.get()](wi::gui::EventArgs){
    if (owner) {
      NodeEditorWindow::ConnectionSnapshot s;
      s.node_uid = this->uid;
      s.conn_uid = ptr->uid;
      s.outputName = ptr->outputName;
      s.target = GetFieldText(ptr->target);
      s.input = GetFieldText(ptr->input);
      s.param = GetFieldText(ptr->param);
      s.delay = GetFieldText(ptr->delay);
      s.hubIds = ptr->anchorHubIds;
      NodeEditorWindow::UndoCommand cmd;
      cmd.type = NodeEditorWindow::UndoType::RemoveConnection;
      cmd.snap = s;
      owner->PushCommand(std::move(cmd));
    }
    this->RemoveConnectionRow(owner, ptr);
    this->LayoutRows();
  });
  window.AddWidget(&row->removeButton);

  ConnectionUI* ret = row.get();
  ret->uid = forced_uid ? forced_uid : (owner ? owner->nextConnUid++ : 0);
  ret->lastTarget = ret->target.GetValue();
  ret->lastInput = ret->input.GetValue();
  ret->lastParam = ret->param.GetValue();
  ret->lastDelay = ret->delay.GetValue();
  connectionRows.push_back(std::move(row));
  if (owner) owner->layoutDirty = true;
  pinCacheDirty = true;
  if (owner && owner->editor) {
    owner->editor->generalWnd.RefreshTheme();
  }
  if (owner) owner->RegisterConnection(this, ret);
  return ret;
}

void NodeEditorWindow::Node::RemoveConnectionRow(NodeEditorWindow* owner, ConnectionUI* row) {
  if (!row) return;
  // Remove widgets from window
  window.RemoveWidget(&row->outLabel);
  window.RemoveWidget(&row->target);
  window.RemoveWidget(&row->input);
  window.RemoveWidget(&row->param);
  window.RemoveWidget(&row->delay);
  window.RemoveWidget(&row->removeButton);
  row->outLabel.Detach();
  row->target.Detach();
  row->input.Detach();
  row->param.Detach();
  row->delay.Detach();
  row->removeButton.Detach();

  
  // Save hubs to clean up after erasing
  auto hubs_to_check = row->anchorHubIds;
  for (auto it = connectionRows.begin(); it != connectionRows.end(); ++it) {
    if (it->get() == row) { connectionRows.erase(it); break; }
  }
  if (owner) owner->UnregisterConnection(this, row);
  for (auto hid : hubs_to_check) {
    if (auto* h = owner->GetHub(hid)) { if (h->refcount > 0) h->refcount--; }
    owner->DeleteHubIfUnreferenced(hid);
  }
  if (owner) owner->layoutDirty = true;
  pinCacheDirty = true;
}

void NodeEditorWindow::Node::LayoutRows() {
  const float padding = 6.0f;
  const float section_gap = 12.0f;
  const float label_h = 18.0f;
  const float row_h = 22.0f;
  const float start_x = 6.0f;
  // Current content width (inside left/right padding)
  float w = window.GetSize().x - start_x * 2.0f;

  // Heuristic text width estimator to drive auto-width growth
  auto estimate_text_width = [](const std::string& s, float font_size) -> float {
    // Average glyph width ~0.55 x font size
    return std::max(0.0f, (float)s.size() * (std::max(10.0f, font_size) * 0.55f));
  };

  // Determine minimal content width required by longest visible labels.
  // Start from minimum so we can also shrink when content gets smaller.
  float required_content_w = NODE_MIN_CONTENT_W;

  // Inputs: ensure the widest input label fits into its 70% allocation
  for (auto& il : inputLabels) {
    const float fs = (float)il->font.params.size;
    const float tw = estimate_text_width(il->GetText(), fs) + padding * 2.0f;
    // input area width is (w * 0.7f - padding)
    required_content_w = std::max(required_content_w, (tw + padding) / 0.7f);
  }

  // Output header labels: 40% of content width reserved near the pin
  for (auto& orow : outputRows) {
    const float fs = (float)orow->label.font.params.size;
    const float tw = estimate_text_width(orow->label.GetText(), fs) + padding * 2.0f;
    required_content_w = std::max(required_content_w, (tw) / 0.40f);
  }

  // Connection rows: ensure the per-row output name label fits its 24% share
  {
    const float remove_w = 18.0f;
    for (auto& crow : connectionRows) {
      const float fs = (float)crow->outLabel.font.params.size;
      const float tw = estimate_text_width(crow->outLabel.GetText(), fs) + padding * 2.0f;
      // available width for columns (aw) = w - remove_w - padding*5
      // outLabel gets 24% of aw => aw*0.24f >= tw  => w >= remove_w + padding*5 + tw/0.24
      required_content_w = std::max(required_content_w, remove_w + padding * 5.0f + tw / 0.24f);

      // Also consider user-editable fields (target/input/param) to avoid truncation.
      // Use live input only when this node is actively being edited, otherwise stable value.
      auto stable_field_text = [](const wi::gui::TextInputField& f) -> std::string {
        wi::gui::TextInputField& nc = const_cast<wi::gui::TextInputField&>(f);
        return nc.GetValue();
      };
      const float fs_field = (float)crow->input.font.params.size;
      const std::string s_target = activeEditing ? GetFieldText(crow->target) : stable_field_text(crow->target);
      const std::string s_input  = activeEditing ? GetFieldText(crow->input)  : stable_field_text(crow->input);
      const std::string s_param  = activeEditing ? GetFieldText(crow->param)  : stable_field_text(crow->param);
      const float tw_target = estimate_text_width(s_target, fs_field) + padding * 2.0f;
      const float tw_input  = estimate_text_width(s_input,  fs_field) + padding * 2.0f;
      const float tw_param  = estimate_text_width(s_param,  fs_field) + padding * 2.0f;
      // target 28% of aw, input 20%, param 20%
      required_content_w = std::max(required_content_w, remove_w + padding * 5.0f + tw_target / 0.28f);
      required_content_w = std::max(required_content_w, remove_w + padding * 5.0f + tw_input  / 0.20f);
      required_content_w = std::max(required_content_w, remove_w + padding * 5.0f + tw_param  / 0.20f);
    }
  }

  // Clamp measured content width to configured bounds
  required_content_w = std::max(NODE_MIN_CONTENT_W, std::min(required_content_w, NODE_MAX_CONTENT_W));
  measuredContentWidth = required_content_w;

  if (required_content_w > w + 0.5f) {
    const XMFLOAT2 cur = window.GetSize();
    // expand window width (content + paddings)
    window.SetSize(XMFLOAT2(start_x * 2.0f + required_content_w, cur.y));
    w = required_content_w; // use the grown width for this layout pass
  }

  // 1) Inputs section (top area, left side)
  float y_inputs = 36.0f; // more breathing room under title bar
  for (auto& il : inputLabels) {
    il->SetPos(XMFLOAT2(start_x, y_inputs));
    il->SetSize(XMFLOAT2(w * 0.7f - padding, row_h));
    y_inputs += row_h + padding;
  }

  // 2) Outputs section (below inputs)
  float y = y_inputs + section_gap;
  for (auto& orow : outputRows) {
    // Allow longer output names by widening the header label (40% of content)
    const float pinlabel_w = w * 0.40f;
    orow->label.SetPos(XMFLOAT2(start_x + w - 40.0f - padding - pinlabel_w, y));
    orow->label.SetSize(XMFLOAT2(pinlabel_w, label_h));
    orow->addButton.SetPos(XMFLOAT2(start_x + w - 40.0f, y));
    y += label_h + padding;

    for (auto& crow : connectionRows) {
      if (crow->outputName != orow->name) continue;

      float x = start_x;
      const float remove_w = 18.0f;
      const float aw = std::max(0.0f, w - remove_w - padding * 5.0f);
      // Allocate a bit more width to the output label to accommodate longer names
      const float out_w    = aw * 0.24f;
      const float target_w = aw * 0.26f;
      const float input_w  = aw * 0.18f;
      const float param_w  = aw * 0.18f;
      const float delay_w  = std::max(0.0f, aw - (out_w + target_w + input_w + param_w));

      crow->outLabel.SetPos(XMFLOAT2(x, y));
      crow->outLabel.SetSize(XMFLOAT2(out_w, row_h)); x += out_w + padding;
      crow->target.SetPos(XMFLOAT2(x, y));
      crow->target.SetSize(XMFLOAT2(target_w, row_h)); x += target_w + padding;
      crow->input.SetPos(XMFLOAT2(x, y));
      crow->input.SetSize(XMFLOAT2(input_w, row_h)); x += input_w + padding;
      crow->param.SetPos(XMFLOAT2(x, y));
      crow->param.SetSize(XMFLOAT2(param_w, row_h)); x += param_w + padding;
      crow->delay.SetPos(XMFLOAT2(x, y));
      crow->delay.SetSize(XMFLOAT2(delay_w, row_h)); x += delay_w + padding;
      crow->removeButton.SetPos(XMFLOAT2(x, y));
      crow->removeButton.SetSize(XMFLOAT2(remove_w, row_h));

      y += row_h + padding;
    }
  }

  // 3) Bottom spacer to add some padding
  if (bottomSpacer.parent == nullptr) {
    bottomSpacer.Create("spacer");
    bottomSpacer.SetLocalizationEnabled(false);
    bottomSpacer.SetShadowRadius(0);
    bottomSpacer.SetText("");
    window.AddWidget(&bottomSpacer);
  }
  bottomSpacer.SetPos(XMFLOAT2(start_x, y));
  bottomSpacer.SetSize(XMFLOAT2(1, section_gap));
  pinCacheDirty = true;
}

void NodeEditorWindow::Node::ComputePinCache() {
  // Inputs: left of input labels
  cachedInputPins.clear();
  cachedInputPins.reserve(inputLabels.size());
  for (auto& il : inputLabels) {
    float cx = il->translation.x - 6.0f;
    float cy = il->translation.y + il->scale.y * 0.5f;
    cachedInputPins.push_back(XMFLOAT2(cx, cy));
  }
  // Outputs: right of max(label.right, addButton.right)
  cachedOutputPins.clear();
  cachedOutputPins.reserve(outputRows.size());
  for (auto& orow : outputRows) {
    float headerRight = std::max(orow->label.translation.x + orow->label.scale.x,
                                 orow->addButton.translation.x + orow->addButton.scale.x);
    float cx = headerRight + 6.0f;
    float cy = orow->label.translation.y + orow->label.scale.y * 0.5f;
    cachedOutputPins.push_back({ orow.get(), XMFLOAT2(cx, cy) });
  }
}

void NodeEditorWindow::RemoveNode(Node* node) {
  if (!node) return;

  // Remove GUI widget linkage first
  RemoveWidget(&node->window);
  node->window.Detach();
  node->window.RemoveWidgets();

  // remove from index
  auto itidx = nodeIndex.find(node->name);
  if (itidx != nodeIndex.end()) {
    auto& vec = itidx->second;
    for (auto vit = vec.begin(); vit != vec.end(); ) {
      if (*vit == node) vit = vec.erase(vit); else ++vit;
    }
    if (vec.empty()) nodeIndex.erase(itidx);
  }
  if (node->entity != wi::ecs::INVALID_ENTITY) {
    auto itent = entityIndex.find(node->entity);
    if (itent != entityIndex.end() && itent->second == node) {
      entityIndex.erase(itent);
    }
  }

  // unregister from uid maps
  UnregisterNode(node);

  // Erase the node from our list (unique_ptr will clean up)
  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    if (it->get() == node) {
      nodes.erase(it);
      break;
    }
  }

  // No global relayout on removal to preserve existing positions
  // Cleanup hubs that may have become unreferenced
  for (int i = (int)hubs.size() - 1; i >= 0; --i) {
    DeleteHubIfUnreferenced(hubs[i].id);
  }
}

void NodeEditorWindow::RenameNode(Node* node, const std::string& newname) {
  if (!node) return;
  // erase old mapping if it points to this node
  auto it = nodeIndex.find(node->name);
  if (it != nodeIndex.end()) {
    auto& vec = it->second;
    for (auto vit = vec.begin(); vit != vec.end(); ) {
      if (*vit == node) vit = vec.erase(vit); else ++vit;
    }
    if (vec.empty()) nodeIndex.erase(it);
  }
  node->name = newname;
  node->label.SetText(newname);
  node->window.SetText(newname);
  node->window.SetName(newname);
  node->window.SetTitle(newname);
  nodeIndex[newname].push_back(node);
}



// Shared hub helpers:
NodeEditorWindow::RerouteHub* NodeEditorWindow::GetHub(uint32_t id) {
  auto it = hubs.find(id);
  return it == hubs.end() ? nullptr : &it->second;
}
const NodeEditorWindow::RerouteHub* NodeEditorWindow::GetHub(uint32_t id) const {
  auto it = hubs.find(id);
  return it == hubs.end() ? nullptr : &it->second;
}
uint32_t NodeEditorWindow::CreateHub(const XMFLOAT2& local) {
  RerouteHub h; h.id = nextHubId++; h.pos = local; h.refcount = 0;
  hubs.emplace(h.id, h);
  return h.id;
}
void NodeEditorWindow::DeleteHubIfUnreferenced(uint32_t id) {
  auto it = hubs.find(id);
  if (it != hubs.end() && it->second.refcount == 0) {
    hubs.erase(it);
  }
}

// ---- Undo/Redo helpers ----
void NodeEditorWindow::RegisterNode(Node* n){ if(!n) return; nodesByUid[n->uid]=n; }
void NodeEditorWindow::UnregisterNode(Node* n){ if(!n) return; nodesByUid.erase(n->uid); }
void NodeEditorWindow::RegisterConnection(Node* owner, Node::ConnectionUI* row){ if(!row) return; connsByUid[row->uid]=row; }
void NodeEditorWindow::UnregisterConnection(Node* owner, Node::ConnectionUI* row){ if(!row) return; connsByUid.erase(row->uid); }
NodeEditorWindow::Node* NodeEditorWindow::FindNode(uint64_t uid) const { auto it=nodesByUid.find(uid); return it==nodesByUid.end()?nullptr:it->second; }
NodeEditorWindow::Node::ConnectionUI* NodeEditorWindow::FindConnection(uint64_t uid) const { auto it=connsByUid.find(uid); return it==connsByUid.end()?nullptr:it->second; }

NodeEditorWindow::ConnectionSnapshot NodeEditorWindow::MakeSnapshot(const Node* owner, const Node::ConnectionUI* row) const {
  ConnectionSnapshot s;
  if (!row) return s;
  // Resolve owner if not provided
  const Node* own = owner;
  if (!own) {
    for (const auto& n : nodes) {
      for (const auto& cr : n->connectionRows) {
        if (cr.get() == row) { own = n.get(); break; }
      }
      if (own) break;
    }
  }
  if (!own) return s;
  s.node_uid = own->uid;
  s.conn_uid = row->uid;
  s.outputName = row->outputName;
  // Use helper that reads current input safely from const field
  s.target = GetFieldText(row->target);
  s.input = GetFieldText(row->input);
  s.param = GetFieldText(row->param);
  s.delay = GetFieldText(row->delay);
  s.hubIds = row->anchorHubIds;
  return s;
}

void NodeEditorWindow::ApplySnapshotAdd(const ConnectionSnapshot& s){
  Node* n = FindNode(s.node_uid); if(!n) return; auto* row = n->AddConnectionRow(this, s.outputName, s.conn_uid); if(!row) return; row->target.SetValue(s.target); row->input.SetValue(s.input); row->param.SetValue(s.param); row->delay.SetValue(s.delay); row->anchorHubIds = s.hubIds; for(auto hid: s.hubIds){ if (auto* h = GetHub(hid)) { h->refcount++; } }
  n->LayoutRows();
  RegisterConnection(n,row);
}
void NodeEditorWindow::ApplySnapshotRemove(const ConnectionSnapshot& s){ Node* n=FindNode(s.node_uid); if(!n) return; Node::ConnectionUI* row = FindConnection(s.conn_uid); if(!row){ for(auto& cr: n->connectionRows){ if(cr->outputName==s.outputName && GetFieldText(cr->target)==s.target && GetFieldText(cr->input)==s.input && GetFieldText(cr->param)==s.param && GetFieldText(cr->delay)==s.delay){ row=cr.get(); break; } } }
  if(row){ n->RemoveConnectionRow(this,row); }
}

void NodeEditorWindow::PushCommand(UndoCommand&& cmd){ undoStack.push_back(std::move(cmd)); redoStack.clear(); if(undoStack.size()>UNDO_LIMIT){ undoStack.erase(undoStack.begin()); } }

void NodeEditorWindow::Undo(){ if(undoStack.empty()) return; UndoCommand cmd = std::move(undoStack.back()); undoStack.pop_back();
  std::function<void(UndoCommand&)> applyUndo = [&](UndoCommand& c){
    switch(c.type){
      case UndoType::AddConnection: ApplySnapshotRemove(c.snap); break;
      case UndoType::RemoveConnection: ApplySnapshotAdd(c.snap); break;
      case UndoType::EditConnection: {
        Node::ConnectionUI* r = FindConnection(c.after.conn_uid);
        Node* n = FindNode(c.after.node_uid);
        if (r && n) {
          r->target.SetValue(c.before.target);
          r->input.SetValue(c.before.input);
          r->param.SetValue(c.before.param);
          r->delay.SetValue(c.before.delay);
          r->lastTarget = c.before.target;
          r->lastInput  = c.before.input;
          r->lastParam  = c.before.param;
          r->lastDelay  = c.before.delay;
          n->LayoutRows();
        }
      } break;
      case UndoType::SetConnectionHubs: {
        Node::ConnectionUI* r = FindConnection(c.after.conn_uid);
        if (r) {
          // Recreate any hubs needed for the "before" state
          for (const auto& hc : c.hubs_undo_create) {
            if (!GetHub(hc.id)) {
              RerouteHub h; h.id = hc.id; h.pos = hc.pos; h.refcount = 0; hubs[h.id] = h; if (nextHubId <= h.id) nextHubId = h.id + 1;
            }
          }
          for (auto hid: r->anchorHubIds) { if (auto* h=GetHub(hid)) { if (h->refcount>0) h->refcount--; } DeleteHubIfUnreferenced(hid); }
          r->anchorHubIds = c.before.hubIds;
          for (auto hid: r->anchorHubIds) { if (auto* h=GetHub(hid)) h->refcount++; }
        }
      } break;
      case UndoType::MoveHub: { if(auto* h = GetHub(c.movehub.hubId)) h->pos = c.movehub.from; } break;
      case UndoType::Macro: for(int i=(int)c.macro.size()-1;i>=0;--i) applyUndo(c.macro[i]); break; default: break; }
  };
  applyUndo(cmd);
  redoStack.push_back(std::move(cmd));
}

void NodeEditorWindow::Redo(){ if(redoStack.empty()) return; UndoCommand cmd = std::move(redoStack.back()); redoStack.pop_back();
  std::function<void(UndoCommand&)> applyRedo = [&](UndoCommand& c){
    switch(c.type){
      case UndoType::AddConnection: ApplySnapshotAdd(c.snap); break;
      case UndoType::RemoveConnection: ApplySnapshotRemove(c.snap); break;
      case UndoType::EditConnection: {
        Node::ConnectionUI* r = FindConnection(c.before.conn_uid);
        Node* n = FindNode(c.before.node_uid);
        if (r && n) {
          r->target.SetValue(c.after.target);
          r->input.SetValue(c.after.input);
          r->param.SetValue(c.after.param);
          r->delay.SetValue(c.after.delay);
          r->lastTarget = c.after.target;
          r->lastInput  = c.after.input;
          r->lastParam  = c.after.param;
          r->lastDelay  = c.after.delay;
          n->LayoutRows();
        }
      } break;
      case UndoType::SetConnectionHubs: {
        Node::ConnectionUI* r = FindConnection(c.before.conn_uid);
        if (r) {
          // Recreate any hubs needed for the "after" state
          for (const auto& hc : c.hubs_redo_create) {
            if (!GetHub(hc.id)) {
              RerouteHub h; h.id = hc.id; h.pos = hc.pos; h.refcount = 0; hubs[h.id] = h; if (nextHubId <= h.id) nextHubId = h.id + 1;
            }
          }
          for (auto hid: r->anchorHubIds) { if (auto* h=GetHub(hid)) { if (h->refcount>0) h->refcount--; } DeleteHubIfUnreferenced(hid); }
          r->anchorHubIds = c.after.hubIds;
          for (auto hid: r->anchorHubIds) { if (auto* h=GetHub(hid)) h->refcount++; }
        }
      } break;
      case UndoType::MoveHub: { if(auto* h = GetHub(c.movehub.hubId)) h->pos = c.movehub.to; } break;
      case UndoType::Macro: for(auto& sc : c.macro) applyRedo(sc); break; default: break; }
  };
  applyRedo(cmd);
  undoStack.push_back(std::move(cmd));
}
void NodeEditorWindow::OnEntityRenamed(wi::ecs::Entity entity, const std::string& newname) {
  auto it = entityIndex.find(entity);
  if (it != entityIndex.end()) {
    Node* nd = it->second;
    if (nd && nd->name != newname) {
      // capture old name and multiplicity before renaming to decide on name-only updates safely
      std::string oldname = nd->name;
      size_t oldCount = 0;
      if (auto itold = nodeIndex.find(oldname); itold != nodeIndex.end()) {
        oldCount = itold->second.size(); // includes this node
      }

      RenameNode(nd, newname);
      nd->pinCacheDirty = true;
      layoutDirty = true;

      // Update any connection rows that strongly bind to this entity
      for (auto& n : nodes) {
        for (auto& cr : n->connectionRows) {
          if (cr->targetEntity == entity) {
            cr->target.SetValue(newname);
          }
        }
      }

      // Optional safe fix-up for name-only targets:
      // If this node was the ONLY node with oldname (oldCount == 1), then update all name-only
      // connection rows (targetEntity == INVALID_ENTITY) that target oldname to the newname.
      if (oldCount == 1) {
        for (auto& n : nodes) {
          for (auto& cr : n->connectionRows) {
            if (cr->targetEntity == wi::ecs::INVALID_ENTITY && GetFieldText(cr->target) == oldname) {
              cr->target.SetValue(newname);
            }
          }
        }
      }
    }
  }
}

// =====================
// Save / Load (JSON)
// =====================

bool NodeEditorWindow::SaveGraph(const std::string& path) const
{
  using nlohmann::json;
  json j;
  j["version"] = 1;

  // Hubs
  json jhubs = json::array();
  for (const auto& kv : hubs) {
    const auto& h = kv.second;
    json hj;
    hj["id"] = h.id;
    hj["pos"] = { {"x", h.pos.x}, {"y", h.pos.y} };
    jhubs.push_back(hj);
  }
  j["hubs"] = jhubs;

  // Nodes
  json jnodes = json::array();
  for (const auto& np : nodes) {
    const Node* n = np.get();
    json nj;
    nj["uid"] = n->uid;
    nj["type"] = (n->type == Node::NodeType::EntityBound ? "EntityBound" : "LogicOnly");
    nj["name"] = n->name;
    // NOTE: Do not serialize engine entity linkage here.
    // Future work: InputOutputComponent will hold association from Entity -> Node uid.
    nj["pos"] = { {"x", n->window.translation.x}, {"y", n->window.translation.y} };
    nj["size"] = { {"w", n->window.scale.x}, {"h", n->window.scale.y} };

    // I/O lists
    // NOTE: For EntityBound nodes, outputs should be derived from the bound entity.
    // Saving them for now for compatibility; consider ignoring on load in the future.
    json jins = json::array(); for (auto& s : n->inputs) jins.push_back(s);
    json jouts = json::array(); for (auto& s : n->outputs) jouts.push_back(s);
    nj["inputs"] = jins;
    nj["outputs"] = jouts;

    // Connections
    json jconns = json::array();
    for (const auto& crp : n->connectionRows) {
      const Node::ConnectionUI* cr = crp.get();
      json cj;
      cj["uid"] = cr->uid;
      cj["outputName"] = cr->outputName;
      cj["target"] = GetFieldText(cr->target);
      cj["targetEntity"] = (uint64_t)cr->targetEntity;
      cj["input"] = GetFieldText(cr->input);
      cj["param"] = GetFieldText(cr->param);
      cj["delay"] = GetFieldText(cr->delay);
      json jh = json::array(); for (auto hid : cr->anchorHubIds) jh.push_back(hid); cj["hubs"] = jh;
      jconns.push_back(cj);
    }
    nj["connections"] = jconns;

    jnodes.push_back(nj);
  }
  j["nodes"] = jnodes;

  // Next ids
  j["nextIds"] = { {"node", nextNodeUid}, {"conn", nextConnUid}, {"hub", nextHubId} };

  std::ofstream ofs(path, std::ios::binary);
  if (!ofs.good()) return false;
  ofs << j.dump(2);
  return ofs.good();
}

bool NodeEditorWindow::LoadGraph(const std::string& path)
{
  using nlohmann::json;
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.good()) return false;
  json j; ifs >> j; if (!ifs.good() && !ifs.eof()) return false;

  // Clear current graph
  selectedConnection = nullptr; hoveredConnection = nullptr;
  // Remove nodes safely
  wi::vector<Node*> toremove; for (auto& n : nodes) toremove.push_back(n.get());
  for (auto* n : toremove) RemoveNode(n);
  nodes.clear(); nodeIndex.clear(); entityIndex.clear(); nodesByUid.clear(); connsByUid.clear();
  hubs.clear(); nextHubId = 1;
  undoStack.clear(); redoStack.clear();

  int version = j.value("version", 1);
  (void)version;

  // Hubs
  if (j.contains("hubs")) {
    for (auto& hj : j["hubs"]) {
      RerouteHub h; h.id = hj.value("id", 0u);
      auto pj = hj["pos"]; h.pos.x = pj.value("x", 0.0f); h.pos.y = pj.value("y", 0.0f);
      h.refcount = 0; hubs[h.id] = h; if (nextHubId <= h.id) nextHubId = h.id + 1;
    }
  }

  auto mk_vec_str = [](const json& arr){ wi::vector<std::string> out; if(arr.is_array()) for(auto& x: arr) out.push_back(x.get<std::string>()); return out; };

  // Nodes
  if (j.contains("nodes")) {
    for (auto& nj : j["nodes"]) {
      auto node = std::make_unique<Node>(nj.value("name", std::string("Node")));
      node->uid = nj.value("uid", nextNodeUid++);
      std::string type = nj.value("type", std::string("LogicOnly"));
      node->type = (type == "EntityBound" ? Node::NodeType::EntityBound : Node::NodeType::LogicOnly);
      // NOTE: We intentionally do not restore engine entity linkage here.
      // That mapping will be provided by a future InputOutputComponent that
      // associates an Entity with a Node uid. For now, keep it invalid:
      node->entity = wi::ecs::INVALID_ENTITY;

      // Build UI similar to AddNode()/AddNodeForEntity() but from saved IO lists
      node->window.Create(node->name,
        Window::WindowControls::MOVE | Window::WindowControls::CLOSE | Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL);
      node->window.SetSize(XMFLOAT2(NODE_MIN_CONTENT_W, 110));
      node->label.Create(node->name); node->label.SetText(node->name); node->label.SetPos(XMFLOAT2(4,4)); node->label.SetSize(XMFLOAT2(112,20));
      node->window.AddWidget(&node->label);

      node->inputs = mk_vec_str(nj["inputs"]);
      node->outputs = mk_vec_str(nj["outputs"]);
      // TODO: For EntityBound nodes, outputs should be derived from the bound entity instead of loading.

      for (auto& outname : node->outputs) node->AddOutputRow(this, outname);
      for (auto& inname : node->inputs) {
        auto lbl = std::make_unique<wi::gui::Label>(); lbl->Create(inname); lbl->SetLocalizationEnabled(false); lbl->SetShadowRadius(0); lbl->SetText(inname);
        node->window.AddWidget(lbl.get()); node->inputLabels.push_back(std::move(lbl)); node->inputIndex[inname] = node->inputLabels.size()-1;
      }
      node->LayoutRows();
      AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
      node->window.SetEnabled(true); node->window.SetVisible(true);

      Node* raw = node.get();
      node->window.OnClose([this, raw](wi::gui::EventArgs){ pendingRemoval.push_back(raw); });
      nodes.push_back(std::move(node));
      RegisterNode(nodes.back().get());
      nodeIndex[nodes.back()->name].push_back(nodes.back().get());

      // Position/size
      auto pj = nj["pos"]; float px = pj.value("x", 0.0f), py = pj.value("y", 0.0f);
      auto sj = nj["size"]; float sw = sj.value("w", nodes.back()->window.scale.x), sh = sj.value("h", nodes.back()->window.scale.y);
      nodes.back()->window.SetPos(XMFLOAT2(px, py)); nodes.back()->window.SetSize(XMFLOAT2(sw, sh));

      // Connections
      if (nj.contains("connections")) {
        for (auto& cj : nj["connections"]) {
          std::string outname = cj.value("outputName", std::string()); if (outname.empty()) continue;
          uint64_t cuid = cj.value("uid", (uint64_t)0);
          auto* cr = nodes.back()->AddConnectionRow(this, outname, cuid);
          if (!cr) continue; RegisterConnection(nodes.back().get(), cr);
          cr->target.SetValue(cj.value("target", std::string("!self")));
          cr->targetEntity = (wi::ecs::Entity)cj.value("targetEntity", (uint64_t)wi::ecs::INVALID_ENTITY);
          cr->input.SetValue(cj.value("input", std::string("FunctionName")));
          cr->param.SetValue(cj.value("param", std::string("")));
          cr->delay.SetValue(cj.value("delay", std::string("0.0")));
          // last-committed mirrors
          cr->lastTarget = cr->target.GetValue(); cr->lastInput = cr->input.GetValue(); cr->lastParam = cr->param.GetValue(); cr->lastDelay = cr->delay.GetValue();
          // hubs
          cr->anchorHubIds.clear(); if (cj.contains("hubs")) { for (auto& hx : cj["hubs"]) { uint32_t hid = hx.get<uint32_t>(); cr->anchorHubIds.push_back(hid); if (auto* h=GetHub(hid)) h->refcount++; } }
        }
      }
      nodes.back()->LayoutRows();
    }
  }

  // Next ids
  if (j.contains("nextIds")) {
    nextNodeUid = j["nextIds"].value("node", nextNodeUid);
    nextConnUid = j["nextIds"].value("conn", nextConnUid);
    nextHubId   = j["nextIds"].value("hub",  nextHubId);
  } else {
    // recompute
    uint64_t maxn=0, maxc=0; uint32_t maxh=0;
    for (auto& n : nodes) { if (n->uid>maxn) maxn=n->uid; for (auto& cr : n->connectionRows) if (cr->uid>maxc) maxc=cr->uid; }
    for (auto& kv : hubs) if (kv.first>maxh) maxh=kv.first;
    nextNodeUid = maxn + 1; nextConnUid = maxc + 1; nextHubId = maxh + 1;
  }

  // Finalize
  layoutDirty = true; for (auto& n : nodes) n->pinCacheDirty = true;
  return true;
}
void NodeEditorWindow::AddTimerNode() {
  std::string name = "Timer";
  auto node = std::make_unique<Node>(name);
  node->type = Node::NodeType::LogicOnly;
  node->window.Create(
      name,
      Window::WindowControls::MOVE |
      Window::WindowControls::CLOSE |
      Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL /* |
	  Window::WindowControls::RESIZE_LEFT |
	  Window::WindowControls::RESIZE_RIGHT*/);
  node->window.SetSize(XMFLOAT2(NODE_MIN_CONTENT_W, 120));

  node->label.Create(name);
  node->label.SetText(name);
  node->label.SetPos(XMFLOAT2(4, 4));
  node->label.SetSize(XMFLOAT2(112, 20));
  node->window.AddWidget(&node->label);

  // Logic-only: Inputs/Outputs
  node->outputs = { "OnTick" };
  node->inputs = { "StartTimer", "StopTimer" };
  for (auto& outname : node->outputs) node->AddOutputRow(this, outname);
  for (auto& inname : node->inputs) {
    auto lbl = std::make_unique<wi::gui::Label>();
    lbl->Create(inname);
    lbl->SetLocalizationEnabled(false);
    lbl->SetShadowRadius(0);
    lbl->SetText(inname);
    node->window.AddWidget(lbl.get());
    node->inputLabels.push_back(std::move(lbl));
    node->inputIndex[inname] = node->inputLabels.size() - 1;
  }
  node->LayoutRows();

  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
  node->window.SetEnabled(true);
  node->window.SetVisible(true);
  if (editor) {
    editor->generalWnd.RefreshTheme();
  }

  Node* raw = node.get();
  node->window.OnClose([this, raw](wi::gui::EventArgs) { pendingRemoval.push_back(raw); });

  nodes.push_back(std::move(node));
  nodeIndex[raw->name].push_back(raw);
  raw->pinCacheDirty = true;
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
}

void NodeEditorWindow::AddSequenceNode() {
  std::string name = "Sequence";
  auto node = std::make_unique<Node>(name);
  node->type = Node::NodeType::LogicOnly;
  node->window.Create(
      name,
      Window::WindowControls::MOVE |
      Window::WindowControls::CLOSE |
      Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL /* |
	  Window::WindowControls::RESIZE_LEFT |
	  Window::WindowControls::RESIZE_RIGHT*/);
  node->window.SetSize(XMFLOAT2(NODE_MIN_CONTENT_W, 120));

  node->label.Create(name);
  node->label.SetText(name);
  node->label.SetPos(XMFLOAT2(4, 4));
  node->label.SetSize(XMFLOAT2(112, 20));
  node->window.AddWidget(&node->label);

  node->inputs = { "In" };
  node->outputs = { "Then1", "Then2" };
  for (auto& outname : node->outputs) node->AddOutputRow(this, outname);
  for (auto& inname : node->inputs) {
    auto lbl = std::make_unique<wi::gui::Label>();
    lbl->Create(inname);
    lbl->SetLocalizationEnabled(false);
    lbl->SetShadowRadius(0);
    lbl->SetText(inname);
    node->window.AddWidget(lbl.get());
    node->inputLabels.push_back(std::move(lbl));
  }
  node->LayoutRows();

  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
  node->window.SetEnabled(true);
  node->window.SetVisible(true);
  if (editor) {
    editor->generalWnd.RefreshTheme();
  }

  Node* raw = node.get();
  node->window.OnClose([this, raw](wi::gui::EventArgs) { pendingRemoval.push_back(raw); });

  nodes.push_back(std::move(node));
  nodeIndex[raw->name].push_back(raw);
  raw->pinCacheDirty = true;
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
}
