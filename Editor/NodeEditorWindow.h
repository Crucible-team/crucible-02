#include <cfloat>
#pragma once
class EditorComponent;
#include <unordered_map>
#include "wiECS.h"

class NodeEditorWindow : public wi::gui::Window {
public:
  void Create(EditorComponent *editor);
  void BuildNodesFromSceneMetadata();

  struct Node {
    enum class NodeType { LogicOnly, EntityBound };
    wi::gui::Window window;
    wi::gui::Label label;
    std::string name;
    wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
    NodeType type = NodeType::LogicOnly;
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
      // Optional strong binding to a specific entity target; UI target text mirrors NameComponent
      wi::ecs::Entity targetEntity = wi::ecs::INVALID_ENTITY;
    };
    wi::vector<std::unique_ptr<OutputUI>> outputRows;
    wi::vector<std::unique_ptr<ConnectionUI>> connectionRows;
    wi::vector<std::unique_ptr<wi::gui::Label>> inputLabels;
    wi::gui::Label bottomSpacer; // pushes window bottom to add padding

    // Cached pin positions (in screen space) to avoid one-frame jitter
    wi::vector<XMFLOAT2> cachedInputPins; // one-to-one with inputLabels
    struct CachedOutputPin { OutputUI* row = nullptr; XMFLOAT2 pos = XMFLOAT2(0,0); };
    wi::vector<CachedOutputPin> cachedOutputPins; // one per outputRows
    bool pinCacheDirty = true; // set when node moves or layout changes
    XMFLOAT2 lastWindowPos = XMFLOAT2(FLT_MAX, FLT_MAX);

    // Autosize helpers
    float measuredContentWidth = 0.0f;   // last measured required content width from LayoutRows()
    float autosizeShrinkTimer = 0.0f;    // accumulates dt while eligible to shrink
    float autosizeThrottleTimer = 0.0f;  // limits live relayout frequency while typing
    bool needsLayout = false;            // per-node layout dirty
    bool activeEditing = false;          // true if any field in this node is active

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
  wi::gui::Button importFromSceneButton;
  wi::gui::Button addTimerButton;
  wi::gui::Button addSequenceButton;
  bool recentlyAddedNewNode = false;

  void Update(const wi::Canvas &canvas, float dt) override;
  void Render(const wi::Canvas &canvas,
              wi::graphics::CommandList cmd) const override;
  void ResizeLayout() override;
  // External notifications:
  void OnEntityRenamed(wi::ecs::Entity entity, const std::string& newname);

private:
  // Wire style tuning for tangent-based reroute rendering/selection
  static constexpr float WIRE_ENDPOINT_BIAS = 60.0f; // horizontal bias at ends
  static constexpr float WIRE_MIN_HANDLE = 10.0f;    // minimum handle length
  static constexpr float WIRE_CLAMP_K = 0.45f;       // max handle = K * segment length

  void AddNode();
  void AddNodeForEntity(wi::ecs::Entity entity, const std::string& name);
  void AddTimerNode();
  void AddSequenceNode();
  void RemoveNode(Node* node);
  void RenameNode(Node* node, const std::string& newname);
  wi::vector<Node*> pendingRemoval;
  Node* lastAddedNode = nullptr; // track the last created node for centering
  bool layoutDirty = true;
  std::unordered_map<std::string, wi::vector<Node*>> nodeIndex; // fast name->nodes lookup (supports duplicates)
  std::unordered_map<wi::ecs::Entity, Node*> entityIndex; // entity->node lookup

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
    uint32_t refcount = 0;
  };
  std::unordered_map<uint32_t, RerouteHub> hubs;
  uint32_t nextHubId = 1;
  RerouteHub* GetHub(uint32_t id);
  const RerouteHub* GetHub(uint32_t id) const;
  uint32_t CreateHub(const XMFLOAT2& local);
  void DeleteHubIfUnreferenced(uint32_t id);

};
