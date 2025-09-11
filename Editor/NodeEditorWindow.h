#include <cfloat>
#pragma once
class EditorComponent;
#include <unordered_map>
#include <unordered_set>
#include "wiECS.h"

class NodeEditorWindow : public wi::gui::Window {
public:
  void Create(EditorComponent *editor);
  void BuildNodesFromSceneMetadata();
  void SetLayoutDirty() { layoutDirty = true; }
  // Public helper to add a single entity's node to the graph
  //void AddEntityToGraph(wi::ecs::Entity entity);

  struct Node {
    enum class NodeType { LogicOnly, EntityBound };
    wi::gui::Window window;
    wi::gui::Label label;
    std::string name;
    wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
    NodeType type = NodeType::LogicOnly;
    uint64_t uid = 0; // stable identity within Node Editor
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
      wi::gui::TextInputField refire;  // refire count (-1=infinite)
      wi::gui::Button removeButton;
      wi::vector<uint32_t> anchorHubIds;    // reroute anchors referencing shared hubs
      // Optional strong binding to a specific entity target; UI target text mirrors NameComponent
      wi::ecs::Entity targetEntity = wi::ecs::INVALID_ENTITY;
      uint64_t uid = 0; // stable identity of this connection row
      // Last committed values for undo of text edits:
      std::string lastTarget;
      std::string lastInput;
      std::string lastParam;
      std::string lastDelay;
      std::string lastRefire;
    };
    wi::vector<std::unique_ptr<OutputUI>> outputRows;
    wi::vector<std::unique_ptr<ConnectionUI>> connectionRows;
    wi::vector<std::unique_ptr<wi::gui::Label>> inputLabels;
    wi::gui::Label bottomSpacer; // pushes window bottom to add padding
    // Preset (non-removable) output names derived from class preset or logic defaults
    std::unordered_set<std::string> presetOutputs;

    // Cached pin positions (in screen space) to avoid one-frame jitter
    wi::vector<XMFLOAT2> cachedInputPins; // one-to-one with inputLabels
    struct CachedOutputPin { OutputUI* row = nullptr; XMFLOAT2 pos = XMFLOAT2(0,0); };
    wi::vector<CachedOutputPin> cachedOutputPins; // one per outputRows
    bool pinCacheDirty = true; // set when node moves or layout changes
    XMFLOAT2 lastWindowPos = XMFLOAT2(FLT_MAX, FLT_MAX);

    // Fast lookup indices for inputs/outputs (names are unique per node):
    std::unordered_map<std::string, size_t> inputIndex;      // input name -> inputLabels index
    std::unordered_map<std::string, OutputUI*> outputIndex;  // output name -> OutputUI*

    // Autosize helpers
    float measuredContentWidth = 0.0f;   // last measured required content width from LayoutRows()
    float autosizeShrinkTimer = 0.0f;    // accumulates dt while eligible to shrink
    float autosizeThrottleTimer = 0.0f;  // limits live relayout frequency while typing
    bool needsLayout = false;            // per-node layout dirty
    bool activeEditing = false;          // true if any field in this node is active

    void AddOutputRow(NodeEditorWindow* owner, const std::string& outputName);
    ConnectionUI* AddConnectionRow(NodeEditorWindow* owner, const std::string& outputName, uint64_t forced_uid = 0);
    void RemoveConnectionRow(NodeEditorWindow* owner, ConnectionUI* row);
    // Remove an output header row if it's custom and has no connections
    void RemoveOutputHeader(NodeEditorWindow* owner, const std::string& outputName);
    // Remove any custom output headers that have no connections
    void PruneCustomEmptyOutputHeaders(NodeEditorWindow* owner);
    // Check if an output name is part of the preset (non-removable) set
    bool IsPresetOutput(const std::string& name) const;
    void LayoutRows();
    void ComputePinCache();
    void RebuildInputIndex() {
      inputIndex.clear();
      for (size_t i = 0; i < inputLabels.size(); ++i) {
        if (inputLabels[i]) inputIndex[inputLabels[i]->GetText()] = i;
      }
    }
    void RebuildOutputIndex() {
      outputIndex.clear();
      for (auto& r : outputRows) if (r) outputIndex[r->name] = r.get();
    }
    bool GetInputIndex(const std::string& name, size_t& out) const {
      auto it = inputIndex.find(name);
      if (it == inputIndex.end()) return false;
      out = it->second;
      return true;
    }
    OutputUI* FindOutputRow(const std::string& outputName) {
      auto it = outputIndex.find(outputName);
      if (it != outputIndex.end()) return it->second;
      for (auto& r : outputRows) if (r->name == outputName) { outputIndex[r->name] = r.get(); return r.get(); }
      return nullptr;
    }
    const OutputUI* FindOutputRow(const std::string& outputName) const {
      auto it = outputIndex.find(outputName);
      if (it != outputIndex.end()) return it->second;
      for (const auto& r : outputRows) if (r->name == outputName) return r.get();
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
  // Called by Editor when active scene tab changes
  void OnActiveSceneChanged();
  // External notifications:
  void OnEntityRenamed(wi::ecs::Entity entity, const std::string& newname);
  // External sync: rebuild an entity-bound node from Scene::EntityOutputsComponent
  void RefreshNodeFromOutputsComponent(wi::ecs::Entity entity);
  // Persistence (JSON): save/load full node graph
  bool SaveGraph(const std::string& path) const;
  bool LoadGraph(const std::string& path);
  bool MergeGraph(const std::string& path);
  // In-memory serialization (no disk IO). If persist_entity_metadata is true,
  // entity-bound node uids are written to scene metadata; leave false for tab caching.
  bool SerializeGraphToString(std::string& out, bool persist_entity_metadata = false) const;
  bool LoadGraphFromString(const std::string& json_text);
  bool MergeGraphFromString(const std::string& json_text);

private:
  // Suppress writing back to EntityOutputsComponent during programmatic builds/imports
  bool suppressComponentSync = false;
  // Generate a unique node name by appending (n) when necessary
  std::string MakeUniqueNodeName(const std::string& base) const;
  // Sync UI <-> Scene::EntityOutputsComponent for EntityBound nodes
  void SyncEntityOutputsFromNode(Node* node);
  void PopulateConnectionsFromComponent(Node* node);
  // Wire style tuning for tangent-based reroute rendering/selection
  static constexpr float WIRE_ENDPOINT_BIAS = 60.0f; // horizontal bias at ends
  static constexpr float WIRE_MIN_HANDLE = 10.0f;    // minimum handle length
  static constexpr float WIRE_CLAMP_K = 0.45f;       // max handle = K * segment length

  void AddNode();
  void AddNodeForEntity(wi::ecs::Entity entity, const std::string& name, const std::string& classtype);
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
  Node::ConnectionUI* hoveredConnection = nullptr; // preselection under cursor
  struct AnchorDrag {
    bool active = false;
    Node::ConnectionUI* conn = nullptr;
    int index = -1;
    XMFLOAT2 oldPos = XMFLOAT2(0,0);
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

  // ---- Undo/Redo ----
  struct ConnectionSnapshot {
    uint64_t node_uid = 0;
    uint64_t conn_uid = 0;
    std::string outputName;
    std::string target;
    std::string input;
    std::string param;
    std::string delay;
    std::string refire;
    wi::vector<uint32_t> hubIds;
  };
  enum class UndoType {
    None,
    AddConnection,
    RemoveConnection,
    EditConnection,          // before/after snapshots
    SetConnectionHubs,       // before/after hub list
    MoveHub,
    Macro
  };
  struct UndoCommand {
    UndoType type = UndoType::None;
    std::string label;
    // Payloads:
    ConnectionSnapshot snap;           // Add/Remove
    ConnectionSnapshot before, after;  // Edit / SetConnectionHubs
    struct { uint32_t hubId = 0; XMFLOAT2 from = {}, to = {}; } movehub;
    struct HubCreate { uint32_t id = 0; XMFLOAT2 pos = XMFLOAT2(0,0); };
    wi::vector<HubCreate> hubs_undo_create; // hubs to (re)create before applying Undo
    wi::vector<HubCreate> hubs_redo_create; // hubs to (re)create before applying Redo
    wi::vector<UndoCommand> macro;
  };
  wi::vector<UndoCommand> undoStack;
  wi::vector<UndoCommand> redoStack;
  static constexpr size_t UNDO_LIMIT = 256;

  // id maps
  uint64_t nextNodeUid = 1;
  uint64_t nextConnUid = 1;
  std::unordered_map<uint64_t, Node*> nodesByUid;
  std::unordered_map<uint64_t, Node::ConnectionUI*> connsByUid;

  void RegisterNode(Node* n);
  void UnregisterNode(Node* n);
  void RegisterConnection(Node* owner, Node::ConnectionUI* row);
  void UnregisterConnection(Node* owner, Node::ConnectionUI* row);
  Node* FindNode(uint64_t uid) const;
  Node::ConnectionUI* FindConnection(uint64_t uid) const;

  ConnectionSnapshot MakeSnapshot(const Node* owner, const Node::ConnectionUI* row) const;
  void ApplySnapshotAdd(const ConnectionSnapshot& s);
  void ApplySnapshotRemove(const ConnectionSnapshot& s);
  void PushCommand(UndoCommand&& cmd);
  void Undo();
  void Redo();

  // Build shared wire path points (screen space) from a source point through hubs
  void BuildSharedPathPoints(const wi::vector<uint32_t>& hubIds, const XMFLOAT2& src, wi::vector<XMFLOAT2>& out_pts) const;
  // Variant that also emits per-point tokens for de-duplication
  void BuildSharedPathPointsAndTokens(const wi::vector<uint32_t>& hubIds, const XMFLOAT2& src,
                                      uint64_t src_token, uint64_t tag_src, uint64_t tag_hub,
                                      wi::vector<XMFLOAT2>& out_pts, wi::vector<uint64_t>& out_tokens) const;

};
