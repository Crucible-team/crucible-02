#pragma once
class EditorComponent;

class EntityOutputsWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);

    EditorComponent* editor = nullptr;
    wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
    void SetEntity(wi::ecs::Entity entity);

    wi::gui::Button addButton;

    struct Row
    {
        wi::gui::Button remove;
        wi::gui::TextInputField evt;
        wi::gui::TextInputField target;
        wi::gui::TextInputField input;
        wi::gui::TextInputField param;
        wi::gui::TextInputField delay;
        wi::gui::TextInputField refire;
    };
    std::deque<Row> rows;

    void RefreshRows();

    void ResizeLayout() override;
};

