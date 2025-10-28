module;

#include <stdafx.h>
#include <dxsdk/dx8/d3d8.h>

export module D3DDrv;

import ComVars;

void* FMVPtrY = nullptr;
void* FMVPtrX = nullptr;
D3DPRESENT_PARAMETERS* pPresentParams = nullptr;
SafetyHookInline shUD3DRenderDeviceSetRes = {};
int __fastcall UD3DRenderDeviceSetRes(void* UD3DRenderDevice, void* edx, void* UViewport, int width, int height, int a5)
{
    bool bInvalidRes = true;
    std::wstring resStr = std::to_wstring(width) + L"x" + std::to_wstring(height);
    for (const auto& it : ResList)
    {
        if (resStr == it.first)
        {
            swscanf(it.second.c_str(), L"%dx%d", &width, &height);
            bInvalidRes = false;
            break;
        }
    }

    if (bInvalidRes)
    {
        width = Screen.Width;
        height = Screen.Height;
        bInvalidRes = false;
    }

    auto ret = shUD3DRenderDeviceSetRes.unsafe_fastcall<int>(UD3DRenderDevice, edx, UViewport, width, height, a5);

    Screen.Width = width;
    Screen.Height = height;
    Screen.fWidth = static_cast<float>(Screen.Width);
    Screen.fHeight = static_cast<float>(Screen.Height);
    Screen.fAspectRatio = (Screen.fWidth / Screen.fHeight);
    Screen.fHudOffset = (Screen.fWidth - Screen.fHeight * (4.0f / 3.0f)) / 2.0f;
    Screen.HUDScaleX = 1.0f / Screen.fWidth * (Screen.fHeight / 480.0f);
    Screen.fFMVoffsetStartX = (Screen.fWidth - 640.0f) / 2.0f;
    Screen.fFMVoffsetStartY = (Screen.fHeight - 480.0f) / 2.0f;
    Screen.fWidescreenHudOffset = ((Screen.fHeight * (4.0f / 3.0f)) / (640.0f / Screen.fIniHudOffset));
    if (Screen.fAspectRatio < (16.0f / 9.0f))
        Screen.fWidescreenHudOffset = Screen.fWidescreenHudOffset / (((16.0f / 9.0f) / (Screen.fAspectRatio)) * 1.5f);
    if (Screen.fAspectRatio <= (4.0f / 3.0f))
        Screen.fWidescreenHudOffset = 0.0f;
    if (Screen.Width < 640 || Screen.Height < 480)
        return ret;

    injector::WriteMemory(FMVPtrY, Screen.Height, true);
    injector::WriteMemory(FMVPtrX, Screen.Width, true);

    CIniReader iniReader("");
    auto [DesktopResW, DesktopResH] = GetDesktopRes();
    if (Screen.Width != DesktopResW || Screen.Height != DesktopResH)
    {
        iniReader.WriteInteger("MAIN", "ResX", Screen.Width);
        iniReader.WriteInteger("MAIN", "ResY", Screen.Height);
    }
    else
    {
        iniReader.WriteInteger("MAIN", "ResX", 0);
        iniReader.WriteInteger("MAIN", "ResY", 0);
    }

    if (pPresentParams->Windowed)
    {
        tagRECT rect;
        rect.left = (LONG)(((float)DesktopResW / 2.0f) - (Screen.fWidth / 2.0f));
        rect.top = (LONG)(((float)DesktopResH / 2.0f) - (Screen.fHeight / 2.0f));
        rect.right = (LONG)Screen.Width;
        rect.bottom = (LONG)Screen.Height;
        SetWindowLong(pPresentParams->hDeviceWindow, GWL_STYLE, GetWindowLong(pPresentParams->hDeviceWindow, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(pPresentParams->hDeviceWindow, NULL, rect.left, rect.top, rect.right, rect.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
        SetForegroundWindow(pPresentParams->hDeviceWindow);
        SetCursor(NULL);
    }

    return ret;
}

namespace UD3DRenderDevice
{
    SafetyHookInline shDisplayVideo = {};
    void __fastcall DisplayVideo(void* UD3DRenderDevice, void* edx, void* UCanvas, void* a3)
    {
        shDisplayVideo.unsafe_fastcall(UD3DRenderDevice, edx, UCanvas, a3);

        IDirect3DDevice8* pD3DDevice = *(IDirect3DDevice8**)((uintptr_t)UD3DRenderDevice + 0x4684);

        if (pD3DDevice)
        {
            IDirect3DSurface8* pBackBuffer = nullptr;

            if (SUCCEEDED(pD3DDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
            {
                D3DSURFACE_DESC desc;
                if (SUCCEEDED(pBackBuffer->GetDesc(&desc)))
                {
                    int W = desc.Width;
                    int H = desc.Height;

                    // Create a temporary surface to copy the back buffer
                    IDirect3DSurface8* pTemp = nullptr;
                    if (SUCCEEDED(pD3DDevice->CreateImageSurface(W, H, desc.Format, &pTemp)))
                    {
                        // Copy the entire back buffer to temp using CopyRects
                        POINT pt = { 0, 0 };
                        pD3DDevice->CopyRects(pBackBuffer, nullptr, 0, pTemp, &pt);

                        // Clear the back buffer to black
                        pD3DDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

                        // Create a texture from the temp surface
                        IDirect3DTexture8* pTexture = nullptr;
                        if (SUCCEEDED(pD3DDevice->CreateTexture(W, H, 1, 0, desc.Format, D3DPOOL_MANAGED, &pTexture)))
                        {
                            IDirect3DSurface8* pTexSurf = nullptr;
                            if (SUCCEEDED(pTexture->GetSurfaceLevel(0, &pTexSurf)))
                            {
                                // Copy temp to texture surface using CopyRects (corrected direction)
                                pD3DDevice->CopyRects(pTemp, nullptr, 0, pTexSurf, &pt);
                                pTexSurf->Release();

                                // Set up render states for 2D drawing
                                pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
                                pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                                pD3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                                pD3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                                pD3DDevice->SetTexture(0, pTexture);

                                // Add for smoother stretching
                                pD3DDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                                pD3DDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

                                // 640x480 content is at 0,0
                                int srcX = 0;
                                int srcY = 0;
                                int videoWidth = 640;
                                int videoHeight = 480;

                                // Define vertices for a quad (TL, TR, BL, BR)
                                struct Vertex
                                {
                                    float x, y, z, rhw;
                                    float u, v;
                                };

                                if (Screen.nFMVWidescreenMode)
                                {
                                    const int barPixels = 60;
                                    int innerHeight = videoHeight - 2 * barPixels;
                                    float contentAspect = (float)videoWidth / (float)innerHeight;
                                    float screenAspect = (float)Screen.Width / (float)Screen.Height;

                                    float targetW, targetH, targetX, targetY;
                                    if (screenAspect > contentAspect)
                                    {
                                        // Pillarbox: fit to height
                                        targetH = (float)Screen.Height;
                                        targetW = targetH * contentAspect;
                                        targetX = ((float)Screen.Width - targetW) / 2.0f;
                                        targetY = 0.0f;
                                    }
                                    else
                                    {
                                        // Letterbox: fit to width
                                        targetW = (float)Screen.Width;
                                        targetH = targetW / contentAspect;
                                        targetX = 0.0f;
                                        targetY = ((float)Screen.Height - targetH) / 2.0f;
                                    }

                                    // UVs for cropped inner content
                                    float uLeft = (float)srcX / (float)W;
                                    float uRight = (float)(srcX + videoWidth) / (float)W;
                                    float vTop = (float)(srcY + barPixels) / (float)H;
                                    float vBottom = (float)(srcY + barPixels + innerHeight) / (float)H;

                                    Vertex vertices[4] = {
                                        { targetX, targetY, 0.0f, 1.0f, uLeft, vTop },
                                        { targetX + targetW, targetY, 0.0f, 1.0f, uRight, vTop },
                                        { targetX, targetY + targetH, 0.0f, 1.0f, uLeft, vBottom },
                                        { targetX + targetW, targetY + targetH, 0.0f, 1.0f, uRight, vBottom }
                                    };

                                    // Draw the quad
                                    pD3DDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
                                    pD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
                                }
                                else // Original pillarbox without cropping (includes bars)
                                {
                                    // Pillarboxing
                                    int targetW = Screen.Height * videoWidth / videoHeight;
                                    int targetH = Screen.Height;
                                    int targetX = (Screen.Width - targetW) / 2;
                                    int targetY = 0;

                                    float uLeft = (float)srcX / (float)W;
                                    float uRight = (float)(srcX + videoWidth) / (float)W;
                                    float vTop = (float)srcY / (float)H;
                                    float vBottom = (float)(srcY + videoHeight) / (float)H;

                                    Vertex vertices[4] = {
                                        { (float)targetX, (float)targetY, 0.0f, 1.0f, uLeft, vTop },
                                        { (float)(targetX + targetW), (float)targetY, 0.0f, 1.0f, uRight, vTop },
                                        { (float)targetX, (float)(targetY + targetH), 0.0f, 1.0f, uLeft, vBottom },
                                        { (float)(targetX + targetW), (float)(targetY + targetH), 0.0f, 1.0f, uRight, vBottom }
                                    };

                                    // Draw the quad
                                    pD3DDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
                                    pD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
                                }
                            }
                            pTexture->Release();
                        }
                        pTemp->Release();
                    }
                }
                pBackBuffer->Release();
            }
        }
    }
}

export void InitD3DDrv()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "68 ? ? ? ? 68 ? ? ? ? 50 8B CF FF 55 60"); //0x1000F72E
    FMVPtrY = pattern.get_first(1);
    FMVPtrX = pattern.get_first(6);

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 44 24 04 81 EC 80 00 00 00");
    shUD3DRenderDeviceSetRes = safetyhook::create_inline(pattern.get_first(), UD3DRenderDeviceSetRes);
    pPresentParams = *hook::module_pattern(GetModuleHandle(L"D3DDrv"), "BF ? ? ? ? 33 C0 8B D9 C1 E9 02 83 E3 03").get_first<D3DPRESENT_PARAMETERS*>(1);

    //FMV
    static auto SetFMVPos = [](injector::reg_pack& regs)
    {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        //if (Screen.Width >= 1280 && Screen.Height >= 960)
        //{
        //    static constexpr auto BINKCOPY2XWH = 0x40000000L; // copy the width and height zoomed by two
        //    regs.eax |= BINKCOPY2XWH;
        //
        //    *(int32_t*)&regs.ebp = static_cast<int32_t>((Screen.fWidth - (640.0f * 2.0f)) / 2.0f);
        //    *(int32_t*)&regs.ebx = static_cast<int32_t>((Screen.fHeight - (480.0f * 2.0f)) / 2.0f);
        //}
        //else
        {
            //*(int32_t*)&regs.ebp = static_cast<int32_t>((Screen.fWidth - 640.0f) / 2.0f);
            //*(int32_t*)&regs.ebx = static_cast<int32_t>((Screen.fHeight - 480.0f) / 2.0f);

            regs.ebp = 0;
            regs.ebx = 0;
        }
    };

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 4C 24 3C 8B 54 24 28");
    struct BINKHook1
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.ecx = *(uint32_t*)(regs.esp + 0x3C);
            regs.edx = *(uint32_t*)(regs.esp + 0x28);
            SetFMVPos(regs);
        }
    }; injector::MakeInline<BINKHook1>(pattern.get_first(0), pattern.get_first(8));

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 54 24 3C 8B 4C 24 2C");
    struct BINKHook2
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edx = *(uint32_t*)(regs.esp + 0x3C);
            regs.ecx = *(uint32_t*)(regs.esp + 0x2C);
            SetFMVPos(regs);
        }
    }; injector::MakeInline<BINKHook2>(pattern.get_first(0), pattern.get_first(8));

    UD3DRenderDevice::shDisplayVideo = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"D3DDrv"), "?DisplayVideo@UD3DRenderDevice@@UAEXPAVUCanvas@@PAX@Z"), UD3DRenderDevice::DisplayVideo);
}
