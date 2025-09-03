#pragma once
#include <memory>
class EditorComponent;

// Simple node-based editor window for experimenting with IO graphs
// Nodes are represented as small movable windows and connections are drawn between them.
class NodeEditorWindow : public wi::gui::Window
{
public:
	void Create(EditorComponent* editor);

	EditorComponent* editor = nullptr;

	// button to add new nodes
	wi::gui::Button addNodeButton;

        struct Node
        {
                wi::gui::Window window; // visual representation of the node

                Node() = default;
                Node(const Node&) = delete;
                Node& operator=(const Node&) = delete;
                Node(Node&&) = default;
                Node& operator=(Node&&) = default;
        };

        // collection of nodes and links
        wi::vector<std::unique_ptr<Node>> nodes;
	wi::vector<std::pair<size_t, size_t>> links;

	void AddNode(const std::string& name, const XMFLOAT2& position);

	void Update(const wi::Canvas& canvas, float dt) override;
	void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
	void ResizeLayout() override;
};

