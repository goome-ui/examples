#include <d3d12.h>
#include <dxgi1_4.h>

#ifndef GOOME_HAVE_SERIALIZE_ROOT_SIG
#define GOOME_HAVE_SERIALIZE_ROOT_SIG
typedef HRESULT(WINAPI *PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)(const D3D12_ROOT_SIGNATURE_DESC *, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob **, ID3DBlob **);
#endif

#define GOOME_IMPL_WIN32_IMPLEMENTATION
#define GOOME_IMPL_DX12_IMPLEMENTATION
#include "goome_dx12.h"
#include "goome_win32.h"

#include "mod.h"

static const GUID g_dev_iid = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
static const GUID g_fac2_iid = {0x1bc6ea02, 0xef36, 0x464f, {0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a}};
static const GUID g_sc3_iid = {0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};
static const GUID g_res_iid = {0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
static const GUID g_heap_iid = {0x8efb471d, 0x616c, 0x4f49, {0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51}};
static const GUID g_alloc_iid = {0x6102dee4, 0xaf59, 0x4b09, {0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24}};
static const GUID g_list_iid = {0x5b160d0f, 0xac1b, 0x4185, {0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55}};
static const GUID g_fence_iid = {0x0a753dcf, 0xc4d8, 0x4b91, {0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76}};

typedef HRESULT(WINAPI *present_t)(IDXGISwapChain *, UINT, UINT);
typedef void(WINAPI *exec_t)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *);
static present_t real_present;
static exec_t real_exec;

static ID3D12CommandQueue *queue;
static ID3D12Device *dev;
static ID3D12DescriptorHeap *srv_heap, *rtv_heap;
static ID3D12CommandAllocator *allocs[8];
static ID3D12GraphicsCommandList *cl;
static ID3D12Fence *fence;
static UINT64 fence_val, done[8];
static HANDLE fence_ev;
static UINT rtv_size, nframes;
static int ready;

static void WINAPI my_exec(ID3D12CommandQueue *q, UINT n, ID3D12CommandList *const *lists)
{
    if (!queue && n) {
        D3D12_COMMAND_QUEUE_DESC qd;
        q->lpVtbl->GetDesc(q, &qd);
        if (qd.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) queue = q;
    }
    real_exec(q, n, lists);
}

static int setup(DXGI_FORMAT fmt, UINT n)
{
    nframes = n < 1 ? 1 : n > 8 ? 8 : n;
    D3D12_DESCRIPTOR_HEAP_DESC hd;
    memset(&hd, 0, sizeof(hd));
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, hd.NumDescriptors = 1, hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(dev->lpVtbl->CreateDescriptorHeap(dev, &hd, &g_heap_iid, (void **)&srv_heap))) return 0;
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, hd.NumDescriptors = 1, hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(dev->lpVtbl->CreateDescriptorHeap(dev, &hd, &g_heap_iid, (void **)&rtv_heap))) return 0;
    rtv_size = dev->lpVtbl->GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < nframes; i++)
        if (FAILED(dev->lpVtbl->CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &g_alloc_iid, (void **)&allocs[i]))) return 0;
    if (FAILED(dev->lpVtbl->CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocs[0], NULL, &g_list_iid, (void **)&cl))) return 0;
    cl->lpVtbl->Close(cl);
    if (FAILED(dev->lpVtbl->CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &g_fence_iid, (void **)&fence))) return 0;
    fence_ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    D3D12_CPU_DESCRIPTOR_HANDLE scpu;
    D3D12_GPU_DESCRIPTOR_HANDLE sgpu;
    srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(srv_heap, &scpu);
    srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(srv_heap, &sgpu);
    return goome_dx12_init(gmod.g, dev, (int)nframes, fmt, srv_heap, scpu, sgpu) == GOOME_OK;
}

static void wait_slot(UINT i)
{
    if (fence->lpVtbl->GetCompletedValue(fence) < done[i] && fence_ev) {
        fence->lpVtbl->SetEventOnCompletion(fence, done[i], fence_ev);
        WaitForSingleObject(fence_ev, 1000);
    }
}

static HRESULT WINAPI my_present(IDXGISwapChain *sc, UINT sync, UINT flags)
{
    DXGI_SWAP_CHAIN_DESC desc;
    IDXGISwapChain3 *sc3 = NULL;
    if (queue && SUCCEEDED(sc->lpVtbl->GetDesc(sc, &desc)) && goome_mod_ensure((int)desc.BufferDesc.Width, (int)desc.BufferDesc.Height)) {
        goome_mod_subclass(desc.OutputWindow);
        if (!dev) queue->lpVtbl->GetDevice(queue, &g_dev_iid, (void **)&dev);
        if (dev && !ready) ready = setup(desc.BufferDesc.Format, desc.BufferCount);
        sc->lpVtbl->QueryInterface(sc, &g_sc3_iid, (void **)&sc3);
        UINT idx = sc3 ? sc3->lpVtbl->GetCurrentBackBufferIndex(sc3) : (UINT)(fence_val % nframes);
        ID3D12Resource *bb = NULL;
        if (ready && idx < nframes && SUCCEEDED(sc->lpVtbl->GetBuffer(sc, idx, &g_res_iid, (void **)&bb)) && bb) {
            D3D12_CPU_DESCRIPTOR_HANDLE rtv;
            rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtv_heap, &rtv);
            dev->lpVtbl->CreateRenderTargetView(dev, bb, NULL, rtv);
            wait_slot(idx);
            allocs[idx]->lpVtbl->Reset(allocs[idx]);
            cl->lpVtbl->Reset(cl, allocs[idx], NULL);
            D3D12_RESOURCE_BARRIER b;
            memset(&b, 0, sizeof(b));
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, b.Transition.pResource = bb;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT, b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cl->lpVtbl->ResourceBarrier(cl, 1, &b);
            cl->lpVtbl->OMSetRenderTargets(cl, 1, &rtv, FALSE, NULL);
            goome_mod_frame("D3D12");
            goome_dx12_render(gmod.g, cl);
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET, b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            cl->lpVtbl->ResourceBarrier(cl, 1, &b);
            cl->lpVtbl->Close(cl);
            real_exec(queue, 1, (ID3D12CommandList *const *)&cl);
            queue->lpVtbl->Signal(queue, fence, ++fence_val);
            done[idx] = fence_val;
            bb->lpVtbl->Release(bb);
        }
        if (sc3) sc3->lpVtbl->Release(sc3);
    }
    return real_present(sc, sync, flags);
}

static int find_hooks(void **present, void **exec)
{
    IDXGIFactory4 *fac = NULL;
    if (FAILED(CreateDXGIFactory1(&g_fac2_iid, (void **)&fac)) || !fac) return 0;
    ID3D12Device *dd = NULL;
    int ok = SUCCEEDED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &g_dev_iid, (void **)&dd));
    if (!ok) {
        IDXGIAdapter *warp = NULL;
        if (SUCCEEDED(fac->lpVtbl->EnumWarpAdapter(fac, &g_dev_iid, (void **)&warp)) && warp) {
            ok = SUCCEEDED(D3D12CreateDevice((IUnknown *)warp, D3D_FEATURE_LEVEL_11_0, &g_dev_iid, (void **)&dd));
            warp->lpVtbl->Release(warp);
        }
    }
    if (!ok || !dd) {
        fac->lpVtbl->Release(fac);
        return 0;
    }
    D3D12_COMMAND_QUEUE_DESC qd;
    memset(&qd, 0, sizeof(qd));
    ID3D12CommandQueue *q = NULL;
    dd->lpVtbl->CreateCommandQueue(dd, &qd, &(GUID){0x0ec870a6, 0x5d7e, 0x4c22, {0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed}}, (void **)&q);
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc), wc.lpfnWndProc = DefWindowProcW, wc.hInstance = GetModuleHandleW(NULL), wc.lpszClassName = L"goome_dx12_dummy";
    RegisterClassExW(&wc);
    HWND hw = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, NULL, NULL, wc.hInstance, NULL);
    DXGI_SWAP_CHAIN_DESC1 sd;
    memset(&sd, 0, sizeof(sd));
    sd.BufferCount = 2, sd.Width = 16, sd.Height = 16, sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, sd.SampleDesc.Count = 1, sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *sc = NULL;
    if (q) fac->lpVtbl->CreateSwapChainForHwnd(fac, (IUnknown *)q, hw, &sd, NULL, NULL, &sc);
    if (sc) *present = (*(void ***)sc)[8];
    if (q) *exec = (*(void ***)q)[10];
    int got = sc && q;
    if (sc) sc->lpVtbl->Release(sc);
    if (q) q->lpVtbl->Release(q);
    if (hw) DestroyWindow(hw);
    dd->lpVtbl->Release(dd);
    fac->lpVtbl->Release(fac);
    return got;
}

int goome_mod_start(void)
{
    void *present = NULL, *exec = NULL;
    if (!find_hooks(&present, &exec) || MH_Initialize() != MH_OK) return 0;
    if (MH_CreateHook(exec, (void *)my_exec, (void **)&real_exec) != MH_OK) return 0;
    if (MH_CreateHook(present, (void *)my_present, (void **)&real_present) != MH_OK) return 0;
    MH_EnableHook(MH_ALL_HOOKS);
    return 1;
}

void goome_mod_stop(void)
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (fence) {
        for (UINT i = 0; i < nframes; i++) wait_slot(i);
    }
    if (ready) goome_dx12_shutdown(gmod.g);
    if (cl) cl->lpVtbl->Release(cl);
    for (UINT i = 0; i < nframes; i++)
        if (allocs[i]) allocs[i]->lpVtbl->Release(allocs[i]);
    if (srv_heap) srv_heap->lpVtbl->Release(srv_heap);
    if (rtv_heap) rtv_heap->lpVtbl->Release(rtv_heap);
    if (fence) fence->lpVtbl->Release(fence);
    if (fence_ev) CloseHandle(fence_ev);
    if (dev) dev->lpVtbl->Release(dev);
    goome_mod_teardown();
}
