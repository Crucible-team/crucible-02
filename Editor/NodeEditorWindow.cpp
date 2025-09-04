// clang-format off
#include "stdafx.h"
#include "NodeEditorWindow.h"
#include "wiImage.h"
#include "wiScene.h"
#include <unordered_set>
#include <unordered_map>
// clang-format on

using namespace wi::graphics;
using namespace wi::gui;

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

void NodeEditorWindow::Create(EditorComponent *_editor) {
  editor = _editor;
  control_size = 30;

  wi::gui::Window::Create("Node Editor");

 // RemoveWidget(&scrollbar_horizontal);
 // RemoveWidget(&scrollbar_vertical);
 // scrollable_area.Detach();
 // scrollable_area.SetEnabled(false);
 // scrollable_area.SetVisible(false);

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

  // (Zoom slider removed)

  SetVisible(false);
}

void NodeEditorWindow::BuildNodesFromSceneMetadata() {
  if (!editor) return;
  wi::scene::Scene& scene = editor->GetCurrentScene();
  const auto& entities = scene.metadatas.GetEntityArray();
  for (wi::ecs::Entity e : entities) {
    // Don't recreate if already added
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
      const auto& wnd = n->window;
      const float pinR = 4.0f;
      // Inputs on left (use cached positions to avoid transient jitter)
      for (size_t i = 0; i < n->inputLabels.size(); ++i) {
        auto& il = n->inputLabels[i];
        XMFLOAT2 pinpos = (i < n->cachedInputPins.size()) ? n->cachedInputPins[i] : XMFLOAT2(il->translation.x - 6.0f, il->translation.y + il->scale.y * 0.5f);
        float cx = pinpos.x; // just left to the label
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
          // subtle outer ring to indicate snap target
          wi::image::Params ring = pin;
          ring.siz = XMFLOAT2((r + 4.0f) * 2, (r + 4.0f) * 2);
          ring.color = XMFLOAT4(0.4f, 1.0f, 0.6f, 0.25f);
          wi::image::Draw(nullptr, ring, cmd);
        }
      }
      // Outputs on right: one pin per output header row, with self-target highlight (anchor to header widgets)
      for (auto& orow : n->outputRows) {
        // Check if any connection for this output targets self
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

        // Use cached output pin position when available
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

    // Draw connection wires based on connection rows (deduplicated by key)
    for (const auto& n : nodes) {
      const auto& wnd = n->window;
      std::unordered_set<std::string> seen_keys;
      for (const auto& crow_uptr : n->connectionRows) {
        const auto* crow = crow_uptr.get();
        // find source output header row by name
        const Node::OutputUI* src_orow = nullptr;
        for (const auto& orow_uptr : n->outputRows) {
          if (orow_uptr->name == crow->outputName) { src_orow = orow_uptr.get(); break; }
        }
        if (!src_orow) continue;

        // Source pin position: use cached output pin position when possible
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

        // Determine target nodes by target field (can be multiple)
        std::string target_text = GetFieldText(crow->target);
        if (target_text == "!self" || target_text.empty()) {
          // self target: don't draw a wire, it's indicated by row highlight
          continue;
        }

        // Deduplication will be performed per-target node below (key includes target pointer)

        // Determine target nodes by name via index (supports duplicates)
        wi::vector<const Node*> targets;
        if (auto itnode = nodeIndex.find(target_text); itnode != nodeIndex.end()) {
          for (auto* cand : itnode->second) if (cand != n.get()) targets.push_back(cand);
        }
        if (targets.empty()) continue;

        const std::string input_name = GetFieldText(crow->input);
        for (const Node* target_node : targets) {
          const wi::gui::Label* target_input_label = nullptr;
          if (!input_name.empty())
          {
            for (const auto& ilbl_uptr : target_node->inputLabels) {
              if (ilbl_uptr->GetText() == input_name) { target_input_label = ilbl_uptr.get(); break; }
            }
          }
          float ex;
          float ey;
          if (target_input_label) {
            // use cached input pin position for target node when available
            bool foundTargetCache = false;
            for (size_t i = 0; i < target_node->inputLabels.size(); ++i) {
              if (target_node->inputLabels[i].get() == target_input_label) {
                if (i < target_node->cachedInputPins.size()) { ex = target_node->cachedInputPins[i].x; ey = target_node->cachedInputPins[i].y; foundTargetCache = true; }
                break;
              }
            }
            if (!foundTargetCache) {
              ex = target_node->window.translation.x + 6.0f;
              ey = target_input_label->translation.y + target_input_label->scale.y * 0.5f;
            }
          } else {
            // Fallback to center-left of target window
            ex = target_node->window.translation.x + 6.0f;
            ey = target_node->window.translation.y + target_node->window.scale.y * 0.5f;
          }

          // Draw piecewise segments through anchors (if any)
          XMFLOAT4 col = (selectedConnection == crow) ? XMFLOAT4(1.0f, 0.9f, 0.6f, 1) : XMFLOAT4(0.6f, 1.0f, 0.6f, 1);
          XMFLOAT2 astart(sx, sy);
          if (!crow->anchorHubIds.empty()) {
            for (size_t i = 0; i < crow->anchorHubIds.size(); ++i) {
              const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
              XMFLOAT2 ap(XMFLOAT2(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y));
              wi::gui::DrawWireBezierStrip(astart, ap, 2.0f, col, canvas, cmd);
              astart = ap;
            }
          }
          wi::gui::DrawWireBezierStrip(astart, XMFLOAT2(ex, ey), 2.0f, col, canvas, cmd);

          // Draw anchors if selected
          if (selectedConnection == crow) {
            for (size_t i = 0; i < crow->anchorHubIds.size(); ++i) {
              const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
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
    }

    // Draw preview wire while dragging (compute live source position to avoid 1-frame mismatch)
    if (drag.active && drag.srcNode && drag.srcOutput) {
      float sx = 0.0f;
      float sy = 0.0f;
      if (drag.fromAnchor) {
        // If dragging from an anchor, use the hub's current position
        if (anchorRight.active && anchorRight.conn && anchorRight.index >= 0 && anchorRight.index < (int)anchorRight.conn->anchorHubIds.size()) {
          const auto* hub = GetHub(anchorRight.conn->anchorHubIds[anchorRight.index]);
          if (hub) {
            sx = scrollable_area.translation.x + hub->pos.x;
            sy = scrollable_area.translation.y + hub->pos.y;
          }
        }
      }
      if (sx == 0.0f && sy == 0.0f) {
        // Fallback to current output pin position
        float headerRight = std::max(drag.srcOutput->label.translation.x + drag.srcOutput->label.scale.x,
                                     drag.srcOutput->addButton.translation.x + drag.srcOutput->addButton.scale.x);
        sx = headerRight + 6.0f;
        sy = drag.srcOutput->label.translation.y + drag.srcOutput->label.scale.y * 0.5f;
      }

      float ex = drag.cursor.x;
      float ey = drag.cursor.y;
      // snap preview end to hovered input pin if any
      if (drag.hoverNode && drag.hoverInput) {
        ex = drag.hoverNode->window.translation.x + 6.0f;
        ey = drag.hoverInput->translation.y + drag.hoverInput->scale.y * 0.5f;
      }
      wi::gui::DrawWireBezierStrip(
        XMFLOAT2(sx, sy), XMFLOAT2(ex, ey),
        2.0f,
        XMFLOAT4(0.8f, 0.9f, 1.0f, 0.9f),
        canvas, cmd);
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

  // Drag & drop from output pin to input pin:
  {
    XMFLOAT4 pointer = wi::input::GetPointer();
    XMFLOAT2 p = XMFLOAT2(pointer.x, pointer.y);
    const float pinDetectR = 8.0f;

    // Begin drag if pressed on an output pin
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
      // find closest input pin under cursor
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
        const std::string newParam = GetFieldText(drag.movingConnection ? drag.movingConnection->param : *(new wi::gui::TextInputField()));
        const std::string newDelay = GetFieldText(drag.movingConnection ? drag.movingConnection->delay : *(new wi::gui::TextInputField()));
        bool exists = false;
        for (auto& cr : drag.srcNode->connectionRows) {
          if (cr->outputName != drag.srcOutput->name) continue;
          if (GetFieldText(cr->target) != newTarget) continue;
          if (GetFieldText(cr->input) != newInput) continue;
          if (GetFieldText(cr->param) != newParam) continue;
          if (GetFieldText(cr->delay) != newDelay) continue;
          exists = true; selectedConnection = cr.get(); break;
        }
        if (drag.movingConnection) {
          // update existing connection instead of creating a new one
          if (!exists) {
            drag.movingConnection->target.SetValue(newTarget);
            drag.movingConnection->input.SetValue(newInput);
            // keep original param/delay
            drag.movingOwner->LayoutRows();
            selectedConnection = drag.movingConnection;
          }
          layoutDirty = true;
        } else if (!exists) {
          auto* crow = drag.srcNode->AddConnectionRow(this, drag.srcOutput->name);
          if (crow) {
            crow->target.SetValue(newTarget);
            crow->input.SetValue(newInput);
            crow->param.SetValue("");
            crow->delay.SetValue("0.0");
            selectedConnection = crow;
            drag.srcNode->LayoutRows();
          }
          layoutDirty = true;
        }
      }
      else if (drag.movingConnection && drag.movingOwner) {
        // dropped to nowhere: delete the moving connection
        drag.movingOwner->RemoveConnectionRow(this, drag.movingConnection);
        selectedConnection = nullptr;
        layoutDirty = true;
      }
      drag = DragState{}; // reset
    }
  }

  // Wire selection and reroute anchors:
  {
    if (!drag.active) {
      XMFLOAT4 pointer = wi::input::GetPointer();
      XMFLOAT2 p = XMFLOAT2(pointer.x, pointer.y);

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
          // Left release: attempt merge with other anchors of same node+output
          if (anchorDrag.conn) {
            const std::string outname = anchorDrag.conn->outputName;
            auto* myhub = GetHub(anchorDrag.conn->anchorHubIds[anchorDrag.index]);
            XMFLOAT2 my = myhub ? myhub->pos : XMFLOAT2(0,0);
            Node* owner = nullptr;
            for (auto& n : nodes) {
              for (auto& cr : n->connectionRows) if (cr.get() == anchorDrag.conn) { owner = n.get(); break; }
              if (owner) break;
            }
            if (owner) {
              const float mergeThr2 = 10.0f * 10.0f;
              bool merged = false;
              for (auto& cr2_uptr : owner->connectionRows) {
                auto* cr2 = cr2_uptr.get();
                if (cr2 == anchorDrag.conn) continue;
                if (cr2->outputName != outname) continue;
                for (size_t j = 0; j < cr2->anchorHubIds.size(); ++j) {
                  auto* otherhub = GetHub(cr2->anchorHubIds[j]); if (!otherhub) continue;
                  XMFLOAT2 other = otherhub->pos;
                  float dx = my.x - other.x; float dy = my.y - other.y;
                  if (dx * dx + dy * dy <= mergeThr2) {
                    // rebind to shared hub id
                    uint32_t old = anchorDrag.conn->anchorHubIds[anchorDrag.index];
                    anchorDrag.conn->anchorHubIds[anchorDrag.index] = cr2->anchorHubIds[j];
                    DeleteHubIfUnreferenced(old);
                    merged = true; break;
                  }
                }
                if (merged) break;
              }
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
            anchorRight.conn->anchorHubIds.erase(anchorRight.conn->anchorHubIds.begin() + anchorRight.index);
            DeleteHubIfUnreferenced(hid);
          }
          anchorRight = {};
        }
      }

      // Wire selection or anchor add
      if (!anchorDrag.active && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
        // Hit-test wires by sampling piecewise segments: source->anchors->target
        Node::ConnectionUI* hit = nullptr;
        const float threshold = 6.0f; // px
        const float thr2 = threshold * threshold;
        for (auto& n : nodes) {
          const auto& wnd = n->window;
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

            // Source pos
            const Node::OutputUI* src_orow = nullptr;
            for (const auto& orow_uptr : n->outputRows) if (orow_uptr->name == crow->outputName) { src_orow = orow_uptr.get(); break; }
            if (!src_orow) continue;
            float headerRight = std::max(src_orow->label.translation.x + src_orow->label.scale.x,
                                         src_orow->addButton.translation.x + src_orow->addButton.scale.x);
            XMFLOAT2 astart(headerRight + 6.0f, src_orow->label.translation.y + src_orow->label.scale.y * 0.5f);

            auto testSegment = [&](const XMFLOAT2& A, const XMFLOAT2& B) {
              // simple distance to line segment squared
              XMFLOAT2 AB(B.x - A.x, B.y - A.y);
              float ab2 = AB.x * AB.x + AB.y * AB.y;
              float t = 0;
              if (ab2 > 0) t = std::max(0.f, std::min(1.f, ((p.x - A.x) * AB.x + (p.y - A.y) * AB.y) / ab2));
              XMFLOAT2 H(A.x + AB.x * t, A.y + AB.y * t);
              float dx = p.x - H.x; float dy = p.y - H.y; return dx * dx + dy * dy;
            };

            // through anchors (shared hubs)
            if (!crow->anchorHubIds.empty()) {
              for (size_t i = 0; i < crow->anchorHubIds.size(); ++i) {
                const auto* hub = GetHub(crow->anchorHubIds[i]); if (!hub) continue;
                XMFLOAT2 ap(scrollable_area.translation.x + hub->pos.x, scrollable_area.translation.y + hub->pos.y);
                if (testSegment(astart, ap) <= thr2) { hit = crow; break; }
                astart = ap;
              }
            }
            if (hit) { selectedConnection = hit; break; }
            // to each target (test last segment to target input)
            const std::string input_name = crow->input.GetText();
            for (const Node* target_node : targets) {
              const wi::gui::Label* target_input_label = nullptr;
              if (!input_name.empty()) {
                for (const auto& ilbl_uptr : target_node->inputLabels) if (ilbl_uptr->GetText() == input_name) { target_input_label = ilbl_uptr.get(); break; }
              }
              XMFLOAT2 end(target_node->window.translation.x + 6.0f, target_input_label ? (target_input_label->translation.y + target_input_label->scale.y * 0.5f) : (target_node->window.translation.y + target_node->window.scale.y * 0.5f));
              if (testSegment(astart, end) <= thr2) { hit = crow; break; }
            }
            if (hit) { selectedConnection = hit; break; }
          }
          if (hit) break;
        }
        if (!hit) selectedConnection = nullptr;
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
        selectedConnection->anchorHubIds.push_back(hid);
      }
    }
  }

  // Unify duplicate connections: ensure only one representative per (output,target,input,param,delay)
  // and redirect selection to representative. Also unify anchors to representative hubs.
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
        // unify hubs: bind duplicate to master's hub chain
        if (cr->anchorHubIds != master->anchorHubIds) {
          // release old hubs if unreferenced
          auto old = cr->anchorHubIds;
          cr->anchorHubIds = master->anchorHubIds;
          for (auto hid : old) {
            bool stillUsed = false;
            // check other connections
            for (auto& cr2_uptr : n->connectionRows) {
              auto* cr2 = cr2_uptr.get();
              if (cr2 == cr) continue;
              for (auto id2 : cr2->anchorHubIds) { if (id2 == hid) { stillUsed = true; break; } }
              if (stillUsed) break;
            }
            if (!stillUsed) DeleteHubIfUnreferenced(hid);
          }
        }
      }
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

  // Ensure child node windows finalize their layout after any connection/UI changes this frame,
  // then snapshot stable pin positions to avoid transient jitter during render.
  if (layoutDirty) {
    for (auto& n : nodes) {
      n->window.Update(canvas, 0);
      n->ComputePinCache();
    }
    layoutDirty = false;
  }
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

  // Place Import button above Add Node
  importFromSceneButton.Detach();
  importFromSceneButton.SetSize(XMFLOAT2(separator - padding * 2, importFromSceneButton.GetSize().y));
  importFromSceneButton.SetPos(XMFLOAT2(
      translation.x + padding,
      addNodeButton.GetPos().y - importFromSceneButton.GetSize().y - padding));
  importFromSceneButton.AttachTo(this);

  // (Zoom slider removed)

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
  
}

void NodeEditorWindow::AddNode() {
  std::string name = "Node " + std::to_string(nodes.size() + 1);
  auto node = std::make_unique<Node>(name);
  node->window.Create(
      name,
      Window::WindowControls::MOVE |
          Window::WindowControls::CLOSE |
          Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL /*|
                              Window::WindowControls::DISABLE_TITLE_BAR*/);
  // Give more horizontal room so the Delay field is usable
  node->window.SetSize(XMFLOAT2(340, 110));

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

  // Build output rows with add buttons
  for (auto& outname : node->outputs) {
    node->AddOutputRow(this, outname);
  }
  // Build input labels (read-only visuals)
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
  layoutDirty = true;
 
  // Register the node window as a child widget so it can be rendered,
  // updated and receive input events (such as moving and interacting
  // with widgets inside it).
  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
  node->window.SetEnabled(true);
  node->window.SetVisible(true);

  // When the node window is closed, queue it for safe removal.
  Node* raw = node.get();
  node->window.OnClose([this, raw](wi::gui::EventArgs) {
    // Close button already hides the window; defer actual removal
    // to end of Update() to avoid mutating containers mid-iteration.
    pendingRemoval.push_back(raw);
  });

  nodes.push_back(std::move(node));
  // index by name for fast lookup (support duplicates)
  nodeIndex[nodes.back()->name].push_back(nodes.back().get());
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
}

void NodeEditorWindow::AddNodeForEntity(wi::ecs::Entity ent, const std::string& name) {
  if (ent == wi::ecs::INVALID_ENTITY) return;
  if (entityIndex.find(ent) != entityIndex.end()) return; // already exists

  auto node = std::make_unique<Node>(name);
  node->entity = ent;
  node->window.Create(
      name,
      Window::WindowControls::MOVE |
          Window::WindowControls::CLOSE |
          Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL);
  node->window.SetSize(XMFLOAT2(280, 100));

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
  }
  node->LayoutRows();

  // Register the node window to render/update
  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::SCROLLABLE);
  node->window.SetEnabled(true);
  node->window.SetVisible(true);

  Node* raw = node.get();
  node->window.OnClose([this, raw](wi::gui::EventArgs) {
    pendingRemoval.push_back(raw);
  });

  entityIndex[ent] = raw;
  nodes.push_back(std::move(node));
  nodeIndex[raw->name].push_back(raw);
  // Center the newly added node like AddNode()
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
  layoutDirty = true;
}

// ----- Node UI helpers -----
void NodeEditorWindow::Node::AddOutputRow(NodeEditorWindow* owner, const std::string& outputName) {
  auto row = std::make_unique<OutputUI>();
  row->name = outputName;
  row->label.Create(outputName);
  row->label.SetLocalizationEnabled(false);
  row->label.SetShadowRadius(0);
  row->label.font.params.size = 14; // smaller label near pin
  window.AddWidget(&row->label);

  row->addButton.Create("Add");
  row->addButton.SetLocalizationEnabled(false);
  row->addButton.SetShadowRadius(0);
  row->addButton.SetSize(XMFLOAT2(40, 20));
  row->addButton.OnClick([this, owner, outputName](wi::gui::EventArgs) {
    this->AddConnectionRow(owner, outputName);
    this->LayoutRows();
  });
  window.AddWidget(&row->addButton);

  outputRows.push_back(std::move(row));
  if (owner) owner->layoutDirty = true;
}

NodeEditorWindow::Node::ConnectionUI* NodeEditorWindow::Node::AddConnectionRow(NodeEditorWindow* owner, const std::string& outputName) {
  auto row = std::make_unique<ConnectionUI>();
  row->outputName = outputName;

  row->outLabel.Create(outputName);
  row->outLabel.SetLocalizationEnabled(false);
  row->outLabel.SetShadowRadius(0);
  row->outLabel.font.params.size = 16;
  window.AddWidget(&row->outLabel);

  row->target.Create("Target");
  row->target.SetLocalizationEnabled(false);
  row->target.SetShadowRadius(0);
  row->target.SetValue("!self");
  window.AddWidget(&row->target);

  row->input.Create("Input");
  row->input.SetLocalizationEnabled(false);
  row->input.SetShadowRadius(0);
  row->input.SetValue("FunctionName");
  window.AddWidget(&row->input);

  row->param.Create("Param");
  row->param.SetLocalizationEnabled(false);
  row->param.SetShadowRadius(0);
  row->param.SetValue("");
  window.AddWidget(&row->param);

  row->delay.Create("Delay");
  row->delay.SetLocalizationEnabled(false);
  row->delay.SetShadowRadius(0);
  row->delay.SetValue("0.0");
  window.AddWidget(&row->delay);

  row->removeButton.Create("x");
  row->removeButton.SetLocalizationEnabled(false);
  row->removeButton.SetShadowRadius(0);
  row->removeButton.SetSize(XMFLOAT2(18, 20));
  row->removeButton.OnClick([this, owner, ptr=row.get()](wi::gui::EventArgs){
    this->RemoveConnectionRow(owner, ptr);
    this->LayoutRows();
  });
  window.AddWidget(&row->removeButton);

  ConnectionUI* ret = row.get();
  connectionRows.push_back(std::move(row));
  if (owner) owner->layoutDirty = true;
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

  // Erase from list
  // Save hubs to clean up after erasing
  auto hubs_to_check = row->anchorHubIds;
  for (auto it = connectionRows.begin(); it != connectionRows.end(); ++it) {
    if (it->get() == row) { connectionRows.erase(it); break; }
  }
  for (auto hid : hubs_to_check) {
    owner->DeleteHubIfUnreferenced(hid);
  }
  if (owner) owner->layoutDirty = true;
}

void NodeEditorWindow::Node::LayoutRows() {
  const float padding = 6.0f;
  const float section_gap = 12.0f;
  const float label_h = 18.0f;
  const float row_h = 22.0f;
  const float start_x = 6.0f;
  const float w = window.GetSize().x - start_x * 2.0f;

  // 1) Inputs section (top area, left side)
  float y_inputs = 36.0f; // more breathing room under title bar
  for (auto& il : inputLabels) {
    il->SetPos(XMFLOAT2(start_x, y_inputs));
    il->SetSize(XMFLOAT2(w * 0.5f - padding, row_h));
    y_inputs += row_h + padding;
  }

  // 2) Outputs section (below inputs)
  float y = y_inputs + section_gap;
  for (auto& orow : outputRows) {
    // Output header row: small label next to pin, add button at far right
    const float pinlabel_w = std::min(100.0f, w * 0.25f);
    orow->label.SetPos(XMFLOAT2(start_x + w - 40.0f - padding - pinlabel_w, y));
    orow->label.SetSize(XMFLOAT2(pinlabel_w, label_h));
    orow->addButton.SetPos(XMFLOAT2(start_x + w - 40.0f, y));
    y += label_h + padding;

    // Connection rows that belong to this output
    for (auto& crow : connectionRows) {
      if (crow->outputName != orow->name) continue;

      float x = start_x;
      const float remove_w = 18.0f;
      const float aw = std::max(0.0f, w - remove_w - padding * 5.0f);
      const float out_w    = aw * 0.18f;
      const float target_w = aw * 0.28f;
      const float input_w  = aw * 0.20f;
      const float param_w  = aw * 0.20f;
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
  nodeIndex[newname].push_back(node);
}

// (Zoom functionality removed)

// Shared hub helpers:
NodeEditorWindow::RerouteHub* NodeEditorWindow::GetHub(uint32_t id) {
  for (auto& h : hubs) if (h.id == id) return &h;
  return nullptr;
}
const NodeEditorWindow::RerouteHub* NodeEditorWindow::GetHub(uint32_t id) const {
  for (auto& h : hubs) if (h.id == id) return &h;
  return nullptr;
}
uint32_t NodeEditorWindow::CreateHub(const XMFLOAT2& local) {
  RerouteHub h; h.id = nextHubId++; h.pos = local; hubs.push_back(h); return h.id;
}
void NodeEditorWindow::DeleteHubIfUnreferenced(uint32_t id) {
  bool used = false;
  for (auto& n : nodes) {
    for (auto& cr : n->connectionRows) {
      for (auto hid : cr->anchorHubIds) { if (hid == id) { used = true; break; } }
      if (used) break;
    }
    if (used) break;
  }
  if (!used) {
    for (size_t i = 0; i < hubs.size(); ++i) { if (hubs[i].id == id) { hubs.erase(hubs.begin() + i); break; } }
  }
}
