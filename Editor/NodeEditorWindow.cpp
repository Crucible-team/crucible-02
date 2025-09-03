#include "NodeEditorWindow.h"
#include "Editor.h"
#include "stdafx.h"
#include "wiImage.h"
#include "wiRenderer.h"

using namespace wi::graphics;
using namespace wi::gui;

static const float separator = 140.0f;

void NodeEditorWindow::Create(EditorComponent *_editor) {
  editor = _editor;
  control_size = 30;
  wi::gui::Window::Create("Node Editor");
  RemoveWidget(&scrollbar_horizontal);
  RemoveWidget(&scrollbar_vertical);
  SetVisible(false);

  addNodeButton.Create("Add Node");
  addNodeButton.SetTooltip("Create a new node");
  addNodeButton.SetLocalizationEnabled(false);
  addNodeButton.SetSize(XMFLOAT2(100, 20));
  addNodeButton.OnClick([this](wi::gui::EventArgs args) {
    XMFLOAT2 pos = XMFLOAT2(separator + 20 + nodes.size() * 120.0f, 80.0f);
    AddNode("Node " + std::to_string(nodes.size()), pos);
  });
  AddWidget(&addNodeButton, wi::gui::Window::AttachmentOptions::NONE);
}

void NodeEditorWindow::AddNode(const std::string &name,
                               const XMFLOAT2 &position) {
  nodes.emplace_back(std::make_unique<Node>());
  Node &node = *nodes.back();
  node.window.Create(name, Window::WindowControls::MOVE |
                               Window::WindowControls::DISABLE_TITLE_BAR);
  node.window.SetSize(XMFLOAT2(120, 60));
  node.window.SetPos(position);
  node.window.SetVisible(true);
  node.window.SetColor(wi::Color(60, 60, 60, 200));

  AddWidget(&node.window, wi::gui::Window::AttachmentOptions::NONE);

  if (nodes.size() > 1) {
    links.emplace_back(nodes.size() - 2, nodes.size() - 1);
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

  addNodeButton.SetShadowRadius(0);

  for (auto &node : nodes) {
    node->window.SetShadowRadius(4);
    for (auto &spr : node->window.sprites) {
      spr.params.enableCornerRounding();
      spr.params.corners_rounding[0].radius = radius;
      spr.params.corners_rounding[1].radius = radius;
      spr.params.corners_rounding[2].radius = radius;
      spr.params.corners_rounding[3].radius = radius;
    }
  }
}

void NodeEditorWindow::Render(const wi::Canvas &canvas,
                              wi::graphics::CommandList cmd) const {
  wi::gui::Window::Render(canvas, cmd);

  if (!IsVisible())
    return;

  wi::gui::Window::ApplyScissor(canvas, scissorRect, cmd);

  // draw separator similar to Content Browser
  wi::image::Params params;
  params.pos =
      XMFLOAT3(translation.x + separator, translation.y + control_size, 0);
  params.siz = XMFLOAT2(2, scale.y - control_size);
  params.color = shadow_color;
  wi::image::Draw(nullptr, params, cmd);

  for (auto &link : links) {
    if (link.first >= nodes.size() || link.second >= nodes.size())
      continue;

    const Node &a = *nodes[link.first];
    const Node &b = *nodes[link.second];

    wi::renderer::RenderableLine2D line;
    line.start = XMFLOAT2(a.window.GetPos().x + a.window.GetSize().x * 0.5f,
                          a.window.GetPos().y + a.window.GetSize().y * 0.5f);
    line.end = XMFLOAT2(b.window.GetPos().x + b.window.GetSize().x * 0.5f,
                        b.window.GetPos().y + b.window.GetSize().y * 0.5f);
    line.color_start = wi::Color::White();
    line.color_end = wi::Color::White();
    wi::renderer::DrawLine(line);
  }
}

void NodeEditorWindow::ResizeLayout() {
  wi::gui::Window::ResizeLayout();
  const float padding = 4;

  addNodeButton.Detach();
  addNodeButton.SetPos(XMFLOAT2(translation.x + padding,
                                translation.y + control_size + padding));
  addNodeButton.SetSize(
      XMFLOAT2(separator - padding * 2, addNodeButton.GetSize().y));
  addNodeButton.AttachTo(this);
}
