#include "common.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <math.h>

static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;
static IDXGISwapChain *sc;
static ID3D11RenderTargetView *rtv;
static ID3D11VertexShader *vs;
static ID3D11PixelShader *ps;
static ID3D11InputLayout *layout;
static ID3D11Buffer *vbuf, *cbuf;
static ID3D11Texture2D *staging;
static float spin;

static const char *hlsl =
    "cbuffer Cb : register(b0) { float4 rot; }\n"
    "struct VO { float4 p : SV_POSITION; float3 c : COLOR; };\n"
    "VO vs(float2 p : POS, float3 c : COL) {\n"
    "  VO o; float2 r = float2(p.x*rot.x - p.y*rot.y, p.x*rot.y + p.y*rot.x);\n"
    "  o.p = float4(r*0.4 + float2(0.45, -0.45), 0, 1); o.c = c; return o; }\n"
    "float4 ps(VO i) : SV_Target { return float4(i.c, 1); }\n";

static int make_pipeline(void)
{
    ID3DBlob *vb = NULL, *pb = NULL, *err = NULL;
    if (FAILED(D3DCompile(hlsl, strlen(hlsl), NULL, NULL, NULL, "vs", "vs_4_0", 0, 0, &vb, &err)) ||
        FAILED(D3DCompile(hlsl, strlen(hlsl), NULL, NULL, NULL, "ps", "ps_4_0", 0, 0, &pb, &err)))
        return 0;
    dev->lpVtbl->CreateVertexShader(dev, vb->lpVtbl->GetBufferPointer(vb), vb->lpVtbl->GetBufferSize(vb), NULL, &vs);
    dev->lpVtbl->CreatePixelShader(dev, pb->lpVtbl->GetBufferPointer(pb), pb->lpVtbl->GetBufferSize(pb), NULL, &ps);
    D3D11_INPUT_ELEMENT_DESC el[2] = {
        {"POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    dev->lpVtbl->CreateInputLayout(dev, el, 2, vb->lpVtbl->GetBufferPointer(vb), vb->lpVtbl->GetBufferSize(vb), &layout);
    vb->lpVtbl->Release(vb), pb->lpVtbl->Release(pb);
    float verts[] = {0, 1, 1, 0.3f, 0.2f, -1, -1, 0.2f, 1, 0.3f, 1, -1, 0.3f, 0.4f, 1};
    D3D11_BUFFER_DESC bd = {sizeof(verts), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA sr = {verts, 0, 0};
    dev->lpVtbl->CreateBuffer(dev, &bd, &sr, &vbuf);
    D3D11_BUFFER_DESC cd = {16, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
    dev->lpVtbl->CreateBuffer(dev, &cd, NULL, &cbuf);
    return vs && ps && layout && vbuf && cbuf;
}

static void frame(void)
{
    spin += 0.04f;
    float c[4] = {cosf(spin), sinf(spin), 0, 0};
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx->lpVtbl->Map(ctx, (ID3D11Resource *)cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, c, sizeof(c));
        ctx->lpVtbl->Unmap(ctx, (ID3D11Resource *)cbuf, 0);
    }
    float clear[4] = {tg_clear[0] / 255.0f, tg_clear[1] / 255.0f, tg_clear[2] / 255.0f, 1};
    D3D11_VIEWPORT vp = {0, 0, TG_W, TG_H, 0, 1};
    UINT stride = 20, off = 0;
    ctx->lpVtbl->OMSetRenderTargets(ctx, 1, &rtv, NULL);
    ctx->lpVtbl->ClearRenderTargetView(ctx, rtv, clear);
    ctx->lpVtbl->RSSetViewports(ctx, 1, &vp);
    ctx->lpVtbl->IASetInputLayout(ctx, layout);
    ctx->lpVtbl->IASetVertexBuffers(ctx, 0, 1, &vbuf, &stride, &off);
    ctx->lpVtbl->IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->lpVtbl->VSSetShader(ctx, vs, NULL, 0);
    ctx->lpVtbl->VSSetConstantBuffers(ctx, 0, 1, &cbuf);
    ctx->lpVtbl->PSSetShader(ctx, ps, NULL, 0);
    ctx->lpVtbl->Draw(ctx, 3, 0);
    sc->lpVtbl->Present(sc, 0, 0);
}

static int readback(unsigned char *rgba)
{
    ID3D11Texture2D *bb = NULL;
    static const GUID tex_iid = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};
    if (FAILED(sc->lpVtbl->GetBuffer(sc, 0, &tex_iid, (void **)&bb)) || !bb) return 0;
    ctx->lpVtbl->CopyResource(ctx, (ID3D11Resource *)staging, (ID3D11Resource *)bb);
    bb->lpVtbl->Release(bb);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->lpVtbl->Map(ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &m))) return 0;
    for (int y = 0; y < TG_H; y++) {
        const unsigned char *row = (const unsigned char *)m.pData + (size_t)y * m.RowPitch;
        for (int x = 0; x < TG_W; x++) {
            unsigned char *o = rgba + ((size_t)y * TG_W + x) * 4;
            o[0] = row[x * 4 + 2], o[1] = row[x * 4 + 1], o[2] = row[x * 4 + 0], o[3] = row[x * 4 + 3];
        }
    }
    ctx->lpVtbl->Unmap(ctx, (ID3D11Resource *)staging, 0);
    return 1;
}

int main(void)
{
    HWND win = tg_make_window(L"goome_tg_dx11");
    if (!win) return printf("no window\n"), 1;
    DXGI_SWAP_CHAIN_DESC sd;
    memset(&sd, 0, sizeof(sd));
    sd.BufferCount = 1, sd.BufferDesc.Width = TG_W, sd.BufferDesc.Height = TG_H, sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, sd.OutputWindow = win, sd.SampleDesc.Count = 1, sd.Windowed = TRUE, sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_DRIVER_TYPE types[2] = {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP};
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 2 && FAILED(hr); i++)
        hr = D3D11CreateDeviceAndSwapChain(NULL, types[i], NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, NULL, &ctx);
    if (FAILED(hr)) return printf("no d3d11 device\n"), 1;
    ID3D11Texture2D *bb = NULL;
    static const GUID tex_iid = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};
    sc->lpVtbl->GetBuffer(sc, 0, &tex_iid, (void **)&bb);
    dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
    D3D11_TEXTURE2D_DESC td;
    memset(&td, 0, sizeof(td));
    td.Width = TG_W, td.Height = TG_H, td.MipLevels = 1, td.ArraySize = 1, td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1, td.Usage = D3D11_USAGE_STAGING, td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    dev->lpVtbl->CreateTexture2D(dev, &td, NULL, &staging);
    bb->lpVtbl->Release(bb);
    if (!make_pipeline()) return printf("no pipeline\n"), 1;
    int fails = tg_run("D3D11", frame, readback);
    return fails != 0;
}
