#pragma once
class EditorComponent;

// Minimal node editor window mirroring Content Browser layout.
// Functionality will be extended with actual node editing later.
class NodeEditorWindow : public wi::gui::Window {
public:
  void Create(EditorComponent *editor);

  struct Node {
    wi::gui::Window window;
    wi::gui::Label label;
    std::string name;
    Node(const std::string &name) : name(name) {}
    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;
  };

  EditorComponent *editor = nullptr;

  wi::vector<std::unique_ptr<Node>> nodes;
  wi::gui::Button addNodeButton;

  void Update(const wi::Canvas &canvas, float dt) override;
  void Render(const wi::Canvas &canvas,
              wi::graphics::CommandList cmd) const override;
  void ResizeLayout() override;

private:
  void AddNode();
};
