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
      wi::vector<uint32_t> anchorHubIds;    // reroute anchors referencing shared hubs
    };
    wi::vector<std::unique_ptr<OutputUI>> outputRows;
    wi::vector<std::unique_ptr<ConnectionUI>> connectionRows;
    wi::vector<std::unique_ptr<wi::gui::Label>> inputLabels; // visual list of inputs
    wi::gui::Label bottomSpacer; // pushes window bottom to add padding

    // Cached pin positions (in screen space) to avoid one-frame jitter
    wi::vector<XMFLOAT2> cachedInputPins; // one-to-one with inputLabels
    struct CachedOutputPin { OutputUI* row = nullptr; XMFLOAT2 pos = XMFLOAT2(0,0); };
    wi::vector<CachedOutputPin> cachedOutputPins; // one per outputRows

    void AddOutputRow(NodeEditorWindow* owner, const std::string& outputName);
    ConnectionUI* AddConnectionRow(NodeEditorWindow* owner, const std::string& outputName);
    void RemoveConnectionRow(NodeEditorWindow* owner, ConnectionUI* row);
    void LayoutRows();
    void ComputePinCache();
    OutputUI* FindOutputRow(const std::string& outputName) {
      for (auto& r : outputRows) if (r->name == outputName) return r.get();
      return nullptr;
    }

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
  bool layoutDirty = true; // recompute pin caches only when layout changed

  // Drag & drop state for creating connections by dragging from output pins to input pins
  struct DragState {
    bool active = false;
    bool rightButton = false;
    bool fromAnchor = false;
    Node* srcNode = nullptr;
    Node::OutputUI* srcOutput = nullptr;
    XMFLOAT2 srcPos = XMFLOAT2(0, 0);
    XMFLOAT2 cursor = XMFLOAT2(0, 0);
    Node* hoverNode = nullptr;
    const wi::gui::Label* hoverInput = nullptr;
    // moving an existing connection (dragging from input pin)
    Node::ConnectionUI* movingConnection = nullptr;
    Node* movingOwner = nullptr;
  } drag;

  // Wire selection + anchor dragging
  Node::ConnectionUI* selectedConnection = nullptr;
  struct AnchorDrag {
    bool active = false;
    Node::ConnectionUI* conn = nullptr;
    int index = -1;
  } anchorDrag;
  struct AnchorRightOp {
    bool active = false;
    bool moved = false;
    Node* node = nullptr;
    Node::ConnectionUI* conn = nullptr;
    int index = -1;
    XMFLOAT2 start = XMFLOAT2(0,0);
  } anchorRight;

  // Shared reroute hubs (content-local positions)
  struct RerouteHub {
    uint32_t id = 0;
    XMFLOAT2 pos = XMFLOAT2(0,0);
  };
  wi::vector<RerouteHub> hubs;
  uint32_t nextHubId = 1;
  RerouteHub* GetHub(uint32_t id);
  const RerouteHub* GetHub(uint32_t id) const;
  uint32_t CreateHub(const XMFLOAT2& local);
  void DeleteHubIfUnreferenced(uint32_t id);

};
