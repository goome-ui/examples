#define GOOME_IMPL_WIN32_IMPLEMENTATION
#define GOOME_IMPL_DX11_IMPLEMENTATION
#include "goome_dx11.h"
#include "goome_win32.h"

#include "mod.h"

#include <d3d11.h>
#include <dxgi.h>

static const GUID guid_device = {0xdb6f6ddb, 0xac77, 0x4e88, {0x82, 0x53, 0x81, 0x9d, 0xf9, 0xbb, 0xf1, 0x40}};
static const GUID guid_tex2d = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};

typedef HRESULT(WINAPI *present_t)(IDXGISwapChain *, UINT, UINT);
static present_t real_present;
static int ready;

static HRESULT WINAPI my_present(IDXGISwapChain *sc, UINT sync, UINT flags)
{
    ID3D11Device *dev = NULL;
    if (SUCCEEDED(sc->lpVtbl->GetDevice(sc, &guid_device, (void **)&dev)) && dev) {
        ID3D11DeviceContext *ctx = NULL;
        DXGI_SWAP_CHAIN_DESC desc;
        dev->lpVtbl->GetImmediateContext(dev, &ctx);
        if (ctx && SUCCEEDED(sc->lpVtbl->GetDesc(sc, &desc)) && goome_mod_ensure((int)desc.BufferDesc.Width, (int)desc.BufferDesc.Height)) {
            goome_mod_subclass(desc.OutputWindow);
            if (!ready) ready = goome_dx11_init(gmod.g, dev, ctx) == GOOME_OK;
            ID3D11Texture2D *bb = NULL;
            if (ready && SUCCEEDED(sc->lpVtbl->GetBuffer(sc, 0, &guid_tex2d, (void **)&bb)) && bb) {
                ID3D11RenderTargetView *rtv = NULL;
                if (SUCCEEDED(dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv))) {
                    ID3D11RenderTargetView *old_rtv = NULL;
                    ID3D11DepthStencilView *old_ds = NULL;
                    ctx->lpVtbl->OMGetRenderTargets(ctx, 1, &old_rtv, &old_ds);
                    ctx->lpVtbl->OMSetRenderTargets(ctx, 1, &rtv, NULL);
                    goome_mod_frame("D3D11");
                    goome_dx11_render(gmod.g);
                    ctx->lpVtbl->OMSetRenderTargets(ctx, 1, &old_rtv, old_ds);
                    if (old_rtv) old_rtv->lpVtbl->Release(old_rtv);
                    if (old_ds) old_ds->lpVtbl->Release(old_ds);
                    rtv->lpVtbl->Release(rtv);
                }
                bb->lpVtbl->Release(bb);
            }
        }
        if (ctx) ctx->lpVtbl->Release(ctx);
        dev->lpVtbl->Release(dev);
    }
    return real_present(sc, sync, flags);
}

static void *find_present(void)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc), wc.lpfnWndProc = DefWindowProcW, wc.hInstance = GetModuleHandleW(NULL), wc.lpszClassName = L"goome_dx11_dummy";
    RegisterClassExW(&wc);
    HWND hw = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, NULL, NULL, wc.hInstance, NULL);
    DXGI_SWAP_CHAIN_DESC sd;
    memset(&sd, 0, sizeof(sd));
    sd.BufferCount = 1, sd.BufferDesc.Width = 16, sd.BufferDesc.Height = 16, sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, sd.OutputWindow = hw, sd.SampleDesc.Count = 1, sd.Windowed = TRUE, sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, NULL, &ctx);
    if (FAILED(hr)) hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, NULL, &ctx);
    void *present = NULL;
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)sc;
        present = vt[8];
    }
    if (sc) sc->lpVtbl->Release(sc);
    if (ctx) ctx->lpVtbl->Release(ctx);
    if (dev) dev->lpVtbl->Release(dev);
    if (hw) DestroyWindow(hw);
    return present;
}

int goome_mod_start(void)
{
    void *present = find_present();
    if (!present || MH_Initialize() != MH_OK) return 0;
    if (MH_CreateHook(present, (void *)my_present, (void **)&real_present) != MH_OK) return 0;
    MH_EnableHook(present);
    return 1;
}

void goome_mod_stop(void)
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (ready) goome_dx11_shutdown(gmod.g);
    goome_mod_teardown();
}
