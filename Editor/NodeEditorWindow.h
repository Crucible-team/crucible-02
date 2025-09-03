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
    // I/O visualization data for this node:
    wi::vector<std::string> inputs;   // function names (sinks)
    wi::vector<std::string> outputs;  // event names (sources)

    struct OutputUI {
      std::string name;
      wi::gui::Label label;
      wi::gui::Button addButton;
    };
    struct ConnectionUI {
      std::string outputName; // which output this connection belongs to
      wi::gui::Label outLabel; // per-row output name label
      wi::gui::TextInputField target;  // "!self" or entity id
      wi::gui::TextInputField input;   // function name to call
      wi::gui::TextInputField param;   // optional param
      wi::gui::TextInputField delay;   // seconds
      wi::gui::Button removeButton;
    };
    wi::vector<std::unique_ptr<OutputUI>> outputRows;
    wi::vector<std::unique_ptr<ConnectionUI>> connectionRows;
    wi::vector<std::unique_ptr<wi::gui::Label>> inputLabels; // visual list of inputs
    wi::gui::Label bottomSpacer; // pushes window bottom to add padding

    void AddOutputRow(NodeEditorWindow* owner, const std::string& outputName);
    void AddConnectionRow(NodeEditorWindow* owner, const std::string& outputName);
    void RemoveConnectionRow(NodeEditorWindow* owner, ConnectionUI* row);
    void LayoutRows();

    Node(const std::string &name) : name(name) {}
    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;
  };

  EditorComponent *editor = nullptr;

  wi::vector<std::unique_ptr<Node>> nodes;
  wi::gui::Button addNodeButton;
  bool recentlyAddedNewNode = false;

  void Update(const wi::Canvas &canvas, float dt) override;
  void Render(const wi::Canvas &canvas,
              wi::graphics::CommandList cmd) const override;
  void ResizeLayout() override;

private:
  void AddNode();
  void RemoveNode(Node* node);
  wi::vector<Node*> pendingRemoval;
  Node* lastAddedNode = nullptr; // track the last created node for centering
};
