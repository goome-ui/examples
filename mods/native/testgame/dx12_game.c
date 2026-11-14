#include "common.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <math.h>

static const GUID iid_dev = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
static const GUID iid_fac = {0x1bc6ea02, 0xef36, 0x464f, {0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a}};
static const GUID iid_queue = {0x0ec870a6, 0x5d7e, 0x4c22, {0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed}};
static const GUID iid_sc3 = {0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};
static const GUID iid_heap = {0x8efb471d, 0x616c, 0x4f49, {0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51}};
static const GUID iid_res = {0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
static const GUID iid_alloc = {0x6102dee4, 0xaf59, 0x4b09, {0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24}};
static const GUID iid_list = {0x5b160d0f, 0xac1b, 0x4185, {0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55}};
static const GUID iid_root = {0xc54a6b66, 0x72df, 0x4ee8, {0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14}};
static const GUID iid_pso = {0x765a30f3, 0xf624, 0x4c6f, {0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45}};
static const GUID iid_fence = {0x0a753dcf, 0xc4d8, 0x4b91, {0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76}};

enum { NBUF = 2 };
static ID3D12Device *dev;
static ID3D12CommandQueue *queue;
static IDXGISwapChain3 *sc;
static ID3D12DescriptorHeap *rtvh;
static ID3D12Resource *bufs[NBUF], *readbuf, *vbuf;
static ID3D12CommandAllocator *alloc;
static ID3D12GraphicsCommandList *cl;
static ID3D12RootSignature *root;
static ID3D12PipelineState *pso;
static ID3D12Fence *fence;
static HANDLE ev;
static UINT64 fv;
static UINT rtv_size;
static float spin;

static const char *hlsl =
    "cbuffer Cb : register(b0) { float4 rot; }\n"
    "struct VO { float4 p : SV_POSITION; float3 c : COLOR; };\n"
    "VO vs(float2 p : POS, float3 c : COL) {\n"
    "  VO o; float2 r = float2(p.x*rot.x - p.y*rot.y, p.x*rot.y + p.y*rot.x);\n"
    "  o.p = float4(r*0.4 + float2(0.45, -0.45), 0, 1); o.c = c; return o; }\n"
    "float4 ps(VO i) : SV_Target { return float4(i.c, 1); }\n";

static void sync(void)
{
    UINT64 v = ++fv;
    queue->lpVtbl->Signal(queue, fence, v);
    if (fence->lpVtbl->GetCompletedValue(fence) < v) {
        fence->lpVtbl->SetEventOnCompletion(fence, v, ev);
        WaitForSingleObject(ev, 2000);
    }
}

static D3D12_CPU_DESCRIPTOR_HANDLE rtv_at(UINT i)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h;
    rtvh->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtvh, &h);
    h.ptr += (size_t)i * rtv_size;
    return h;
}

static void barrier(ID3D12Resource *r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
{
    D3D12_RESOURCE_BARRIER br;
    memset(&br, 0, sizeof(br));
    br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, br.Transition.pResource = r;
    br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, br.Transition.StateBefore = a, br.Transition.StateAfter = b;
    cl->lpVtbl->ResourceBarrier(cl, 1, &br);
}

static void frame(void)
{
    spin += 0.04f;
    float rot[4] = {cosf(spin), sinf(spin), 0, 0};
    UINT idx = sc->lpVtbl->GetCurrentBackBufferIndex(sc);
    alloc->lpVtbl->Reset(alloc);
    cl->lpVtbl->Reset(cl, alloc, pso);
    barrier(bufs[idx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_at(idx);
    float clear[4] = {tg_clear[0] / 255.0f, tg_clear[1] / 255.0f, tg_clear[2] / 255.0f, 1};
    cl->lpVtbl->OMSetRenderTargets(cl, 1, &rtv, FALSE, NULL);
    cl->lpVtbl->ClearRenderTargetView(cl, rtv, clear, 0, NULL);
    D3D12_VIEWPORT vp = {0, 0, TG_W, TG_H, 0, 1};
    D3D12_RECT rc = {0, 0, TG_W, TG_H};
    cl->lpVtbl->RSSetViewports(cl, 1, &vp);
    cl->lpVtbl->RSSetScissorRects(cl, 1, &rc);
    cl->lpVtbl->SetGraphicsRootSignature(cl, root);
    cl->lpVtbl->SetGraphicsRoot32BitConstants(cl, 0, 4, rot, 0);
    D3D12_VERTEX_BUFFER_VIEW vbv = {vbuf->lpVtbl->GetGPUVirtualAddress(vbuf), 60, 20};
    cl->lpVtbl->IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl->lpVtbl->IASetVertexBuffers(cl, 0, 1, &vbv);
    cl->lpVtbl->DrawInstanced(cl, 3, 1, 0, 0);
    barrier(bufs[idx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cl->lpVtbl->Close(cl);
    queue->lpVtbl->ExecuteCommandLists(queue, 1, (ID3D12CommandList *const *)&cl);
    sc->lpVtbl->Present(sc, 0, 0);
    sync();
}

static int readback(unsigned char *rgba)
{
    UINT idx = sc->lpVtbl->GetCurrentBackBufferIndex(sc) ^ 1;
    UINT pitch = (TG_W * 4 + 255) & ~255u;
    alloc->lpVtbl->Reset(alloc);
    cl->lpVtbl->Reset(cl, alloc, NULL);
    barrier(bufs[idx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst, src;
    memset(&dst, 0, sizeof(dst));
    memset(&src, 0, sizeof(src));
    dst.pResource = readbuf, dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM, dst.PlacedFootprint.Footprint.Width = TG_W;
    dst.PlacedFootprint.Footprint.Height = TG_H, dst.PlacedFootprint.Footprint.Depth = 1, dst.PlacedFootprint.Footprint.RowPitch = pitch;
    src.pResource = bufs[idx], src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    cl->lpVtbl->CopyTextureRegion(cl, &dst, 0, 0, 0, &src, NULL);
    barrier(bufs[idx], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
    cl->lpVtbl->Close(cl);
    queue->lpVtbl->ExecuteCommandLists(queue, 1, (ID3D12CommandList *const *)&cl);
    sync();
    void *p = NULL;
    D3D12_RANGE rr = {0, (size_t)pitch * TG_H};
    if (FAILED(readbuf->lpVtbl->Map(readbuf, 0, &rr, &p)) || !p) return 0;
    for (int y = 0; y < TG_H; y++) {
        const unsigned char *row = (const unsigned char *)p + (size_t)y * pitch;
        for (int x = 0; x < TG_W; x++) {
            unsigned char *o = rgba + ((size_t)y * TG_W + x) * 4;
            o[0] = row[x * 4 + 2], o[1] = row[x * 4 + 1], o[2] = row[x * 4 + 0], o[3] = 255;
        }
    }
    D3D12_RANGE none = {0, 0};
    readbuf->lpVtbl->Unmap(readbuf, 0, &none);
    return 1;
}

static ID3D12Resource *buffer(D3D12_HEAP_TYPE heap, UINT64 size, D3D12_RESOURCE_STATES state)
{
    D3D12_HEAP_PROPERTIES hp;
    memset(&hp, 0, sizeof(hp));
    hp.Type = heap;
    D3D12_RESOURCE_DESC rd;
    memset(&rd, 0, sizeof(rd));
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, rd.Width = size, rd.Height = 1, rd.DepthOrArraySize = 1, rd.MipLevels = 1;
    rd.SampleDesc.Count = 1, rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *r = NULL;
    dev->lpVtbl->CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, state, NULL, &iid_res, (void **)&r);
    return r;
}

static int make_pipeline(void)
{
    D3D12_ROOT_PARAMETER rp;
    memset(&rp, 0, sizeof(rp));
    rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rp.Constants.Num32BitValues = 4;
    D3D12_ROOT_SIGNATURE_DESC rs;
    memset(&rs, 0, sizeof(rs));
    rs.NumParameters = 1, rs.pParameters = &rp, rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob *blob = NULL, *err = NULL, *vb = NULL, *pb = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) return 0;
    dev->lpVtbl->CreateRootSignature(dev, 0, blob->lpVtbl->GetBufferPointer(blob), blob->lpVtbl->GetBufferSize(blob), &iid_root, (void **)&root);
    blob->lpVtbl->Release(blob);
    if (FAILED(D3DCompile(hlsl, strlen(hlsl), NULL, NULL, NULL, "vs", "vs_5_0", 0, 0, &vb, &err)) ||
        FAILED(D3DCompile(hlsl, strlen(hlsl), NULL, NULL, NULL, "ps", "ps_5_0", 0, 0, &pb, &err)))
        return 0;
    D3D12_INPUT_ELEMENT_DESC el[2] = {
        {"POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd;
    memset(&pd, 0, sizeof(pd));
    pd.pRootSignature = root, pd.InputLayout.pInputElementDescs = el, pd.InputLayout.NumElements = 2;
    pd.VS.pShaderBytecode = vb->lpVtbl->GetBufferPointer(vb), pd.VS.BytecodeLength = vb->lpVtbl->GetBufferSize(vb);
    pd.PS.pShaderBytecode = pb->lpVtbl->GetBufferPointer(pb), pd.PS.BytecodeLength = pb->lpVtbl->GetBufferSize(pb);
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID, pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = 0xffffffff, pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1, pd.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM, pd.SampleDesc.Count = 1;
    HRESULT hr = dev->lpVtbl->CreateGraphicsPipelineState(dev, &pd, &iid_pso, (void **)&pso);
    vb->lpVtbl->Release(vb), pb->lpVtbl->Release(pb);
    if (FAILED(hr)) return 0;
    float verts[] = {0, 1, 1, 0.3f, 0.2f, -1, -1, 0.2f, 1, 0.3f, 1, -1, 0.3f, 0.4f, 1};
    vbuf = buffer(D3D12_HEAP_TYPE_UPLOAD, sizeof(verts), D3D12_RESOURCE_STATE_GENERIC_READ);
    void *p = NULL;
    D3D12_RANGE none = {0, 0};
    if (!vbuf || FAILED(vbuf->lpVtbl->Map(vbuf, 0, &none, &p))) return 0;
    memcpy(p, verts, sizeof(verts));
    vbuf->lpVtbl->Unmap(vbuf, 0, NULL);
    return root && pso;
}

int main(void)
{
    HWND win = tg_make_window(L"goome_tg_dx12");
    if (!win) return printf("no window\n"), 1;
    IDXGIFactory4 *fac = NULL;
    if (FAILED(CreateDXGIFactory1(&iid_fac, (void **)&fac))) return printf("no factory\n"), 1;
    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &iid_dev, (void **)&dev))) {
        IDXGIAdapter *warp = NULL;
        if (FAILED(fac->lpVtbl->EnumWarpAdapter(fac, &iid_dev, (void **)&warp)) ||
            FAILED(D3D12CreateDevice((IUnknown *)warp, D3D_FEATURE_LEVEL_11_0, &iid_dev, (void **)&dev)))
            return printf("no d3d12 device\n"), 1;
    }
    D3D12_COMMAND_QUEUE_DESC qd;
    memset(&qd, 0, sizeof(qd));
    dev->lpVtbl->CreateCommandQueue(dev, &qd, &iid_queue, (void **)&queue);
    DXGI_SWAP_CHAIN_DESC1 sd;
    memset(&sd, 0, sizeof(sd));
    sd.BufferCount = NBUF, sd.Width = TG_W, sd.Height = TG_H, sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, sd.SampleDesc.Count = 1, sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *sc1 = NULL;
    if (FAILED(fac->lpVtbl->CreateSwapChainForHwnd(fac, (IUnknown *)queue, win, &sd, NULL, NULL, &sc1)) || !sc1) return printf("no swapchain\n"), 1;
    sc1->lpVtbl->QueryInterface(sc1, &iid_sc3, (void **)&sc);
    D3D12_DESCRIPTOR_HEAP_DESC hd;
    memset(&hd, 0, sizeof(hd));
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, hd.NumDescriptors = NBUF;
    dev->lpVtbl->CreateDescriptorHeap(dev, &hd, &iid_heap, (void **)&rtvh);
    rtv_size = dev->lpVtbl->GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < NBUF; i++) {
        sc->lpVtbl->GetBuffer(sc, i, &iid_res, (void **)&bufs[i]);
        dev->lpVtbl->CreateRenderTargetView(dev, bufs[i], NULL, rtv_at(i));
    }
    readbuf = buffer(D3D12_HEAP_TYPE_READBACK, (UINT64)(((TG_W * 4 + 255) & ~255u)) * TG_H, D3D12_RESOURCE_STATE_COPY_DEST);
    dev->lpVtbl->CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &iid_alloc, (void **)&alloc);
    dev->lpVtbl->CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, NULL, &iid_list, (void **)&cl);
    cl->lpVtbl->Close(cl);
    dev->lpVtbl->CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &iid_fence, (void **)&fence);
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!make_pipeline()) return printf("no pipeline\n"), 1;
    int fails = tg_run("D3D12", frame, readback);
    return fails != 0;
}
