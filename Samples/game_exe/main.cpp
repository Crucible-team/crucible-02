#include "GameApplication.h"

Game_Application application;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	wi::arguments::Parse(lpCmdLine);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	// Win32 window and message loop setup:
	static auto WndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
	{
		switch (message)
		{
		case WM_SIZE:
		case WM_DPICHANGED:
			if (application.is_window_active)
				application.SetWindow(hWnd);
			break;
		case WM_CHAR:
			switch (wParam)
			{
			case VK_BACK:
				wi::gui::TextInputField::DeleteFromInput();
				break;
			case VK_RETURN:
				break;
			default:
			{
				const wchar_t c = (const wchar_t)wParam;
				wi::gui::TextInputField::AddInput(c);
			}
			break;
			}
			break;
		case WM_INPUT:
			wi::input::rawinput::ParseMessage((void*)lParam);
			break;
		case WM_KILLFOCUS:
			application.is_window_active = false;
			break;
		case WM_SETFOCUS:
			application.is_window_active = true;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	};
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"WickedEngineApplicationTemplate";
	wcex.hIconSm = NULL;
	RegisterClassExW(&wcex);
	/*HWND hWnd = CreateWindowW(wcex.lpszClassName, wcex.lpszClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	ShowWindow(hWnd, SW_SHOWDEFAULT);

	// set Win32 window to engine:
	application.SetWindow(hWnd);

	// process command line string:
	wi::arguments::Parse(lpCmdLine);*/

	// just show some basic info:
	application.infoDisplay.active = false;
	application.infoDisplay.watermark = false;
	application.infoDisplay.resolution = false;
	application.infoDisplay.fpsinfo = false;

	int width = CW_USEDEFAULT;
	int height = 0;
	bool fullscreen = false;
	bool borderless = false;

	wi::Timer timer;
	if (application.config.Open("user_config.ini"))
	{
		if (application.config.Has("width"))
		{
			width = application.config.GetInt("width");
			height = application.config.GetInt("height");

			if (width <= 0)
			{
				width = 100;
			}
			if (height <= 0)
			{
				height = 100;
			}
		}
		fullscreen = application.config.GetBool("fullscreen");
		borderless = application.config.GetBool("borderless");
		application.allow_hdr = application.config.GetBool("allow_hdr");

		wilog("user_config.ini loaded in %.2f milliseconds\n", (float)timer.elapsed_milliseconds());
	}

	HWND hWnd = NULL;

	if (borderless || fullscreen)
	{
		width = std::max(100, width);
		height = std::max(100, height);

		hWnd = CreateWindowEx(
			WS_EX_APPWINDOW,
			wcex.lpszClassName,
			wcex.lpszClassName,
			WS_POPUP,
			CW_USEDEFAULT, 0, width, height,
			NULL,
			NULL,
			hInstance,
			NULL
		);
	}
	else
	{
		hWnd = CreateWindow(
			wcex.lpszClassName,
			wcex.lpszClassName,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, width, height,
			NULL,
			NULL,
			hInstance,
			NULL
		);
	}
	if (hWnd == NULL)
	{
		wilog_error("Win32 window creation failure!");
		return -1;
	}
	if (fullscreen)
	{
		HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &info);
		width = info.rcMonitor.right - info.rcMonitor.left;
		height = info.rcMonitor.bottom - info.rcMonitor.top;
		MoveWindow(hWnd, 0, 0, width, height, FALSE);
	}
	SendMessage(hWnd, WM_SETTINGCHANGE, 0, 0); // trigger dark mode theme detection
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	application.SetWindow(hWnd);

	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

			application.Run();

		}
	}

	wi::jobsystem::ShutDown();

	return (int)msg.wParam;
}
