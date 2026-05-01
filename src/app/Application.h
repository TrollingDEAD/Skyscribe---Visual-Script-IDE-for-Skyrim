#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <string>

// Forward declarations
namespace ui { class MainWindow; }

namespace app {

class Application {
public:
    static Application& Get();

    // Initialise Win32 window, D3D11, ImGui. Returns false on failure.
    bool Init();

    // Enter the main message loop. Returns when the window is closed.
    void Run();

    // Release all resources and shut down ImGui.
    void Shutdown();

    HWND GetHWND() const { return hwnd_; }
    ID3D11Device* GetDevice() const { return device_; }

private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool CreateAppWindow();
    bool CreateDevice();
    void CleanupDevice();
    void CreateRenderTarget();
    void CleanupRenderTarget();

    // Returns %APPDATA%\Skyscribe as a UTF-8 string (directory created if absent).
    std::string GetAppDataDir() const;
    std::string GetIniPath() const;

    // Win32 message callback.
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND                     hwnd_               = nullptr;
    ID3D11Device*            device_             = nullptr;
    ID3D11DeviceContext*     device_context_     = nullptr;
    IDXGISwapChain*          swap_chain_         = nullptr;
    ID3D11RenderTargetView*  render_target_view_ = nullptr;

    bool running_ = false;
};

} // namespace app
