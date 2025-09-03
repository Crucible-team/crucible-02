#include "stdafx.h"
#include "NodeEditorWindow.h"
#include "Editor.h"
#include "wiRenderer.h"

using namespace wi::graphics;
using namespace wi::gui;

void NodeEditorWindow::Create(EditorComponent* _editor)
{
	editor = _editor;
	wi::gui::Window::Create("Node Editor");
	SetSize(XMFLOAT2(800, 600));
	SetMinimized(true);
	SetVisible(false);

	addNodeButton.Create("Add Node");
	addNodeButton.SetPos(XMFLOAT2(4, 4));
	addNodeButton.SetSize(XMFLOAT2(100, 20));
	addNodeButton.SetTooltip("Create a new node");
	addNodeButton.OnClick([this](wi::gui::EventArgs args) {
		XMFLOAT2 pos = XMFLOAT2(50 + nodes.size() * 120.0f, 80.0f);
		AddNode("Node " + std::to_string(nodes.size()), pos);
	});
	AddWidget(&addNodeButton);
}

void NodeEditorWindow::AddNode(const std::string& name, const XMFLOAT2& position)
{
        nodes.emplace_back(std::make_unique<Node>());
        Node& node = *nodes.back();
        node.window.Create(name, Window::WindowControls::MOVE | Window::WindowControls::DISABLE_TITLE_BAR);
        node.window.SetSize(XMFLOAT2(120, 60));
        node.window.SetPos(position);
        node.window.SetVisible(true);
        node.window.SetColor(wi::Color(60, 60, 60, 200));

        AddWidget(&node.window);

	if (nodes.size() > 1)
	{
		links.emplace_back(nodes.size() - 2, nodes.size() - 1);
	}
}

void NodeEditorWindow::Update(const wi::Canvas& canvas, float dt)
{
	wi::gui::Window::Update(canvas, dt);
}

void NodeEditorWindow::Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const
{
	wi::gui::Window::Render(canvas, cmd);

	if (!IsVisible())
		return;

	wi::gui::Window::ApplyScissor(canvas, scissorRect, cmd);

	for (auto& link : links)
	{
		if (link.first >= nodes.size() || link.second >= nodes.size())
			continue;

                const Node& a = *nodes[link.first];
                const Node& b = *nodes[link.second];

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

void NodeEditorWindow::ResizeLayout()
{
	wi::gui::Window::ResizeLayout();
	addNodeButton.SetPos(XMFLOAT2(4, 4));
}

