//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2001 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "d3d_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TDirect3DDrawer		Direct3DDrawer;
#if DIRECT3D_VERSION >= 0x0800
static bool					Windowed;
#endif

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	TDirect3DDrawer::TDirect3DDrawer
//
//==========================================================================

TDirect3DDrawer::TDirect3DDrawer(void) :
	device("d3d_device", "0", CVAR_ARCHIVE),
	clear("d3d_clear", "0", CVAR_ARCHIVE),
	tex_linear("d3d_tex_linear", "2", CVAR_ARCHIVE),
	dither("d3d_dither", "0", CVAR_ARCHIVE),
	IdentityMatrix(	1, 0, 0, 0,
					0, 1, 0, 0,
					0, 0, 1, 0,
					0, 0, 0, 1)
{
	_Direct3DDrawer = this;

#if DIRECT3D_VERSION < 0x0800
	//	DirectDraw interfaces
	DDraw = NULL;
	PrimarySurface = NULL;
	RenderSurface = NULL;
	ZBuffer = NULL;
#endif

	//	Direct3D interfaces
	Direct3D = NULL;
	RenderDevice = NULL;
}

//==========================================================================
//
//	TDirect3DDrawer::Init
//
// 	Determine the hardware configuration
//
//==========================================================================

void TDirect3DDrawer::Init(void)
{
#if DIRECT3D_VERSION >= 0x0800
	typedef IDirect3D8* (WINAPI*fp_Direct3DCreate8)(UINT SDKVersion);

	fp_Direct3DCreate8 p_Direct3DCreate8;

	DLLHandle = LoadLibrary("d3d8.dll");
	if (!DLLHandle)
	{
		Sys_Error("Couldn't load d3d8.dll");
	}

	p_Direct3DCreate8 = fp_Direct3DCreate8(GetProcAddress(DLLHandle, "Direct3DCreate8"));
	if (!p_Direct3DCreate8)
	{
		Sys_Error("Symbol Direct3DCreate8 not found");
	}

	// Create Direct3D object
	Direct3D = p_Direct3DCreate8(D3D_SDK_VERSION);
	if (!Direct3D)
		Sys_Error("Failed to create Direct3D object");

	Windowed = !!M_CheckParm("-window");
#else
	HRESULT			result;

	// Create DirectDraw object
	result = CoCreateInstance(CLSID_DirectDraw7, NULL,
		CLSCTX_ALL, IID_IDirectDraw7, (void**)&DDraw);
	if (result != DD_OK)
		Sys_Error("I_InitGraphics: Failed to create DirecDraw7");

	// Initialize
	result = DDraw->Initialize(NULL);
	if (result != DD_OK)
		Sys_Error("I_InitGraphics: Failed to initialize DirectDraw");

	// Set cooperative level
	result = DDraw->SetCooperativeLevel(hwnd,
		DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_FPUPRESERVE);
	if (result != DD_OK)
		Sys_Error("I_InitGraphics: Failed to set cooperative level.");

	// Create Direct3D object
	result = DDraw->QueryInterface(IID_IDirect3D7, (void**)&Direct3D);
	if (FAILED(result))
		Sys_Error("Failed to create Direct3D object");
#endif
}

//==========================================================================
//
//	TDirect3DDrawer::InitData
//
//==========================================================================

void TDirect3DDrawer::InitData(void)
{
}

#if DIRECT3D_VERSION < 0x0800

//==========================================================================
//
//	EnumDevicesCallback
//
//==========================================================================

static HRESULT CALLBACK EnumDevicesCallback(
	LPSTR lpDeviceDesc,
	LPSTR lpDeviceName,
	LPD3DDEVICEDESC7 lpD3DDeviceDesc,
	LPVOID)
{
	cond << "Device " << lpDeviceName << endl;
	cond << "Description: " << lpDeviceDesc << endl;
	cond << lpD3DDeviceDesc;
	cond << "-------------------------------------\n";

	return D3DENUMRET_OK;
}

//==========================================================================
//
//	EnumZBufferCallback
//
//==========================================================================

static HRESULT CALLBACK EnumZBufferCallback(LPDDPIXELFORMAT pf, void* dst)
{
	cond << "Z buffer format\n"
		<< pf
		<< "-------------------------------------\n";

	// Choose best available z-buffer bit depth
	if (pf->dwFlags == DDPF_ZBUFFER &&
		pf->dwZBufferBitDepth > ((LPDDPIXELFORMAT)dst)->dwZBufferBitDepth)
	{
		memcpy(dst, pf, sizeof(DDPIXELFORMAT));
	}
 
	// Return with D3DENUMRET_OK to continue the search.
	return D3DENUMRET_OK;
}

//==========================================================================
//
//	EnumPixelFormatsCallback
//
//==========================================================================

static HRESULT CALLBACK EnumPixelFormatsCallback(LPDDPIXELFORMAT pf, void* dst)
{
	cond << "Pixel format\n"
		<< pf
		<< "-------------------------------------\n";

	if ((pf->dwFlags == (DDPF_RGB|DDPF_ALPHAPIXELS)) &&
		(pf->dwRGBBitCount == 16))
	{
		memcpy(dst, pf, sizeof(DDPIXELFORMAT));
 
		// Return with D3DENUMRET_CANCEL to end the search.
		return D3DENUMRET_CANCEL;
	}

	// Return with D3DENUMRET_OK to continue the search.
	return D3DENUMRET_OK;
}

//==========================================================================
//
//	EnumPixelFormats32Callback
//
//==========================================================================

static HRESULT CALLBACK EnumPixelFormats32Callback(LPDDPIXELFORMAT pf, void* dst)
{
	if ((pf->dwFlags == (DDPF_RGB|DDPF_ALPHAPIXELS)) &&
		(pf->dwRGBBitCount == 32))
	{
		memcpy(dst, pf, sizeof(DDPIXELFORMAT));

		// Return with D3DENUMRET_CANCEL to end the search.
		return D3DENUMRET_CANCEL;
	}

	// Return with D3DENUMRET_OK to continue the search.
	return D3DENUMRET_OK;
}

//==========================================================================
//
//	GetBits
//
//==========================================================================

int GetBits(dword mask)
{
	int answer = 0;
	while (mask)
	{
		if (mask & 1)
		{
			answer++;
		}
		mask >>= 1;
	}
	return answer;
}

//==========================================================================
//
//	GetShift
//
//==========================================================================

int GetShift(dword mask)
{
	if (!mask)
		return 0;
	int answer = 0;
	while (!(mask & 1))
	{
		answer++;
		mask >>= 1;
	}
	return answer;
}

#endif

//==========================================================================
//
// 	TDirect3DDrawer::SetResolution
//
// 	Set up the video mode
//
//==========================================================================

bool TDirect3DDrawer::SetResolution(int Width, int Height, int BPP)
{
	if (!Width || !Height)
	{
		//	Set defaults
		Width = 640;
		Height = 480;
		BPP = 16;
	}

	if (BPP == 15) BPP = 16;

	if (BPP < 16)
	{
		//	True-color only
		return false;
	}

#if DIRECT3D_VERSION >= 0x0800
	//	Shut down current mode
	ReleaseTextures();
	SAFE_RELEASE(RenderDevice)

	D3DPRESENT_PARAMETERS d3dpp;

	if (Windowed)
	{
		RECT WindowRect;
		WindowRect.left = 0;
		WindowRect.right = Width;
		WindowRect.top = 0;
		WindowRect.bottom = Height;
		AdjustWindowRectEx(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE,
			WS_EX_APPWINDOW);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, WindowRect.right - WindowRect.left,
			WindowRect.bottom - WindowRect.top, SWP_NOMOVE);
	}

	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferWidth = Width;
	d3dpp.BackBufferHeight = Height;
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	d3dpp.BackBufferCount = 1;
	d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
	d3dpp.hDeviceWindow = hwnd;
	d3dpp.Windowed = Windowed;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

	if (Direct3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
//		D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
		&d3dpp, &RenderDevice) != D3D_OK)
	{
		return false;
	}

	D3DCAPS8 DeviceCaps;
	RenderDevice->GetDeviceCaps(&DeviceCaps);
//	Cvar_Set("r_sort_sprites", int((DeviceCaps.DevCaps & D3DDEVCAPS_SORTINCREASINGZ) != 0));
	square_textures = (DeviceCaps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
	maxTexSize = MAX(DeviceCaps.MaxTextureWidth, DeviceCaps.MaxTextureHeight);
	if (square_textures)
	{
		//	Limit texture size when square textures are requred
		maxTexSize = 256;
	}
	maxMultiTex = DeviceCaps.MaxSimultaneousTextures;
	if (device == 1)
	{
		//	In software actually can be only one texture
		maxMultiTex = 1;
	}
	RenderDevice->SetTextureStageState(0, D3DTSS_MAXANISOTROPY, DeviceCaps.MaxAnisotropy);

	abits = 1;
	ashift = 15;
	rbits = 5;
	rshift = 10;
	gbits = 5;
	gshift = 5;
	bbits = 5;
	bshift = 0;

	ashift32 = 24;
	rshift32 = 16;
	gshift32 = 8;
	bshift32 = 0;
#else
	con << "-------------------------------------\n";

	DDSURFACEDESC2	ddsd;

	//	Shut down current mode
	ReleaseTextures();
	SAFE_RELEASE(RenderDevice)
	SAFE_RELEASE(ZBuffer)
	SAFE_RELEASE(RenderSurface)
	SAFE_RELEASE(PrimarySurface)

	//	Set video mode
	if (DDraw->SetDisplayMode(Width, Height, BPP, 0, 0) != DD_OK)
		return false;

	//	Create primary surface
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	if (DDraw->CreateSurface(&ddsd, &PrimarySurface, NULL) != DD_OK)
		Sys_Error("I_SetResolution: Failed to create primary surface");

	Direct3D->EnumDevices(EnumDevicesCallback, NULL);

	GUID DeviceGUID;
	switch (device)
	{
	 default:
		device = 0;
	 case 0:
		DeviceGUID = IID_IDirect3DHALDevice;
		SurfaceMemFlag = DDSCAPS_VIDEOMEMORY;
		break;

	 case 1:
		DeviceGUID = IID_IDirect3DRGBDevice;
		SurfaceMemFlag = DDSCAPS_SYSTEMMEMORY;
		break;

	 case 2:
		DeviceGUID = IID_IDirect3DRefDevice;
		SurfaceMemFlag = DDSCAPS_SYSTEMMEMORY;
		break;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | SurfaceMemFlag;
	ddsd.dwWidth  = Width;
	ddsd.dwHeight = Height;
 
	//	Create the back buffer.
	if (FAILED(DDraw->CreateSurface(&ddsd, &RenderSurface, NULL)))
		return false;

	//	Create the z-buffer
	DDPIXELFORMAT ddpfZBuffer;
	memset(&ddpfZBuffer, 0, sizeof(ddpfZBuffer));
	Direct3D->EnumZBufferFormats(DeviceGUID,
		EnumZBufferCallback, (VOID*)&ddpfZBuffer);

	//	If the enumerated format is good (it should be), the dwSize member
	// will be properly initialized. Check this just in case.
	if (sizeof(DDPIXELFORMAT) != ddpfZBuffer.dwSize)
		return false;
 
	// Get z-buffer dimensions from the render target
	// Setup the surface desc for the z-buffer.
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | SurfaceMemFlag;
	ddsd.dwWidth = Width;
	ddsd.dwHeight = Height;
	memcpy(&ddsd.ddpfPixelFormat, &ddpfZBuffer, sizeof(DDPIXELFORMAT));

	// Create the depth-buffer.
	if (FAILED(DDraw->CreateSurface(&ddsd, &ZBuffer, NULL)))
		return false;

	// Attach the z-buffer to the back buffer.
	if (FAILED(RenderSurface->AddAttachedSurface(ZBuffer)))
		return false;

	//	Create rendering device
	if (FAILED(Direct3D->CreateDevice(DeviceGUID,
		RenderSurface, &RenderDevice)))
	{
		return false;
	}

	D3DDEVICEDESC7	DeviceDesc;
	RenderDevice->GetCaps(&DeviceDesc);
	Cvar_Set("r_sort_sprites", int((DeviceDesc.dwDevCaps & D3DDEVCAPS_SORTINCREASINGZ) != 0));
	square_textures = (DeviceDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
	// needed?
	//minTexSize = MAX(DeviceDesc.dwMinTextureWidth, DeviceDesc.dwMinTextureHeight);
	maxTexSize = MAX(DeviceDesc.dwMaxTextureWidth, DeviceDesc.dwMaxTextureHeight);
	if (square_textures)
	{
		//	Limit texture size when square textures are requred
		maxTexSize = 256;
	}
	maxMultiTex = DeviceDesc.wMaxSimultaneousTextures;
	if (device == 1)
	{
		//	In software actually can be only one texture
		maxMultiTex = 1;
	}
	RenderDevice->SetTextureStageState(0, D3DTSS_MAXANISOTROPY, DeviceDesc.dwMaxAnisotropy);

	memset(&PixelFormat, 0, sizeof(PixelFormat));
	RenderDevice->EnumTextureFormats(EnumPixelFormatsCallback, &PixelFormat);
	if (PixelFormat.dwSize != sizeof(DDPIXELFORMAT))
		return false;
	abits = GetBits(PixelFormat.dwRGBAlphaBitMask);
	ashift = GetShift(PixelFormat.dwRGBAlphaBitMask);
	rbits = GetBits(PixelFormat.dwRBitMask);
	rshift = GetShift(PixelFormat.dwRBitMask);
	gbits = GetBits(PixelFormat.dwGBitMask);
	gshift = GetShift(PixelFormat.dwGBitMask);
	bbits = GetBits(PixelFormat.dwBBitMask);
	bshift = GetShift(PixelFormat.dwBBitMask);

	memset(&PixelFormat32, 0, sizeof(PixelFormat32));
	RenderDevice->EnumTextureFormats(EnumPixelFormats32Callback, &PixelFormat32);
	ashift32 = GetShift(PixelFormat32.dwRGBAlphaBitMask);
	rshift32 = GetShift(PixelFormat32.dwRBitMask);
	gshift32 = GetShift(PixelFormat32.dwGBitMask);
	bshift32 = GetShift(PixelFormat32.dwBBitMask);

//	Sys_HighFPPrecision();
#endif

	//	Set screen params
	ScreenWidth = Width;
	ScreenHeight = Height;
	ScreenBPP = BPP;

	return true;
}

//==========================================================================
//
//	TDirect3DDrawer::InitResolution
//
//==========================================================================

void TDirect3DDrawer::InitResolution(void)
{
#if DIRECT3D_VERSION >= 0x0800
	RenderDevice->SetStreamSource(0, NULL, sizeof(MyD3DVertex));
	RenderDevice->SetVertexShader(MYD3D_VERTEX_FORMAT);
#endif
	RenderDevice->SetTransform(D3DTRANSFORMSTATE_WORLD, &IdentityMatrix);

	RenderDevice->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
 	
	RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
	RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHAREF, 170);

	RenderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
	RenderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);

	RenderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	RenderDevice->SetRenderState(D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT);
	RenderDevice->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);

	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGVERTEXMODE, D3DFOG_LINEAR);
	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGCOLOR, 0x007f7f7f);
	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGDENSITY, PassFloat(0.5));
	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGSTART, PassFloat(1.0));
	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGEND, PassFloat(2048.0));
}

//==========================================================================
//
//	TDirect3DDrawer::NewMap
//
//==========================================================================

void TDirect3DDrawer::NewMap(void)
{
	FlushCaches(false);
}

//==========================================================================
//
//	TDirect3DDrawer::StartUpdate
//
//==========================================================================

void TDirect3DDrawer::StartUpdate(void)
{
#if DIRECT3D_VERSION < 0x0800
	// Check for lost surface
	if (RenderSurface->IsLost() != DD_OK)
	{
		RenderSurface->Restore();
	}
#endif

	//	Clear surface
	if (clear)
	{
		RenderDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0, 0);
	}

#if DIRECT3D_VERSION >= 0x0800
	//	Setup texture filtering
	if (tex_linear == 4)
	{
		magfilter = D3DTEXF_ANISOTROPIC;
		minfilter = D3DTEXF_ANISOTROPIC;
		mipfilter = D3DTEXF_LINEAR;
	}
	else if (tex_linear == 3)
	{
		magfilter = D3DTEXF_LINEAR;
		minfilter = D3DTEXF_LINEAR;
		mipfilter = D3DTEXF_LINEAR;
	}
	else if (tex_linear == 2)
	{
		magfilter = D3DTEXF_LINEAR;
		minfilter = D3DTEXF_LINEAR;
		mipfilter = D3DTEXF_POINT;
	}
	else if (tex_linear)
	{
		magfilter = D3DTEXF_LINEAR;
		minfilter = D3DTEXF_LINEAR;
		mipfilter = D3DTEXF_NONE;
	}
	else
	{
		magfilter = D3DTEXF_POINT;
		minfilter = D3DTEXF_POINT;
		mipfilter = D3DTEXF_NONE;
	}
#else
	//	Setup texture filtering
	if (tex_linear == 4)
	{
		magfilter = D3DTFG_ANISOTROPIC;
		minfilter = D3DTFN_ANISOTROPIC;
		mipfilter = D3DTFP_LINEAR;
	}
	else if (tex_linear == 3)
	{
		magfilter = D3DTFG_LINEAR;
		minfilter = D3DTFN_LINEAR;
		mipfilter = D3DTFP_LINEAR;
	}
	else if (tex_linear == 2)
	{
		magfilter = D3DTFG_LINEAR;
		minfilter = D3DTFN_LINEAR;
		mipfilter = D3DTFP_POINT;
	}
	else if (tex_linear)
	{
		magfilter = D3DTFG_LINEAR;
		minfilter = D3DTFN_LINEAR;
		mipfilter = D3DTFP_NONE;
	}
	else
	{
		magfilter = D3DTFG_POINT;
		minfilter = D3DTFN_POINT;
		mipfilter = D3DTFP_NONE;
	}
#endif

	RenderDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, magfilter);
	RenderDevice->SetTextureStageState(0, D3DTSS_MINFILTER, minfilter);
	RenderDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, mipfilter);

	//	Dithering
	if (dither)
	{
		RenderDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE);
	}
	else
	{
		RenderDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE);
	}

	// Begin the scene.
	RenderDevice->BeginScene();

	Setup2D();
}

//==========================================================================
//
//	TDirect3DDrawer::Setup2D
//
//==========================================================================

void TDirect3DDrawer::Setup2D(void)
{
	//	Setup viewport
#if DIRECT3D_VERSION >= 0x0800
	D3DVIEWPORT8 view2D;
	memset(&view2D, 0, sizeof(D3DVIEWPORT8));
	view2D.X = 0;
	view2D.Y = 0;
	view2D.Width  = ScreenWidth;
	view2D.Height = ScreenHeight;
	view2D.MinZ = 0.0f;
	view2D.MaxZ = 1.0f;
#else
	D3DVIEWPORT7 view2D;
	memset(&view2D, 0, sizeof(D3DVIEWPORT7));
	view2D.dwX = 0;
	view2D.dwY = 0;
	view2D.dwWidth  = ScreenWidth;
	view2D.dwHeight = ScreenHeight;
	view2D.dvMinZ = 0.0f;
	view2D.dvMaxZ = 1.0f;
#endif
	RenderDevice->SetViewport(&view2D);

	//	Setup projection
	MyD3DMatrix proj2D = IdentityMatrix;
	proj2D(0, 0) = 2.0 / (float)ScreenWidth;
	proj2D(1, 1) = -2.0 / (float)ScreenHeight;
	proj2D(3, 0) = -1.0;
	proj2D(3, 1) = 1.0;
	RenderDevice->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &proj2D);

	RenderDevice->SetTransform(D3DTRANSFORMSTATE_VIEW, &IdentityMatrix);

	RenderDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
}

//==========================================================================
//
//	TDirect3DDrawer::SetupView
//
//==========================================================================

void TDirect3DDrawer::SetupView(const refdef_t *rd)
{
	if (rd->drawworld && rd->width != ScreenWidth)
	{
		R_DrawViewBorder();
	}

	//	Setup viewport
#if DIRECT3D_VERSION >= 0x0800
	memset(&viewData, 0, sizeof(D3DVIEWPORT8));
	viewData.X = rd->x;
	viewData.Y = rd->y;
	viewData.Width  = rd->width;
	viewData.Height = rd->height;
	viewData.MinZ = 0;
	viewData.MaxZ = 1;
#else
	memset(&viewData, 0, sizeof(D3DVIEWPORT7));
	viewData.dwX = rd->x;
	viewData.dwY = rd->y;
	viewData.dwWidth  = rd->width;
	viewData.dwHeight = rd->height;
	viewData.dvMinZ = 0;
	viewData.dvMaxZ = 1;
#endif
	RenderDevice->SetViewport(&viewData);

	//	Setup projection
	memset(&matProj, 0, sizeof(D3DMATRIX));
	matProj(0, 0) = 1 / rd->fovx;
	matProj(1, 1) = 1 / rd->fovy;
	float zFar = 8192.0;
	float zNear = 1.0;
	float Q = zFar / (zFar - zNear);
	matProj(2, 2) = Q;
	matProj(3, 2) = -Q * zNear;
	matProj(2, 3) = 1;
	RenderDevice->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &matProj);

	// The view matrix defines the position and orientation of the camera.
	MyD3DMatrix matView;
	matView(0, 0) = viewright.x;
	matView(1, 0) = viewright.y;
	matView(2, 0) = viewright.z;
	matView(3, 0) = -DotProduct(vieworg, viewright);
	matView(0, 1) = viewup.x;
	matView(1, 1) = viewup.y;
	matView(2, 1) = viewup.z;
	matView(3, 1) = -DotProduct(vieworg, viewup);
	matView(0, 2) = viewforward.x;
	matView(1, 2) = viewforward.y;
	matView(2, 2) = viewforward.z;
	matView(3, 2) = -DotProduct(vieworg, viewforward);
	matView(0, 3) = 0;
	matView(1, 3) = 0;
	matView(2, 3) = 0;
	matView(3, 3) = 1;
	RenderDevice->SetTransform(D3DTRANSFORMSTATE_VIEW, &matView);

	RenderDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);

	RenderDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, TRUE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
	RenderDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, r_use_fog ? TRUE : FALSE);

	memset(light_chain, 0, sizeof(light_chain));

	RenderDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0, 0);
}

//==========================================================================
//
//	TDirect3DDrawer::EndView
//
//==========================================================================

void TDirect3DDrawer::EndView(void)
{
	Setup2D();

	cl.cshifts[7] = cl.prev_cshifts[7];
	for (int i = 0; i < NUM_CSHIFTS; i++)
	{
		if (!cl.cshifts[i])
		{
			continue;
		}

		MyD3DVertex	dv[4];

		RenderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
		RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
		RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);

		dv[0] = MyD3DVertex(0, 0, cl.cshifts[i], 0, 0);
		dv[1] = MyD3DVertex(ScreenWidth, 0, cl.cshifts[i], 0, 0);
		dv[2] = MyD3DVertex(ScreenWidth, ScreenHeight, cl.cshifts[i], 0, 0);
		dv[3] = MyD3DVertex(0, ScreenHeight, cl.cshifts[i], 0, 0);

#if DIRECT3D_VERSION >= 0x0800
		RenderDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, dv, sizeof(MyD3DVertex));
#else
		RenderDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, MYD3D_VERTEX_FORMAT, dv, 4, 0);
#endif

		RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
		RenderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
		RenderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	}
}

//==========================================================================
//
//	TDirect3DDrawer::Update
//
// 	Blit to the screen / Flip surfaces
//
//==========================================================================

void TDirect3DDrawer::Update(void)
{
	// End the scene.
	RenderDevice->EndScene();

#if DIRECT3D_VERSION >= 0x0800
	RenderDevice->Present(NULL, NULL, NULL, NULL);
#else
	// Check for lost surface
	if (PrimarySurface->IsLost() != DD_OK)
	{
		PrimarySurface->Restore();
	}

	PrimarySurface->Blt(NULL, RenderSurface, NULL, DDBLT_WAIT, NULL);
#endif
}

//==========================================================================
//
//	TDirect3DDrawer::BeginDirectUpdate
//
//==========================================================================

void TDirect3DDrawer::BeginDirectUpdate(void)
{
#if DIRECT3D_VERSION < 0x0800
	// Check for lost surface
	if (RenderSurface->IsLost() != DD_OK)
	{
		RenderSurface->Restore();
	}
#endif

	// Begin the scene.
	RenderDevice->BeginScene();
}

//==========================================================================
//
//	TDirect3DDrawer::EndDirectUpdate
//
//==========================================================================

void TDirect3DDrawer::EndDirectUpdate(void)
{
	//FIXME
	Update();
}

//==========================================================================
//
// 	TDirect3DDrawer::Shutdown
//
//	Close the graphics
//
//==========================================================================

void TDirect3DDrawer::Shutdown(void)
{
	ReleaseTextures();
#if DIRECT3D_VERSION >= 0x0800
	SAFE_RELEASE(RenderDevice)
	SAFE_RELEASE(Direct3D)
	if (DLLHandle)
	{
		FreeLibrary(DLLHandle);
		DLLHandle = NULL;
	}
#else
	SAFE_RELEASE(RenderDevice)
	SAFE_RELEASE(Direct3D)

	SAFE_RELEASE(ZBuffer)
	SAFE_RELEASE(RenderSurface)
	SAFE_RELEASE(PrimarySurface)
	SAFE_RELEASE(DDraw)
#endif
}

//==========================================================================
//
//	TDirect3DDrawer::ReadScreen
//
//==========================================================================

void *TDirect3DDrawer::ReadScreen(int *bpp, bool *bot2top)
{
	//	Allocate buffer
	void *dst = Z_Malloc(ScreenWidth * ScreenHeight * sizeof(rgb_t), PU_VIDEO, 0);
	if (!dst)
	{
		return NULL;
	}

#if DIRECT3D_VERSION >= 0x0800
	LPDIRECT3DSURFACE8 surf;
	RenderDevice->GetRenderTarget(&surf);

	D3DSURFACE_DESC desc;
	surf->GetDesc(&desc);


	//	Decode pixel format
	int scr_rbits;
	int scr_rshift;
	int scr_gbits;
	int scr_gshift;
	int scr_bbits;
	int scr_bshift;
	int scr_pixbytes;
	if (desc.Format == D3DFMT_X1R5G5B5)
	{
		scr_rbits = 5;
		scr_rshift = 10;
		scr_gbits = 5;
		scr_gshift = 5;
		scr_bbits = 5;
		scr_bshift = 0;
		scr_pixbytes = 2;
	}
	else if (desc.Format == D3DFMT_R5G6B5)
	{
		scr_rbits = 5;
		scr_rshift = 11;
		scr_gbits = 6;
		scr_gshift = 5;
		scr_bbits = 5;
		scr_bshift = 0;
		scr_pixbytes = 2;
	}
	else if (desc.Format == D3DFMT_X8R8G8B8 || desc.Format == D3DFMT_A8R8G8B8)
	{
		scr_rbits = 8;
		scr_rshift = 16;
		scr_gbits = 8;
		scr_gshift = 8;
		scr_bbits = 8;
		scr_bshift = 0;
		scr_pixbytes = 4;
	}
	else
	{
		con << "Invalid pixel format\n";
		Z_Free(dst);
		return NULL;
	}

	D3DLOCKED_RECT lrect;
	if (FAILED(surf->LockRect(&lrect, NULL, D3DLOCK_READONLY)))
	{
		Sys_Error("ReadScreen: Failed to lock screen");
	}

	rgb_t *pdst = (rgb_t*)dst;
	for (int j = 0; j < ScreenHeight; j++)
	{
		byte *psrc = (byte*)lrect.pBits + j * lrect.Pitch;
		for (int i = 0; i < ScreenWidth; i++)
		{
			pdst->r = byte((*(dword*)psrc >> scr_rshift) << (8 - scr_rbits));
			pdst->g = byte((*(dword*)psrc >> scr_gshift) << (8 - scr_gbits));
			pdst->b = byte((*(dword*)psrc >> scr_bshift) << (8 - scr_bbits));
			psrc += scr_pixbytes;
			pdst++;
		}
	}

	surf->UnlockRect();
	surf->Release();
#else
	DDSURFACEDESC2	ddsd;

	//	Lock surface
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_LPSURFACE | DDSD_PITCH | DDSD_PIXELFORMAT;
	if (RenderSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
		Sys_Error("ReadScreen: Failed to lock screen");

	//	Decode pixel format
	int scr_rbits = GetBits(ddsd.ddpfPixelFormat.dwRBitMask);
	int scr_rshift = GetShift(ddsd.ddpfPixelFormat.dwRBitMask);
	int scr_gbits = GetBits(ddsd.ddpfPixelFormat.dwGBitMask);
	int scr_gshift = GetShift(ddsd.ddpfPixelFormat.dwGBitMask);
	int scr_bbits = GetBits(ddsd.ddpfPixelFormat.dwBBitMask);
	int scr_bshift = GetShift(ddsd.ddpfPixelFormat.dwBBitMask);

	rgb_t *pdst = (rgb_t*)dst;
	for (int j = 0; j < ScreenHeight; j++)
	{
		byte *psrc = (byte*)ddsd.lpSurface + j * ddsd.lPitch;
		for (int i = 0; i < ScreenWidth; i++)
		{
			pdst->r = byte((*(dword*)psrc >> scr_rshift) << (8 - scr_rbits));
			pdst->g = byte((*(dword*)psrc >> scr_gshift) << (8 - scr_gbits));
			pdst->b = byte((*(dword*)psrc >> scr_bshift) << (8 - scr_bbits));
			psrc += PixelBytes;
			pdst++;
		}
	}

	RenderSurface->Unlock(NULL);
#endif

	*bpp = 24;
	*bot2top = false;
	return dst;
}

//==========================================================================
//
//	TDirect3DDrawer::SetPalette
//
//==========================================================================

void TDirect3DDrawer::SetPalette(int pnum)
{
	rgb_t *pal = (rgb_t*)W_CacheLumpName("playpal", PU_CACHE);

	pal += 256 * pnum;
	int cmax = MAX(MAX(pal[0].r, pal[0].g), pal[0].b);
	if (!cmax)
	{
		cl.cshifts[7] = 0;
	}
	else
	{
		cl.cshifts[7] = (cmax << 24) | ((255 * pal[0].r / cmax) << 16) |
			((255 * pal[0].g / cmax) << 8) | (255 * pal[0].b / cmax);
	}
	cl.prev_cshifts[7] = cl.cshifts[7];
}

//**************************************************************************
//
//	$Log$
//	Revision 1.13  2001/10/04 17:22:05  dj_jl
//	My overloaded matrix, beautification
//
//	Revision 1.12  2001/09/14 16:48:22  dj_jl
//	Switched to DirectX 8
//	
//	Revision 1.11  2001/09/12 17:31:27  dj_jl
//	Rectangle drawing and direct update for plugins
//	
//	Revision 1.10  2001/08/31 17:25:38  dj_jl
//	Anisotropy filtering
//	
//	Revision 1.9  2001/08/29 17:47:55  dj_jl
//	Added texture filtering variables
//	
//	Revision 1.8  2001/08/24 17:03:57  dj_jl
//	Added mipmapping, removed bumpmap test code
//	
//	Revision 1.7  2001/08/15 17:15:55  dj_jl
//	Drawer API changes, removed wipes
//	
//	Revision 1.6  2001/08/07 16:46:23  dj_jl
//	Added player models, skins and weapon
//	
//	Revision 1.5  2001/08/04 17:29:54  dj_jl
//	Fixed fog, beautification
//	
//	Revision 1.4  2001/08/01 17:40:09  dj_jl
//	Fixed check for sprite sorting, beautification
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
