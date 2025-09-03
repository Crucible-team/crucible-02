#pragma once
class EditorComponent;

// Minimal node editor window mirroring Content Browser layout.
// Functionality will be extended with actual node editing later.
class NodeEditorWindow : public wi::gui::Window {
public:
  void Create(EditorComponent *editor);

  EditorComponent *editor = nullptr;

  void Update(const wi::Canvas &canvas, float dt) override;
  void Render(const wi::Canvas &canvas,
              wi::graphics::CommandList cmd) const override;
  void ResizeLayout() override;
};
