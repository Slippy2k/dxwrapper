/**
* Copyright (C) 2018 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*
* Code taken from: https://github.com/strangebytes/diablo-ddrawwrapper
*/

#include "ddraw.h"

struct handle_data
{
	DWORD process_id = 0;
	HWND best_handle = nullptr;
};

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK EnumProcWindowCallback(HWND hwnd, LPARAM lParam);

std::unordered_map<HWND, m_IDirectDrawX*> g_hookmap;

/************************/
/*** IUnknown methods ***/
/************************/

HRESULT m_IDirectDrawX::QueryInterface(REFIID riid, LPVOID FAR * ppvObj)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		if ((riid == IID_IDirectDraw || riid == IID_IDirectDraw2 || riid == IID_IDirectDraw3 || riid == IID_IDirectDraw4 || riid == IID_IDirectDraw7 || riid == IID_IUnknown) && ppvObj)
		{
			if (riid == WrapperID || riid == IID_IUnknown)
			{
				AddRef();
				*ppvObj = this;
				return S_OK;
			}

			d3d9Object->AddRef();

			if (riid == IID_IDirectDraw)
			{
				*ppvObj = new m_IDirectDraw((IDirectDraw *)d3d9Object);
			}
			else if (riid == IID_IDirectDraw2)
			{
				*ppvObj = new m_IDirectDraw2((IDirectDraw2 *)d3d9Object);
			}
			else if (riid == IID_IDirectDraw3)
			{
				*ppvObj = new m_IDirectDraw3((IDirectDraw3 *)d3d9Object);
			}
			else if (riid == IID_IDirectDraw4)
			{
				*ppvObj = new m_IDirectDraw4((IDirectDraw4 *)d3d9Object);
			}
			else
			{
				*ppvObj = new m_IDirectDraw7((IDirectDraw7 *)d3d9Object);
			}

			// Success
			return S_OK;
		}
		if ((riid == IID_IDirect3D || riid == IID_IDirect3D2 || riid == IID_IDirect3D3 || riid == IID_IDirect3D7) && ppvObj)
		{
			if (riid == IID_IDirect3D)
			{
				*ppvObj = new m_IDirect3D((IDirect3D *)this);
			}
			else if (riid == IID_IDirect3D2)
			{
				*ppvObj = new m_IDirect3D2((IDirect3D2 *)this);
			}
			else if (riid == IID_IDirect3D3)
			{
				*ppvObj = new m_IDirect3D3((IDirect3D3 *)this);
			}
			else
			{
				*ppvObj = new m_IDirect3D7((IDirect3D7 *)this);
			}

			// Success
			return S_OK;
		}
	}

	return ProxyQueryInterface(ProxyInterface, riid, ppvObj, WrapperID, WrapperInterface);
}

ULONG m_IDirectDrawX::AddRef()
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		return InterlockedIncrement(&RefCount);
	}

	return ProxyInterface->AddRef();
}

ULONG m_IDirectDrawX::Release()
{
	Logging::LogDebug() << __FUNCTION__;

	ULONG ref;

	if (Config.Dd7to9)
	{
		ref = InterlockedDecrement(&RefCount);
	}
	else
	{
		ref = ProxyInterface->Release();
	}

	if (ref == 0)
	{
		if (Config.Dd7to9)
		{
			// ToDo: Release all m_Direct3DX objects.
			ReleaseD3d9();
		}

		if (WrapperInterface)
		{
			WrapperInterface->DeleteMe();
		}
		else
		{
			delete this;
		}
	}

	return ref;
}

/***************************/
/*** IDirectDraw methods ***/
/***************************/

HRESULT m_IDirectDrawX::Compact()
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// This method is not currently implemented even in ddraw.
		return DD_OK;
	}

	return ProxyInterface->Compact();
}

HRESULT m_IDirectDrawX::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR * lplpDDClipper, IUnknown FAR * pUnkOuter)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		if (!lplpDDClipper)
		{
			return DDERR_INVALIDPARAMS;
		}

		*lplpDDClipper = new m_IDirectDrawClipper(nullptr);

		return DD_OK;
	}

	HRESULT hr = ProxyInterface->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);

	if (SUCCEEDED(hr) && lplpDDClipper)
	{
		*lplpDDClipper = ProxyAddressLookupTable.FindAddress<m_IDirectDrawClipper>(*lplpDDClipper);
	}

	return hr;
}

HRESULT m_IDirectDrawX::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE FAR * lplpDDPalette, IUnknown FAR * pUnkOuter)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		if (!lplpDDPalette || !lpDDColorArray)
		{
			return DDERR_INVALIDPARAMS;
		}

		*lplpDDPalette = new m_IDirectDrawPalette(dwFlags, lpDDColorArray);

		return DD_OK;
	}

	HRESULT hr = ProxyInterface->CreatePalette(dwFlags, lpDDColorArray, lplpDDPalette, pUnkOuter);

	if (SUCCEEDED(hr) && lplpDDPalette)
	{
		*lplpDDPalette = ProxyAddressLookupTable.FindAddress<m_IDirectDrawPalette>(*lplpDDPalette);
	}

	return hr;
}

HRESULT m_IDirectDrawX::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE7 FAR * lplpDDSurface, IUnknown FAR * pUnkOuter)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lplpDDSurface || !lpDDSurfaceDesc)
	{
		return DDERR_INVALIDPARAMS;
	}

	// Game using old DirectX, Convert to LPDDSURFACEDESC2
	if (ConvertSurfaceDescTo2)
	{
		DDSURFACEDESC2 Desc2;
		ConvertSurfaceDesc(Desc2, *lpDDSurfaceDesc);

		// BackBufferCount must be at least 1
		if ((lpDDSurfaceDesc->dwFlags & DDSD_BACKBUFFERCOUNT) != 0 && lpDDSurfaceDesc->dwBackBufferCount == 0)
		{
			lpDDSurfaceDesc->dwBackBufferCount = 1;
		}

		return CreateSurface2(&Desc2, lplpDDSurface, pUnkOuter);
	}

	HRESULT hr = ((IDirectDraw3*)ProxyInterface)->CreateSurface(lpDDSurfaceDesc, (LPDIRECTDRAWSURFACE*)lplpDDSurface, pUnkOuter);

	if (SUCCEEDED(hr))
	{
		*lplpDDSurface = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lplpDDSurface, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirectDrawX::CreateSurface2(LPDDSURFACEDESC2 lpDDSurfaceDesc2, LPDIRECTDRAWSURFACE7 FAR * lplpDDSurface, IUnknown FAR * pUnkOuter)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lplpDDSurface || !lpDDSurfaceDesc2)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (Config.Dd7to9)
	{
		if ((lpDDSurfaceDesc2->dwFlags & DDSD_CAPS) == 0)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Create the requested d3d device for this display mode, report error on failure
		if (!d3d9Device && isWindowed)
		{
			if (FAILED(CreateD3DDevice()))
			{
				Logging::Log() << __FUNCTION__ << " Error creating Direct3D9 Device";
				return DDERR_GENERIC;
			}
		}

		*lplpDDSurface = new m_IDirectDrawSurfaceX(&d3d9Device, this, DirectXVersion, lpDDSurfaceDesc2, displayWidth, displayHeight);

		return DD_OK;
	}

	HRESULT hr = ProxyInterface->CreateSurface(lpDDSurfaceDesc2, lplpDDSurface, pUnkOuter);

	if (SUCCEEDED(hr))
	{
		*lplpDDSurface = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lplpDDSurface, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirectDrawX::DuplicateSurface(LPDIRECTDRAWSURFACE7 lpDDSurface, LPDIRECTDRAWSURFACE7 FAR * lplpDupDDSurface)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		m_IDirectDrawSurfaceX *lpDDSurfaceX = (m_IDirectDrawSurfaceX*)lpDDSurface;
		if (!DoesSurfaceExist(lpDDSurfaceX))
		{
			return DDERR_INVALIDPARAMS;
		}

		DDSURFACEDESC2 DDSurfaceDesc2;
		lpDDSurfaceX->GetSurfaceDesc2(&DDSurfaceDesc2);
		DDSurfaceDesc2.ddsCaps.dwCaps &= ~DDSCAPS_PRIMARYSURFACE;		// Remove Primary surface flag

		*lplpDupDDSurface = new m_IDirectDrawSurfaceX(&d3d9Device, this, DirectXVersion, &DDSurfaceDesc2, displayWidth, displayHeight);

		return DD_OK;
	}

	if (lpDDSurface)
	{
		lpDDSurface = static_cast<m_IDirectDrawSurface7 *>(lpDDSurface)->GetProxyInterface();
	}

	HRESULT hr = ProxyInterface->DuplicateSurface(lpDDSurface, lplpDupDDSurface);

	if (SUCCEEDED(hr) && lplpDupDDSurface && lpDDSurface)
	{
		*lplpDupDDSurface = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lplpDupDDSurface, ((m_IDirectDrawSurface7 *)lpDDSurface)->GetDirectXVersion());
	}

	return hr;
}

HRESULT m_IDirectDrawX::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback)
{
	Logging::LogDebug() << __FUNCTION__;

	// Game using old DirectX, Convert to LPDDSURFACEDESC2
	if (ConvertSurfaceDescTo2)
	{
		DDSURFACEDESC2 Desc2;
		if (lpDDSurfaceDesc)
		{
			ConvertSurfaceDesc(Desc2, *lpDDSurfaceDesc);
		}

		return EnumDisplayModes2(dwFlags, (lpDDSurfaceDesc) ? &Desc2 : nullptr, lpContext, (LPDDENUMMODESCALLBACK2)lpEnumModesCallback);
	}

	return ((IDirectDraw3*)ProxyInterface)->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
}

HRESULT m_IDirectDrawX::EnumDisplayModes2(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc2, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback2)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// Save refresh rate
		DWORD EnumRefreshModes = 0;
		if (lpDDSurfaceDesc2 && (dwFlags & DDEDM_REFRESHRATES) != 0)
		{
			EnumRefreshModes = lpDDSurfaceDesc2->dwRefreshRate > 0;
		}

		// Get display modes to enum
		bool DisplayAllModes = (!lpDDSurfaceDesc2);
		DWORD DisplayBitCount = displayModeBPP;
		if (!DisplayAllModes && lpDDSurfaceDesc2 && (lpDDSurfaceDesc2->dwFlags & DDSD_PIXELFORMAT) != 0)
		{
			DisplayBitCount = GetBitCount(lpDDSurfaceDesc2->ddpfPixelFormat);
		}

		// Setup callback
		LPDDENUMMODESCALLBACK2 p_EnumModesCallback2 = lpEnumModesCallback2;
		LPVOID p_Context = lpContext;
		ENUMDISPLAYMODES CallbackContext;

		// Game using old DirectX, Convert to LPDDSURFACEDESC2
		if (ConvertSurfaceDescTo2)
		{
			p_Context = &CallbackContext;
			CallbackContext.lpContext = lpContext;
			CallbackContext.lpCallback = (LPDDENUMMODESCALLBACK)lpEnumModesCallback2;
			p_EnumModesCallback2 = m_IDirectDrawEnumDisplayModes::ConvertCallback;
		}

		// Setup surface desc
		DDSURFACEDESC2 Desc2 = { NULL };

		// Setup display mode and format
		D3DFORMAT Format;
		D3DDISPLAYMODE d3ddispmode;

		// Loop through each bit count
		for (int x : {32, 16, 8})
		{
			// Set display bit count
			if (DisplayAllModes || DisplayBitCount == 0)
			{
				DisplayBitCount = x;
			}

			// Set static adapter format
			switch (DisplayBitCount)
			{
			case 8:
				Format = D3DFMT_X8R8G8B8;
				break;
			case 16:
				Format = D3DFMT_R5G6B5;
				break;
			case 32:
			default:
				Format = D3DFMT_X8R8G8B8;
				break;
			}

			// Enumerate modes for format XRGB
			UINT modeCount = d3d9Object->GetAdapterModeCount(D3DADAPTER_DEFAULT, Format);

			// Loop through all modes
			for (UINT i = 0; i < modeCount; i++)
			{
				// Get display modes
				ZeroMemory(&d3ddispmode, sizeof(D3DDISPLAYMODE));
				HRESULT hr = d3d9Object->EnumAdapterModes(D3DADAPTER_DEFAULT, Format, i, &d3ddispmode);
				if (FAILED(hr))
				{
					Logging::Log() << __FUNCTION__ << " EnumAdapterModes failed";
					return hr;
				}

				// Check refresh mode
				if (EnumRefreshModes == 0 || d3ddispmode.RefreshRate == EnumRefreshModes)
				{
					// Set surface desc options
					Desc2.dwSize = sizeof(DDSURFACEDESC2);
					Desc2.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_REFRESHRATE | DDSD_PITCH | DDSD_PIXELFORMAT;
					Desc2.dwWidth = d3ddispmode.Width;
					Desc2.dwHeight = d3ddispmode.Height;
					Desc2.dwRefreshRate = d3ddispmode.RefreshRate;

					// Set adapter pixel format
					Desc2.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
					GetPixelDisplayFormat(Format, Desc2.ddpfPixelFormat);

					// Special handling for 8-bit mode
					if (DisplayBitCount == 8)
					{
						Desc2.ddpfPixelFormat.dwRGBBitCount = DisplayBitCount;
						Desc2.ddpfPixelFormat.dwRBitMask = 0;
						Desc2.ddpfPixelFormat.dwGBitMask = 0;
						Desc2.ddpfPixelFormat.dwBBitMask = 0;
					}
					Desc2.lPitch = (Desc2.ddpfPixelFormat.dwRGBBitCount / 8) * Desc2.dwWidth;

					if (p_EnumModesCallback2(&Desc2, p_Context) != DDENUMRET_OK)
					{
						return DD_OK;
					}
				}
			}

			// Exit if not displaying all modes
			if (!DisplayAllModes)
			{
				return DD_OK;
			}
		}

		return DD_OK;
	}

	// Game using old DirectX, Convert to LPDDSURFACEDESC2
	if (ConvertSurfaceDescTo2)
	{
		ENUMDISPLAYMODES CallbackContext;
		CallbackContext.lpContext = lpContext;
		CallbackContext.lpCallback = (LPDDENUMMODESCALLBACK)lpEnumModesCallback2;

		return ProxyInterface->EnumDisplayModes(dwFlags, lpDDSurfaceDesc2, &CallbackContext, m_IDirectDrawEnumDisplayModes::ConvertCallback);
	}

	return ProxyInterface->EnumDisplayModes(dwFlags, lpDDSurfaceDesc2, lpContext, lpEnumModesCallback2);
}

HRESULT m_IDirectDrawX::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback)
{
	Logging::LogDebug() << __FUNCTION__;

	// Game using old DirectX, Convert to LPDDSURFACEDESC2
	if (ConvertSurfaceDescTo2)
	{
		DDSURFACEDESC2 Desc2;
		if (lpDDSurfaceDesc)
		{
			ConvertSurfaceDesc(Desc2, *lpDDSurfaceDesc);
		}

		return EnumSurfaces2(dwFlags, (lpDDSurfaceDesc) ? &Desc2 : nullptr, lpContext, (LPDDENUMSURFACESCALLBACK7)lpEnumSurfacesCallback);
	}

	ENUMSURFACE CallbackContext;
	CallbackContext.lpContext = lpContext;
	CallbackContext.lpCallback = lpEnumSurfacesCallback;
	CallbackContext.DirectXVersion = DirectXVersion;
	CallbackContext.ConvertSurfaceDescTo2 = ConvertSurfaceDescTo2;

	return ((IDirectDraw3*)ProxyInterface)->EnumSurfaces(dwFlags, lpDDSurfaceDesc, &CallbackContext, m_IDirectDrawEnumSurface::ConvertCallback);
}

HRESULT m_IDirectDrawX::EnumSurfaces2(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc2, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback7)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	ENUMSURFACE CallbackContext;
	CallbackContext.lpContext = lpContext;
	CallbackContext.lpCallback7 = lpEnumSurfacesCallback7;
	CallbackContext.DirectXVersion = DirectXVersion;
	CallbackContext.ConvertSurfaceDescTo2 = ConvertSurfaceDescTo2;

	return ProxyInterface->EnumSurfaces(dwFlags, lpDDSurfaceDesc2, &CallbackContext, m_IDirectDrawEnumSurface::ConvertCallback2);
}

HRESULT m_IDirectDrawX::FlipToGDISurface()
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	return ProxyInterface->FlipToGDISurface();
}

HRESULT m_IDirectDrawX::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
{
	Logging::LogDebug() << __FUNCTION__;

	DDCAPS DriverCaps, HELCaps;
	DriverCaps.dwSize = sizeof(DriverCaps);
	HELCaps.dwSize = sizeof(HELCaps);

	HRESULT hr;

	if (Config.Dd7to9)
	{
		D3DCAPS9 Caps9;
		hr = d3d9Object->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &Caps9);

		ConvertCaps(DriverCaps, Caps9);
		ConvertCaps(HELCaps, Caps9);
	}
	else
	{
		hr = ProxyInterface->GetCaps((lpDDDriverCaps) ? &DriverCaps : nullptr, (lpDDHELCaps) ? &HELCaps : nullptr);
	}

	if (SUCCEEDED(hr))
	{
		if (lpDDDriverCaps)
		{
			ConvertCaps(*lpDDDriverCaps, DriverCaps);
		}
		if (lpDDHELCaps)
		{
			ConvertCaps(*lpDDHELCaps, HELCaps);
		}
	}

	return hr;
}

HRESULT m_IDirectDrawX::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpDDSurfaceDesc)
	{
		return DDERR_INVALIDPARAMS;
	}

	// Game using old DirectX, Convert to LPDDSURFACEDESC2
	if (ConvertSurfaceDescTo2)
	{
		DDSURFACEDESC2 Desc2;
		ConvertSurfaceDesc(Desc2, *lpDDSurfaceDesc);

		HRESULT hr = GetDisplayMode2(&Desc2);

		// Convert back to LPDDSURFACEDESC
		if (SUCCEEDED(hr))
		{
			ConvertSurfaceDesc(*lpDDSurfaceDesc, Desc2);
		}

		return hr;
	}

	return ((IDirectDraw3*)ProxyInterface)->GetDisplayMode(lpDDSurfaceDesc);
}

HRESULT m_IDirectDrawX::GetDisplayMode2(LPDDSURFACEDESC2 lpDDSurfaceDesc2)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpDDSurfaceDesc2)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (Config.Dd7to9)
	{
		// Set Surface Desc
		lpDDSurfaceDesc2->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_REFRESHRATE | DDSD_PIXELFORMAT;
		DWORD displayModeBits = displayModeBPP;
		if (displayModeBits)
		{
			lpDDSurfaceDesc2->dwWidth = displayModeWidth;
			lpDDSurfaceDesc2->dwHeight = displayModeHeight;
			lpDDSurfaceDesc2->dwRefreshRate = displayModeRefreshRate;
		}
		else
		{
			HDC hdc = GetDC(nullptr);
			lpDDSurfaceDesc2->dwWidth = GetSystemMetrics(SM_CXSCREEN);
			lpDDSurfaceDesc2->dwHeight = GetSystemMetrics(SM_CYSCREEN);
			lpDDSurfaceDesc2->dwRefreshRate = GetDeviceCaps(hdc, VREFRESH);
			displayModeBits = GetDeviceCaps(hdc, BITSPIXEL);
			ReleaseDC(nullptr, hdc);
		}

		// Set Pixel Format
		lpDDSurfaceDesc2->ddpfPixelFormat.dwFlags = DDPF_RGB;
		lpDDSurfaceDesc2->ddpfPixelFormat.dwRGBBitCount = displayModeBits;
		switch (displayModeBits)
		{
		case 8:
			lpDDSurfaceDesc2->ddpfPixelFormat.dwRBitMask = 0;
			lpDDSurfaceDesc2->ddpfPixelFormat.dwGBitMask = 0;
			lpDDSurfaceDesc2->ddpfPixelFormat.dwBBitMask = 0;
			break;
		case 16:
			GetPixelDisplayFormat(D3DFMT_R5G6B5, lpDDSurfaceDesc2->ddpfPixelFormat);
			break;
		case 24:
		case 32:
			GetPixelDisplayFormat(D3DFMT_X8R8G8B8, lpDDSurfaceDesc2->ddpfPixelFormat);
			break;
		default:
			Logging::Log() << __FUNCTION__ << " Not implemented bit count " << displayModeBits;
			return E_NOTIMPL;
		}

		return DD_OK;
	}

	return ProxyInterface->GetDisplayMode(lpDDSurfaceDesc2);
}

HRESULT m_IDirectDrawX::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	return ProxyInterface->GetFourCCCodes(lpNumCodes, lpCodes);
}

HRESULT m_IDirectDrawX::GetGDISurface(LPDIRECTDRAWSURFACE7 FAR * lplpGDIDDSSurface)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	HRESULT hr = ProxyInterface->GetGDISurface(lplpGDIDDSSurface);

	if (SUCCEEDED(hr) && lplpGDIDDSSurface)
	{
		*lplpGDIDDSSurface = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lplpGDIDDSSurface, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirectDrawX::GetMonitorFrequency(LPDWORD lpdwFrequency)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpdwFrequency)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (Config.Dd7to9)
	{
		// Make sure the device exists
		if (!d3d9Device)
		{
			Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
			return DDERR_INVALIDOBJECT;
		}

		D3DDISPLAYMODE Mode;
		if (FAILED(d3d9Device->GetDisplayMode(0, &Mode)))
		{
			Logging::Log() << __FUNCTION__ << " Failed to get display mode!";
			return DDERR_GENERIC;
		}

		*lpdwFrequency = Mode.RefreshRate;

		return DD_OK;
	}

	return ProxyInterface->GetMonitorFrequency(lpdwFrequency);
}

HRESULT m_IDirectDrawX::GetScanLine(LPDWORD lpdwScanLine)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpdwScanLine)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (Config.Dd7to9)
	{
		// Make sure the device exists
		if (!d3d9Device)
		{
			Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
			return DDERR_INVALIDOBJECT;
		}

		D3DRASTER_STATUS RasterStatus;
		if (FAILED(d3d9Device->GetRasterStatus(0, &RasterStatus)))
		{
			Logging::Log() << __FUNCTION__ << " Failed to get raster status!";
			return DDERR_GENERIC;
		}

		*lpdwScanLine = RasterStatus.ScanLine;

		return DD_OK;
	}

	return ProxyInterface->GetScanLine(lpdwScanLine);
}

HRESULT m_IDirectDrawX::GetVerticalBlankStatus(LPBOOL lpbIsInVB)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpbIsInVB)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (Config.Dd7to9)
	{
		// Make sure the device exists
		if (!d3d9Device)
		{
			Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
			return DDERR_INVALIDOBJECT;
		}

		D3DRASTER_STATUS RasterStatus;
		if (FAILED(d3d9Device->GetRasterStatus(0, &RasterStatus)))
		{
			Logging::Log() << __FUNCTION__ << " Failed to get raster status!";
			return DDERR_GENERIC;
		}

		*lpbIsInVB = RasterStatus.InVBlank;

		return DD_OK;
	}

	return ProxyInterface->GetVerticalBlankStatus(lpbIsInVB);
}

HRESULT m_IDirectDrawX::Initialize(GUID FAR * lpGUID)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// Not needed with d3d9
		return DD_OK;
	}

	return ProxyInterface->Initialize(lpGUID);
}

HRESULT m_IDirectDrawX::RestoreDisplayMode()
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// No exclusive mode
		if (!ExclusiveMode)
		{
			return DDERR_NOEXCLUSIVEMODE;
		}

		// Set mode
		ExclusiveMode = false;
		displayModeWidth = 0;
		displayModeHeight = 0;
		displayModeBPP = 0;
		displayModeRefreshRate = 0;

		return DD_OK;
	}

	return ProxyInterface->RestoreDisplayMode();
}

HRESULT m_IDirectDrawX::SetCooperativeLevel(HWND hWnd, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// Set windowed mode
		if ((dwFlags & DDSCL_NORMAL) || Config.EnableWindowMode)
		{
			isWindowed = true;
		}
		else if (dwFlags & DDSCL_FULLSCREEN)
		{
			isWindowed = false;
		}
		else
		{
			return DDERR_INVALIDPARAMS;
		}

		// Set ExclusiveMode
		ExclusiveMode = false;
		if (dwFlags & DDSCL_EXCLUSIVE)
		{
			ExclusiveMode = true;
		}

		// Set display window
		if (!hWnd)
		{
			// Set variables
			handle_data data;
			data.process_id = GetCurrentProcessId();
			data.best_handle = nullptr;

			// Gets all window layers and looks for a main window that is fullscreen
			EnumWindows(EnumProcWindowCallback, (LPARAM)&data);

			// Get top window
			HWND m_hWnd = GetTopWindow(data.best_handle);
			if (m_hWnd)
			{
				data.best_handle = m_hWnd;
			}

			// Cannot find window handle
			if (!data.best_handle)
			{
				Logging::Log() << __FUNCTION__ << " Could not get window handle";
			}

			// Return the best handle
			hWnd = data.best_handle;
		}
		MainhWnd = hWnd;

		return DD_OK;
	}

	// Release previouse Exclusive flag
	// Hook window message to get notified when the window is about to exit to remove the exclusive flag
	if ((dwFlags & DDSCL_EXCLUSIVE) && hWnd && hWnd != chWnd)
	{
		if (IsWindow(chWnd))
		{
			ProxyInterface->SetCooperativeLevel(chWnd, DDSCL_NORMAL);
		}
		else
		{
			if (g_hook)
			{
				UnhookWindowsHookEx(g_hook);
				g_hook = nullptr;
			}

			g_hookmap[hWnd] = this;
			g_hook = SetWindowsHookEx(WH_CBT, CBTProc, nullptr, GetWindowThreadProcessId(hWnd, nullptr));
		}

		if (chWnd)
		{
			g_hookmap.erase(chWnd);
		}

		chWnd = hWnd;
	}

	return ProxyInterface->SetCooperativeLevel(hWnd, dwFlags);
}

HRESULT m_IDirectDrawX::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// Set display mode to dwWidth x dwHeight with dwBPP color depth
		displayModeWidth = dwWidth;
		displayModeHeight = dwHeight;
		displayModeBPP = dwBPP;
		displayModeRefreshRate = dwRefreshRate;

		if (SetDefaultDisplayMode || !displayWidth || !displayHeight)
		{
			displayWidth = dwWidth;
			displayHeight = dwHeight;
		}

		// Create the requested d3d device for this display mode, report error on failure
		if (FAILED(CreateD3DDevice()))
		{
			Logging::Log() << __FUNCTION__ << " Error creating Direct3D9 Device";
			return DDERR_GENERIC;
		}

		return DD_OK;
	}

	// Force color mode
	if (Config.Force16bitColor)
	{
		dwBPP = 16;
	}
	if (Config.Force32bitColor)
	{
		dwBPP = 32;
	}

	if (ProxyDirectXVersion == 1)
	{
		return ((IDirectDraw*)ProxyInterface)->SetDisplayMode(dwWidth, dwHeight, dwBPP);
	}

	return ProxyInterface->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
}

HRESULT m_IDirectDrawX::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		if (dwFlags & DDWAITVB_BLOCKBEGIN)
		{
			// Return when vertical blank begins
		}
		else if (dwFlags & DDWAITVB_BLOCKBEGINEVENT)
		{
			// Triggers an event when the vertical blank begins. This value is not currently supported.
			Logging::Log() << __FUNCTION__ << " DDWAITVB_BLOCKBEGINEVENT not supported!";
			return DDERR_UNSUPPORTED;
		}
		else if (dwFlags & DDWAITVB_BLOCKEND)
		{
			// Return when the vertical-blank interval ends and the display begins.
		}

		// Vblank supported by vsync so just immediately return
		return DD_OK;
	}

	return ProxyInterface->WaitForVerticalBlank(dwFlags, hEvent);
}

/*********************************/
/*** Added in the v2 interface ***/
/*********************************/

HRESULT m_IDirectDrawX::GetAvailableVidMem(LPDDSCAPS lpDDSCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree)
{
	Logging::LogDebug() << __FUNCTION__;

	// Game using old DirectX, Convert DDSCAPS to DDSCAPS2
	if (ConvertSurfaceDescTo2)
	{
		DDSCAPS2 Caps2;
		if (lpDDSCaps)
		{
			ConvertCaps(Caps2, *lpDDSCaps);
		}

		return GetAvailableVidMem2((lpDDSCaps) ? &Caps2 : nullptr, lpdwTotal, lpdwFree);
	}

	return ((IDirectDraw3*)ProxyInterface)->GetAvailableVidMem(lpDDSCaps, lpdwTotal, lpdwFree);
}

HRESULT m_IDirectDrawX::GetAvailableVidMem2(LPDDSCAPS2 lpDDSCaps2, LPDWORD lpdwTotal, LPDWORD lpdwFree)
{
	Logging::LogDebug() << __FUNCTION__;

	HRESULT hr = DD_OK;

	if (Config.Dd7to9)
	{
		// Make sure the device exists
		if (!d3d9Device)
		{
			Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
			return DDERR_INVALIDOBJECT;
		}

		*lpdwFree = d3d9Device->GetAvailableTextureMem();
		*lpdwTotal = *lpdwFree + 0x400;		// Just make this slightly larger than free
	}
	else
	{
		hr = ProxyInterface->GetAvailableVidMem(lpDDSCaps2, lpdwTotal, lpdwFree);
	}

	// Set available memory, some games have issues if this is set to high
	if (Config.ConvertToDirectDraw7 && lpdwTotal && lpdwFree && *lpdwTotal > 0x8000000)
	{
		*lpdwFree = 0x8000000 - (*lpdwTotal - *lpdwFree);
		*lpdwTotal = 0x8000000;
	}

	return hr;
}

/*********************************/
/*** Added in the V4 Interface ***/
/*********************************/

HRESULT m_IDirectDrawX::GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 * lpDDS)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	HRESULT hr = ProxyInterface->GetSurfaceFromDC(hdc, lpDDS);

	if (SUCCEEDED(hr) && lpDDS)
	{
		*lpDDS = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lpDDS, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirectDrawX::RestoreAllSurfaces()
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		for (m_IDirectDrawSurfaceX* it : SurfaceVector)
		{
			HRESULT hr = it->Restore();
			if (FAILED(hr))
			{
				return hr;
			}
		}

		return DD_OK;
	}

	return ProxyInterface->RestoreAllSurfaces();
}

HRESULT m_IDirectDrawX::TestCooperativeLevel()
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		// Make sure the device exists
		if (!d3d9Device)
		{
			Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
			return DDERR_INVALIDOBJECT;
		}

		return d3d9Device->TestCooperativeLevel();
	}

	return ProxyInterface->TestCooperativeLevel();
}

HRESULT m_IDirectDrawX::GetDeviceIdentifier(LPDDDEVICEIDENTIFIER lpdddi, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpdddi)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (ProxyDirectXVersion > 4 && DirectXVersion < 7)
	{
		DDDEVICEIDENTIFIER2 Id2;

		HRESULT hr = GetDeviceIdentifier2(&Id2, dwFlags);

		if (SUCCEEDED(hr))
		{
			ConvertDeviceIdentifier(*lpdddi, Id2);
		}

		return hr;
	}

	return ((IDirectDraw4*)ProxyInterface)->GetDeviceIdentifier(lpdddi, dwFlags);
}

HRESULT m_IDirectDrawX::GetDeviceIdentifier2(LPDDDEVICEIDENTIFIER2 lpdddi2, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__;

	if (!lpdddi2)
	{
		return DDERR_INVALIDPARAMS;
	}

	if (Config.Dd7to9)
	{
		D3DADAPTER_IDENTIFIER9 Identifier9;
		HRESULT hr = d3d9Object->GetAdapterIdentifier(D3DADAPTER_DEFAULT, D3DENUM_WHQL_LEVEL, &Identifier9);

		if (FAILED(hr))
		{
			Logging::Log() << __FUNCTION__ << " Failed to get Adapter Identifier";
			return hr;
		}

		ConvertDeviceIdentifier(*lpdddi2, Identifier9);

		return DD_OK;
	}

	return ProxyInterface->GetDeviceIdentifier(lpdddi2, dwFlags);
}

HRESULT m_IDirectDrawX::StartModeTest(LPSIZE lpModesToTest, DWORD dwNumEntries, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	return ProxyInterface->StartModeTest(lpModesToTest, dwNumEntries, dwFlags);
}

HRESULT m_IDirectDrawX::EvaluateMode(DWORD dwFlags, DWORD * pSecondsUntilTimeout)
{
	Logging::LogDebug() << __FUNCTION__;

	if (Config.Dd7to9)
	{
		Logging::Log() << __FUNCTION__ << " Not Implemented";
		return E_NOTIMPL;
	}

	return ProxyInterface->EvaluateMode(dwFlags, pSecondsUntilTimeout);
}

/************************/
/*** Helper functions ***/
/************************/

// Creates d3d9 device, destroying the old one if exists
HRESULT m_IDirectDrawX::CreateD3DDevice()
{
	// Release all existing surfaces
	ReleaseAllD9Surfaces();

	// Release existing d3d9device
	ReleaseD3d9Device();

	// Check device caps to make sure it supports dynamic textures
	D3DCAPS9 d3dcaps;
	ZeroMemory(&d3dcaps, sizeof(D3DCAPS9));
	if (FAILED(d3d9Object->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dcaps)))
	{
		Logging::Log() << __FUNCTION__ << " Unable to retrieve device-specific information about the device";
		return DDERR_GENERIC;
	}

	// Is dynamic textures flag set?
	if ((d3dcaps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) == 0)
	{
		// No dynamic textures
		Logging::Log() << __FUNCTION__ << " Device does not support dynamic textures";
		return DDERR_GENERIC;
	}

	// Set display window
	ZeroMemory(&presParams, sizeof(presParams));
	presParams.hDeviceWindow = MainhWnd;

	// Set parameters for the current display mode
	if (isWindowed)
	{
		// Window mode
		presParams.Windowed = TRUE;
		// width/height
		presParams.BackBufferWidth = displayWidth;
		presParams.BackBufferHeight = displayHeight;
		// Swap discard
		presParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		// Unknown format
		presParams.BackBufferFormat = D3DFMT_UNKNOWN;
		// Interval level
		presParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	else
	{
		// Enumerate modes for format XRGB
		UINT modeCount = d3d9Object->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);

		// Get width and height
		DWORD BackBufferWidth = displayWidth;
		DWORD BackBufferHeight = displayHeight;
		if (!BackBufferWidth || !BackBufferHeight)
		{
			BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
			BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
		}

		// Loop through all modes looking for our requested resolution
		D3DDISPLAYMODE d3ddispmode;
		D3DDISPLAYMODE set_d3ddispmode = { NULL };
		bool modeFound = false;
		for (UINT i = 0; i < modeCount; i++)
		{
			// Get display modes here
			ZeroMemory(&d3ddispmode, sizeof(D3DDISPLAYMODE));
			if (FAILED(d3d9Object->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &d3ddispmode)))
			{
				Logging::Log() << __FUNCTION__ << " EnumAdapterModes failed";
				return DDERR_GENERIC;
			}
			if (d3ddispmode.Width == BackBufferWidth && d3ddispmode.Height == BackBufferHeight &&				// Check height and width
				(d3ddispmode.RefreshRate == displayModeRefreshRate || displayModeRefreshRate == 0))		// Check refresh rate
			{
				// Found a match
				modeFound = true;
				memcpy(&set_d3ddispmode, &d3ddispmode, sizeof(D3DDISPLAYMODE));
			}
		}

		// No mode found
		if (!modeFound)
		{
			Logging::Log() << __FUNCTION__ << " Failed to find compatible fullscreen display mode " << BackBufferWidth << "x" << BackBufferHeight;
			return DDERR_GENERIC;
		}

		// Fullscreen
		presParams.Windowed = FALSE;
		// width/height
		presParams.BackBufferWidth = set_d3ddispmode.Width;
		presParams.BackBufferHeight = set_d3ddispmode.Height;
		// Discard swap
		presParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		// Display mode fullscreen format
		presParams.BackBufferFormat = set_d3ddispmode.Format;
		// Display mode refresh
		presParams.FullScreen_RefreshRateInHz = set_d3ddispmode.RefreshRate;
		// Interval level
		presParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	}

	Logging::LogDebug() << __FUNCTION__ << " D3d9 Device size: " << presParams.BackBufferWidth << "x" << presParams.BackBufferHeight;
	// D3DCREATE_NOWINDOWCHANGES possible for alt+tab and mouse leaving window
	// create d3d device with hardware vertex processing if it's available
	if (FAILED(d3d9Object->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, MainhWnd, (d3dcaps.VertexProcessingCaps) ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presParams, &d3d9Device)))
	{
		Logging::Log() << __FUNCTION__ << " Failed to create Direct3D9 device";
		return DDERR_GENERIC;
	}

	// Reset BeginScene
	IsInScene = false;

	// Success
	return DD_OK;
}

// Reinitialize d3d9 device
HRESULT m_IDirectDrawX::ReinitDevice()
{
	// Release existing surfaces
	ReleaseAllD9Surfaces();

	// Attempt to reset the device
	if (FAILED(d3d9Device->Reset(&presParams)))
	{
		Logging::Log() << __FUNCTION__ << " Failed to reset Direct3D9 device";
		return DDERR_GENERIC;
	}

	// Reset BeginScene
	IsInScene = false;

	// Success
	return DD_OK;
}

// Release all d3d9 surfaces
void m_IDirectDrawX::ReleaseAllD9Surfaces()
{
	for (m_IDirectDrawSurfaceX *pSurface : SurfaceVector)
	{
		pSurface->ReleaseD9Surface();
	}
}

// Release all d3d9 classes for Release()
void m_IDirectDrawX::ReleaseD3d9Device()
{
	if (d3d9Device)
	{
		DWORD x = 0, z = 100;
		while (z != 0 && ++x < 100)
		{
			z = d3d9Device->Release();
		}

		// Add error checking
		if (z != 0)
		{
			Logging::Log() << __FUNCTION__ << " Unable to release Direct3D9 device";
		}

		d3d9Device = nullptr;
	}
}

// Release all d3d9 classes for Release()
void m_IDirectDrawX::ReleaseD3d9()
{
	// Release existing surfaces
	ReleaseAllD9Surfaces();

	// Release existing d3d9device
	ReleaseD3d9Device();

	// Release existing d3d9object
	d3d9Object->Release();
}

// Fixes a bug in ddraw in Windows 8 and 10 where the exclusive flag remains even after the window (hWnd) closes
LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	if (nCode == HCBT_DESTROYWND)
	{
		HWND hWnd = (HWND)wParam;
		m_IDirectDrawX *Interface = (m_IDirectDrawX*)InterlockedExchangePointer((PVOID*)&CurrentDDInterface, nullptr);
		if (Interface && Interface == g_hookmap[hWnd])
		{
			Interface->SetCooperativeLevel(hWnd, DDSCL_NORMAL);
		}
		g_hookmap.erase(hWnd);
	}

	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Enums all windows and returns the handle to the active window
BOOL CALLBACK EnumProcWindowCallback(HWND hwnd, LPARAM lParam)
{
	// Get variables from call back
	handle_data& data = *(handle_data*)lParam;

	// Skip windows that are from a different process ID
	DWORD process_id;
	GetWindowThreadProcessId(hwnd, &process_id);
	if (data.process_id != process_id)
	{
		return TRUE;
	}

	// Skip compatibility class windows
	char class_name[80] = { 0 };
	GetClassName(hwnd, class_name, sizeof(class_name));
	if (strcmp(class_name, "CompatWindowDesktopReplacement") == 0)			// Compatibility class windows
	{
		return TRUE;
	}

	// Match found returning value
	data.best_handle = hwnd;
	return FALSE;
}

// Add surface wrapper to vector
void m_IDirectDrawX::AddSurfaceToVector(m_IDirectDrawSurfaceX* lpSurfaceX)
{
	if (!lpSurfaceX)
	{
		return;
	}

	// Store surface
	SurfaceVector.push_back(lpSurfaceX);
}

// Remove surface wrapper from vector
void m_IDirectDrawX::RemoveSurfaceFromVector(m_IDirectDrawSurfaceX* lpSurfaceX)
{
	if (!lpSurfaceX)
	{
		return;
	}

	auto it = std::find_if(SurfaceVector.begin(), SurfaceVector.end(),
		[=](auto pSurface) -> bool { return pSurface == lpSurfaceX; });

	if (it != std::end(SurfaceVector))
	{
		SurfaceVector.erase(it);

		// Remove attached surface from map
		for (auto ip : SurfaceVector)
		{
			ip->RemoveAttachedSurfaceFromMap(lpSurfaceX);
		}
	}
}

// Check if surface wrapper exists
bool m_IDirectDrawX::DoesSurfaceExist(m_IDirectDrawSurfaceX* lpSurfaceX)
{
	if (!lpSurfaceX)
	{
		return false;
	}

	auto it = std::find_if(SurfaceVector.begin(), SurfaceVector.end(),
		[=](auto pSurface) -> bool { return pSurface == lpSurfaceX; });

	if (it == std::end(SurfaceVector))
	{
		return false;
	}

	return true;
}

// Do d3d9 BeginScene if all surfaces are unlocked
HRESULT m_IDirectDrawX::BeginScene()
{
	// Check if need to run BeginScene
	if (IsInScene)
	{
		return DDERR_GENERIC;
	}

	// Make sure the device exists
	if (!d3d9Device)
	{
		Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
		return DDERR_INVALIDOBJECT;
	}

	// Check if any surfaces are locked
	for (m_IDirectDrawSurfaceX* it : SurfaceVector)
	{
		if (it->IsSurfaceLocked())
		{
			return DDERR_LOCKEDSURFACES;
		}
	}

	// Begin scene
	if (FAILED(d3d9Device->BeginScene()))
	{
		Logging::Log() << __FUNCTION__ << " Failed to begin scene";
		return DDERR_GENERIC;
	}

	IsInScene = true;

	return DD_OK;
}

// Do d3d9 EndScene and Present if all surfaces are unlocked
HRESULT m_IDirectDrawX::EndScene()
{
	// Check if surface Flip/Blt has occured 
	if (!IsInScene)
	{
		return DDERR_GENERIC;
	}

	// Make sure the device exists
	if (!d3d9Device)
	{
		Logging::Log() << __FUNCTION__ << " called when d3d9device doesn't exist";
		return DDERR_INVALIDOBJECT;
	}

	// Check if any surfaces are locked
	for (m_IDirectDrawSurfaceX* it : SurfaceVector)
	{
		if (it->IsSurfaceLocked())
		{
			return DDERR_LOCKEDSURFACES;
		}
	}

	// Draw primitive
	if (FAILED(d3d9Device->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2)))
	{
		Logging::Log() << __FUNCTION__ << " Failed to draw primitive";
		return DDERR_GENERIC;
	}

	// End scene
	if (FAILED(d3d9Device->EndScene()))
	{
		Logging::Log() << __FUNCTION__ << " Failed to end scene error ";
		return DDERR_GENERIC;
	}

	IsInScene = false;

	// Present everthing
	HRESULT hr = d3d9Device->Present(nullptr, nullptr, nullptr, nullptr);

	// Device lost
	if (hr == D3DERR_DEVICELOST)
	{
		// Attempt to reinit device
		return ReinitDevice();
	}
	else if (FAILED(hr))
	{
		Logging::Log() << __FUNCTION__ << " Failed to present scene";
		return DDERR_GENERIC;
	}

	return DD_OK;
}
