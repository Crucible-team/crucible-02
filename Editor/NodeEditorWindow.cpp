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

  addNodeButton.Create("Add Node");
  addNodeButton.SetLocalizationEnabled(false);
  addNodeButton.SetSize(XMFLOAT2(120, 25));
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
}
