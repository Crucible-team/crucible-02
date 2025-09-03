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

  RemoveWidget(&scrollbar_horizontal);
  RemoveWidget(&scrollbar_vertical);
  scrollable_area.Detach();
  scrollable_area.SetEnabled(false);
  scrollable_area.SetVisible(false);

  addNodeButton.Create("Add Node");
  addNodeButton.SetLocalizationEnabled(false);
  addNodeButton.SetSize(XMFLOAT2(120, 25));
  addNodeButton.OnClick([this](wi::gui::EventArgs) { AddNode(); });
  AddWidget(&addNodeButton, wi::gui::Window::AttachmentOptions::NONE);

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

    for (size_t i = 1; i < nodes.size(); ++i) {
      const auto &a = nodes[i - 1]->window;
      const auto &b = nodes[i]->window;
      XMFLOAT2 a_center(a.translation.x + a.scale.x * 0.5f,
                        a.translation.y + a.scale.y * 0.5f);
      XMFLOAT2 b_center(b.translation.x + b.scale.x * 0.5f,
                        b.translation.y + b.scale.y * 0.5f);
      float length = wi::math::Distance(a_center, b_center);
      float angle =
          std::atan2(b_center.y - a_center.y, b_center.x - a_center.x);
      XMFLOAT2 mid((a_center.x + b_center.x) * 0.5f,
                   (a_center.y + b_center.y) * 0.5f);
      wi::image::Params lineParams;
      lineParams.pos = XMFLOAT3(mid.x, mid.y, 0);
      lineParams.siz = XMFLOAT2(length, 2);
      lineParams.pivot = XMFLOAT2(0.5f, 0.5f);
      lineParams.rotation = angle;
      lineParams.color = XMFLOAT4(1, 1, 1, 1);
      wi::image::Draw(nullptr, lineParams, cmd);
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
}

void NodeEditorWindow::ResizeLayout() {
  wi::gui::Window::ResizeLayout();
  const float padding = 4;

  addNodeButton.Detach();
  addNodeButton.SetPos(
      XMFLOAT2(translation.x + padding,
               translation.y + scale.y - addNodeButton.GetSize().y - padding));
  addNodeButton.SetSize(
      XMFLOAT2(separator - padding * 2, addNodeButton.GetSize().y));
  addNodeButton.AttachTo(this);

  const float width = GetWidgetAreaSize().x;
  float y = translation.y + control_size + padding + 10;
  const float sep = translation.x + separator + 10;
  float hoffset = sep;
  for (auto &node : nodes) {
    node->window.Detach();
    node->window.SetPos(XMFLOAT2(hoffset, y));
    hoffset += node->window.GetSize().x + 15;
    if (hoffset + node->window.GetSize().x >= translation.x + width) {
      hoffset = sep;
      y += node->window.GetSize().y + 40;
    }
    node->window.AttachTo(this);
  }
}

void NodeEditorWindow::AddNode() {
  std::string name = "Node " + std::to_string(nodes.size() + 1);
  auto node = std::make_unique<Node>(name);
  node->window.Create(name, Window::WindowControls::MOVE |
                              Window::WindowControls::DISABLE_TITLE_BAR);
  node->window.SetSize(XMFLOAT2(120, 60));

  node->label.Create(name);
  node->label.SetText(name);
  node->label.SetPos(XMFLOAT2(4, 4));
  node->label.SetSize(XMFLOAT2(112, 20));
  node->window.AddWidget(&node->label);

  // Register the node window as a child widget so it can be rendered,
  // updated and receive input events (such as moving and interacting
  // with widgets inside it).
  AddWidget(&node->window, wi::gui::Window::AttachmentOptions::NONE);

  nodes.push_back(std::move(node));
  ResizeLayout();
}
