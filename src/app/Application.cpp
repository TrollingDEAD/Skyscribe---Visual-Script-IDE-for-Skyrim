#include "app/Application.h"
#include "app/Logger.h"
#include "ui/MainWindow.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <shlobj.h>
#include <filesystem>
#include <stdexcept>

// Forward-declare ImGui's Win32 message handler.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace app {

// ── Singleton ─────────────────────────────────────────────────────────────────

Application& Application::Get() {
    static Application instance;
    return instance;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool Application::Init() {
    // Ensure %APPDATA%\Skyscribe\ exists.
    std::filesystem::create_directories(GetAppDataDir());

    // Logger must come up before anything else so we can record failures.
    Logger::Get().Init(GetAppDataDir() + "\\skyscribe.log");
    LOG_INFO("Skyscribe starting up");

    if (!CreateAppWindow()) {
        LOG_ERR("Failed to create Win32 window");
        return false;
    }

    if (!CreateDevice()) {
        LOG_ERR("Failed to create D3D11 device");
        return false;
    }

    // ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // We manage imgui.ini manually.

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, device_context_);

    // Load persisted layout (no-op if file absent — default layout built on first frame).
    const std::string ini_path = GetIniPath();
    if (std::filesystem::exists(ini_path)) {
        ImGui::LoadIniSettingsFromDisk(ini_path.c_str());
        ui::MainWindow::Get().SetLayoutAlreadyLoaded(true);
        LOG_INFO("ImGui layout loaded from " + ini_path);
    } else {
        LOG_INFO("No imgui.ini found — default layout will be built on first frame");
    }

    running_ = true;
    LOG_INFO("Initialisation complete");
    return true;
}

void Application::Run() {
    while (running_) {
        // ── Win32 message pump ───────────────────────────────────────────────
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                running_ = false;
        }
        if (!running_) break;

        // ── ImGui frame ──────────────────────────────────────────────────────
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ui::MainWindow::Get().Render();

        // ── Compose & present ────────────────────────────────────────────────
        ImGui::Render();

        constexpr float kClearColor[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
        device_context_->OMSetRenderTargets(1, &render_target_view_, nullptr);
        device_context_->ClearRenderTargetView(render_target_view_, kClearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        swap_chain_->Present(1, 0); // VSync on
    }
}

void Application::Shutdown() {
    // Save layout before teardown.
    ImGui::SaveIniSettingsToDisk(GetIniPath().c_str());
    LOG_INFO("ImGui layout saved");

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDevice();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClass(L"SkyscribeWnd", GetModuleHandle(nullptr));

    Logger::Get().Flush();
    LOG_INFO("Shutdown complete");
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool Application::CreateAppWindow() {
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SkyscribeWnd";
    if (!RegisterClassEx(&wc))
        return false;

    // Create window; store 'this' in GWLP_USERDATA for WndProc access.
    hwnd_ = CreateWindowEx(
        0, L"SkyscribeWnd", L"Skyscribe",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr, nullptr,
        GetModuleHandle(nullptr), this);

    if (!hwnd_) return false;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);
    return true;
}

bool Application::CreateDevice() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd_;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL kFeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL feature_level_out;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, kFeatureLevels, 2,
        D3D11_SDK_VERSION, &sd,
        &swap_chain_, &device_, &feature_level_out, &device_context_);

    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void Application::CreateRenderTarget() {
    ID3D11Texture2D* back_buffer = nullptr;
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (back_buffer) {
        device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_view_);
        back_buffer->Release();
    }
}

void Application::CleanupRenderTarget() {
    if (render_target_view_) {
        render_target_view_->Release();
        render_target_view_ = nullptr;
    }
}

void Application::CleanupDevice() {
    CleanupRenderTarget();
    if (swap_chain_)    { swap_chain_->Release();    swap_chain_    = nullptr; }
    if (device_context_){ device_context_->Release(); device_context_ = nullptr; }
    if (device_)        { device_->Release();         device_         = nullptr; }
}

std::string Application::GetAppDataDir() const {
    PWSTR path_w = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path_w))) {
        // Convert wide path to UTF-8.
        int len = WideCharToMultiByte(CP_UTF8, 0, path_w, -1, nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<size_t>(len > 0 ? len - 1 : 0), '\0');
        if (len > 0)
            WideCharToMultiByte(CP_UTF8, 0, path_w, -1, s.data(), len, nullptr, nullptr);
        CoTaskMemFree(path_w);
        return s + "\\Skyscribe";
    }
    return "."; // fallback: current directory
}

std::string Application::GetIniPath() const {
    return GetAppDataDir() + "\\imgui.ini";
}

// ── Win32 message handler ─────────────────────────────────────────────────────

LRESULT WINAPI Application::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Let ImGui process input first.
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // Store 'this' on WM_CREATE so subsequent messages can reach the instance.
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }

    auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_SIZE:
        if (app && app->device_ && wParam != SIZE_MINIMIZED) {
            app->CleanupRenderTarget();
            app->swap_chain_->ResizeBuffers(
                0, LOWORD(lParam), HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            app->CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        // Suppress the beep on Alt+key.
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

} // namespace app
