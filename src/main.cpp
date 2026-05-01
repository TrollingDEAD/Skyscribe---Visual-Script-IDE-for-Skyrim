#include <Windows.h>

#include "app/Application.h"
#include "app/Logger.h"

int WINAPI WinMain(
    _In_     HINSTANCE /*hInstance*/,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_     LPSTR     /*lpCmdLine*/,
    _In_     int       /*nCmdShow*/)
{
    auto& app = app::Application::Get();

    if (!app.Init()) {
        MessageBoxW(nullptr,
            L"Skyscribe failed to initialise.\n"
            L"Check %APPDATA%\\Skyscribe\\skyscribe.log for details.",
            L"Skyscribe — Fatal Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
