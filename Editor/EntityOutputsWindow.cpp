#include "stdafx.h"
#include "EntityOutputsWindow.h"

using namespace wi::ecs;
using namespace wi::scene;

void EntityOutputsWindow::Create(EditorComponent* _editor)
{
    editor = _editor;
    wi::gui::Window::Create(ICON_OUTPUTS " Entity Outputs", wi::gui::Window::WindowControls::COLLAPSE | wi::gui::Window::WindowControls::CLOSE | wi::gui::Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL);
    SetSize(XMFLOAT2(540, 260));

    closeButton.SetTooltip("Delete EntityOutputsComponent");
    OnClose([=](wi::gui::EventArgs args) {
        wi::Archive& archive = editor->AdvanceHistory();
        archive << EditorComponent::HISTORYOP_COMPONENT_DATA;
        editor->RecordEntity(archive, entity);

        editor->GetCurrentScene().entityoutputs.Remove(entity);

        editor->RecordEntity(archive, entity);
        editor->componentsWnd.RefreshEntityTree();
    });

    addButton.Create("Add Output");
    addButton.SetTooltip("Add a new output binding row");
    addButton.OnClick([=](wi::gui::EventArgs args) {
        auto forEachSelected = [this](auto func) {
            return [this, func](auto args) {
                Scene& scene = editor->GetCurrentScene();
                for (auto& x : editor->translator.selected)
                {
                    auto* comp = scene.entityoutputs.GetComponent(x.entity);
                    if (comp)
                    {
                        func(*comp, args);
                    }
                }
            };
        };
        forEachSelected([](EntityOutputsComponent& comp, wi::gui::EventArgs args) {
            EntityOutputsComponent::OutputBinding b;
            b.event = "OnStart";
            b.target = "";
            b.input = "";
            b.parameter = "";
            b.delay = 0.0f;
            b.once = false;
            comp.outputs.push_back(std::move(b));
        })(args);
        RefreshRows();
    });
    AddWidget(&addButton);

    SetMinimized(true);
    SetVisible(false);
    SetLocalizationEnabled(false);
    SetEntity(INVALID_ENTITY);
}

void EntityOutputsWindow::SetEntity(Entity e)
{
    bool changed = e != this->entity;
    this->entity = e;

    Scene& scene = editor->GetCurrentScene();
    const EntityOutputsComponent* comp = scene.entityoutputs.GetComponent(entity);
    if (comp != nullptr)
    {
        if (changed)
        {
            RefreshRows();
        }
        SetEnabled(true);
    }
    else
    {
        this->entity = INVALID_ENTITY;
    }
}

void EntityOutputsWindow::RefreshRows()
{
    for (auto& row : rows)
    {
        RemoveWidget(&row.remove);
        RemoveWidget(&row.evt);
        RemoveWidget(&row.target);
        RemoveWidget(&row.input);
        RemoveWidget(&row.param);
        RemoveWidget(&row.delay);
        RemoveWidget(&row.once);
    }
    rows.clear();

    Scene& scene = editor->GetCurrentScene();
    EntityOutputsComponent* comp = scene.entityoutputs.GetComponent(entity);
    if (!comp)
        return;

    auto forEachSelectedWithRefresh = [this](size_t index, auto func) {
        return [this, func, index](auto args) {
            Scene& scene = editor->GetCurrentScene();
            for (auto& x : editor->translator.selected)
            {
                EntityOutputsComponent* comp = scene.entityoutputs.GetComponent(x.entity);
                if (comp != nullptr)
                {
                    if (index < comp->outputs.size())
                        func(*comp, args);
                }
            }
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT, [this](uint64_t userdata) {
                RefreshRows();
            });
        };
    };

    for (size_t i = 0; i < comp->outputs.size(); ++i)
    {
        auto& ob = comp->outputs[i];
        Row& row = rows.emplace_back();

        row.remove.Create("");
        row.remove.SetText("X");
        row.remove.SetSize(XMFLOAT2(row.remove.GetSize().y, row.remove.GetSize().y));
        row.remove.SetTooltip("Remove this output binding");
        row.remove.OnClick(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            if (i < c.outputs.size())
                c.outputs.erase(c.outputs.begin() + i);
        }));
        AddWidget(&row.remove);

        row.evt.Create("");
        row.evt.SetTooltip("Event name (eg. OnStart)");
        row.evt.SetText(ob.event);
        row.evt.OnInputAccepted(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            c.outputs[i].event = args.sValue;
        }));
        AddWidget(&row.evt);

        row.target.Create("");
        row.target.SetTooltip("Target entity name or id");
        row.target.SetText(ob.target);
        row.target.OnInputAccepted(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            c.outputs[i].target = args.sValue;
        }));
        AddWidget(&row.target);

        row.input.Create("");
        row.input.SetTooltip("Input to invoke on target (eg. Remove)");
        row.input.SetText(ob.input);
        row.input.OnInputAccepted(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            c.outputs[i].input = args.sValue;
        }));
        AddWidget(&row.input);

        row.param.Create("");
        row.param.SetTooltip("Optional parameter");
        row.param.SetText(ob.parameter);
        row.param.OnInputAccepted(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            c.outputs[i].parameter = args.sValue;
        }));
        AddWidget(&row.param);

        row.delay.Create("");
        row.delay.SetTooltip("Delay (seconds)");
        row.delay.SetValue(ob.delay);
        row.delay.OnInputAccepted(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            c.outputs[i].delay = args.fValue;
        }));
        AddWidget(&row.delay);

        row.once.Create("");
        row.once.SetText("once");
        row.once.SetCheck(ob.once);
        row.once.OnClick(forEachSelectedWithRefresh(i, [i](EntityOutputsComponent& c, auto args) {
            c.outputs[i].once = args.bValue;
        }));
        AddWidget(&row.once);
    }

    editor->generalWnd.RefreshTheme();
}

void EntityOutputsWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();
    layout.margin_left = 8;

    layout.add_fullwidth(addButton);
    layout.jump();

    const float h = addButton.GetSize().y;
    const float pad = layout.padding;

    for (auto& row : rows)
    {
        float x = layout.padding;
        float y = layout.y;

        // Remove button
        row.remove.SetPos(XMFLOAT2(x, y));
        row.remove.SetSize(XMFLOAT2(h, h));
        x += row.remove.GetSize().x + pad;

        // Event
        row.evt.SetPos(XMFLOAT2(x, y));
        row.evt.SetSize(XMFLOAT2(100, h));
        x += row.evt.GetSize().x + pad;

        // Target
        row.target.SetPos(XMFLOAT2(x, y));
        row.target.SetSize(XMFLOAT2(120, h));
        x += row.target.GetSize().x + pad;

        // Input
        row.input.SetPos(XMFLOAT2(x, y));
        row.input.SetSize(XMFLOAT2(110, h));
        x += row.input.GetSize().x + pad;

        // Param
        row.param.SetPos(XMFLOAT2(x, y));
        row.param.SetSize(XMFLOAT2(120, h));
        x += row.param.GetSize().x + pad;

        // Delay
        row.delay.SetPos(XMFLOAT2(x, y));
        row.delay.SetSize(XMFLOAT2(70, h));
        x += row.delay.GetSize().x + pad;

        // Once
        row.once.SetPos(XMFLOAT2(x, y));
        row.once.SetSize(XMFLOAT2(60, h));

        layout.y += h;
        layout.y += pad;
        layout.y += row.evt.GetShadowRadius();
    }
}
