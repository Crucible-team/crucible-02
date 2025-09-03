// clang-format off
#include "stdafx.h"
#include "NodeEditorWindow.h"
#include "wiImage.h"
// clang-format on

using namespace wi::graphics;
using namespace wi::gui;

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

  // (Zoom slider removed)

  SetVisible(false);
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
      // Inputs on left
      for (auto& il : n->inputLabels) {
        float cx = wnd.translation.x + 6.0f; // near left edge
        float cy = il->translation.y + il->scale.y * 0.5f;
        wi::image::Params pin;
        pin.pos = XMFLOAT3(cx, cy, 0);
        pin.siz = XMFLOAT2(pinR * 2, pinR * 2);
        pin.pivot = XMFLOAT2(0.5f, 0.5f);
        pin.color = XMFLOAT4(0.7f, 0.9f, 1.0f, 1);
        pin.enableCornerRounding();
        for (int i = 0; i < arraysize(pin.corners_rounding); ++i) pin.corners_rounding[i].radius = pinR;
        wi::image::Draw(nullptr, pin, cmd);
      }
      // Outputs on right: one pin per output header row, with self-target highlight
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

        float cx = wnd.translation.x + wnd.scale.x - 6.0f;
        float cy = orow->label.translation.y + orow->label.scale.y * 0.5f;
        wi::image::Params pin;
        pin.pos = XMFLOAT3(cx, cy, 0);
        pin.siz = XMFLOAT2(pinR * 2, pinR * 2);
        pin.pivot = XMFLOAT2(0.5f, 0.5f);
        pin.color = XMFLOAT4(1.0f, 0.8f, 0.3f, 1);
        pin.enableCornerRounding();
        for (int i = 0; i < arraysize(pin.corners_rounding); ++i) pin.corners_rounding[i].radius = pinR;
        wi::image::Draw(nullptr, pin, cmd);
      }
    }

    // Draw connection wires based on connection rows
    for (const auto& n : nodes) {
      const auto& wnd = n->window;
      for (const auto& crow_uptr : n->connectionRows) {
        const auto* crow = crow_uptr.get();
        // find source output header row by name
        const Node::OutputUI* src_orow = nullptr;
        for (const auto& orow_uptr : n->outputRows) {
          if (orow_uptr->name == crow->outputName) { src_orow = orow_uptr.get(); break; }
        }
        if (!src_orow) continue;

        // Source pin position (right side of source node header row)
        const float sx = wnd.translation.x + wnd.scale.x - 6.0f;
        const float sy = src_orow->label.translation.y + src_orow->label.scale.y * 0.5f;

        // Determine target nodes by target field (can be multiple)
        std::string target_text = crow->target.GetText();
        if (target_text == "!self" || target_text.empty()) {
          // self target: don't draw a wire, it's indicated by row highlight
          continue;
        }

        wi::vector<const Node*> targets;
        for (const auto& other : nodes) {
          if (other.get() == n.get()) continue;
          if (other->name == target_text || other->label.GetText() == target_text) {
            targets.push_back(other.get());
          }
        }
        if (targets.empty()) {
          continue; // no matching targets
        }

        // Determine target input label by input field
        const std::string input_name = crow->input.GetText();
        for (const Node* target_node : targets) {
          const wi::gui::Label* target_input_label = nullptr;
          if (!input_name.empty())
          {
            for (const auto& ilbl_uptr : target_node->inputLabels) {
              if (ilbl_uptr->GetText() == input_name) { target_input_label = ilbl_uptr.get(); break; }
            }
          }
          if (!target_input_label && !target_node->inputLabels.empty()) {
            target_input_label = target_node->inputLabels.front().get();
          }

          float ex;
          float ey;
          if (target_input_label) {
            ex = target_node->window.translation.x + 6.0f;
            ey = target_input_label->translation.y + target_input_label->scale.y * 0.5f;
          } else {
            // Fallback to center-left of target window
            ex = target_node->window.translation.x + 6.0f;
            ey = target_node->window.translation.y + target_node->window.scale.y * 0.5f;
          }

          wi::gui::DrawWireBezierStrip(
            XMFLOAT2(sx, sy), XMFLOAT2(ex, ey),
            2.0f,
            XMFLOAT4(0.6f, 1.0f, 0.6f, 1),
            canvas, cmd);
        }
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

  // Process any node removals queued during child updates (e.g., close button)
  if (!pendingRemoval.empty()) {
    // Copy to avoid side effects while removing
    auto toRemove = pendingRemoval;
    pendingRemoval.clear();
    for (auto* n : toRemove) {
      RemoveNode(n);
    }
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
  recentlyAddedNewNode = true;
  lastAddedNode = raw;
  ResizeLayout();
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
}

void NodeEditorWindow::Node::AddConnectionRow(NodeEditorWindow* owner, const std::string& outputName) {
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

  connectionRows.push_back(std::move(row));
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
  for (auto it = connectionRows.begin(); it != connectionRows.end(); ++it) {
    if (it->get() == row) { connectionRows.erase(it); break; }
  }
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

void NodeEditorWindow::RemoveNode(Node* node) {
  if (!node) return;

  // Remove GUI widget linkage first
  RemoveWidget(&node->window);
  node->window.Detach();
  node->window.RemoveWidgets();

  // Erase the node from our list (unique_ptr will clean up)
  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    if (it->get() == node) {
      nodes.erase(it);
      break;
    }
  }

  // No global relayout on removal to preserve existing positions
}

// (Zoom functionality removed)
