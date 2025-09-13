#pragma once
#include "WickedEngine.h"
#include "wiConfig.h"

class Game_Application;
class Game_Renderer : public wi::RenderPath3D
{
	
public:
	//Game_Application* main = nullptr;
	void Load() override;
	void Update(float dt) override;
	void ResizeLayout() override;
	void Render() const override;
};

class Game_Application : public wi::Application
{
	

public:
	Game_Renderer renderer;
	wi::config::File config;

	~Game_Application() override;
	void Initialize() override;
	void Update(float dt) override;
	void Compose(wi::graphics::CommandList cmd) override;
};
