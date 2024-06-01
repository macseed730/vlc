/*****************************************************************************
 * d3d11_fmt.c : D3D11 helper calls
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_charset.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#define COBJMACROS
#include <d3d11.h>
#include <assert.h>
#include <initguid.h>
#include <dxgi1_2.h>
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
# include <dxgidebug.h>
#endif

#include "d3d11_fmt.h"

#include <winapifamily.h>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define BUILD_FOR_UAP 0
#else
#define BUILD_FOR_UAP 1
#endif
#if defined(WINAPI_FAMILY)
# undef WINAPI_FAMILY
#endif
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#include <wbemidl.h>

#define D3D11_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of((pic_ctx), struct d3d11_pic_context, s)

picture_sys_d3d11_t *ActiveD3D11PictureSys(picture_t *pic)
{
    assert(pic->context != NULL);
    assert(pic->p_sys == NULL);
    struct d3d11_pic_context *pic_ctx = D3D11_PICCONTEXT_FROM_PICCTX(pic->context);
    return &pic_ctx->picsys;
}

void AcquireD3D11PictureSys(picture_sys_d3d11_t *p_sys)
{
    for (int i=0; i<DXGI_MAX_SHADER_VIEW; i++) {
        if (p_sys->renderSrc[i])
            ID3D11ShaderResourceView_AddRef(p_sys->renderSrc[i]);
        if (p_sys->texture[i])
            ID3D11Texture2D_AddRef(p_sys->texture[i]);
    }
    if (p_sys->processorInput)
        ID3D11VideoProcessorInputView_AddRef(p_sys->processorInput);
    if (p_sys->processorOutput)
        ID3D11VideoProcessorOutputView_AddRef(p_sys->processorOutput);
}

void ReleaseD3D11PictureSys(picture_sys_d3d11_t *p_sys)
{
    for (int i=0; i<DXGI_MAX_SHADER_VIEW; i++) {
        if (p_sys->renderSrc[i])
            ID3D11ShaderResourceView_Release(p_sys->renderSrc[i]);
        if (p_sys->texture[i])
            ID3D11Texture2D_Release(p_sys->texture[i]);
    }
    if (p_sys->processorInput)
        ID3D11VideoProcessorInputView_Release(p_sys->processorInput);
    if (p_sys->processorOutput)
        ID3D11VideoProcessorOutputView_Release(p_sys->processorOutput);
}

/* map texture planes to resource views */
int D3D11_AllocateResourceView(struct vlc_logger *obj, ID3D11Device *d3ddevice,
                              const d3d_format_t *format,
                              ID3D11Texture2D *p_texture[DXGI_MAX_SHADER_VIEW], UINT slice_index,
                              ID3D11ShaderResourceView *renderSrc[DXGI_MAX_SHADER_VIEW])
{
    HRESULT hr;
    int i;
    D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc = { 0 };
    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc(p_texture[0], &texDesc);
    assert(texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE);

    if (texDesc.ArraySize == 1)
    {
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        resviewDesc.Texture2D.MipLevels = 1;
    }
    else
    {
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        resviewDesc.Texture2DArray.MipLevels = -1;
        resviewDesc.Texture2DArray.ArraySize = 1;
        resviewDesc.Texture2DArray.FirstArraySlice = slice_index;
        assert(slice_index < texDesc.ArraySize);
    }
    for (i=0; i<DXGI_MAX_SHADER_VIEW; i++)
    {
        resviewDesc.Format = format->resourceFormat[i];
        if (resviewDesc.Format == DXGI_FORMAT_UNKNOWN)
            renderSrc[i] = NULL;
        else
        {
            hr = ID3D11Device_CreateShaderResourceView(d3ddevice, (ID3D11Resource*)p_texture[i], &resviewDesc, &renderSrc[i]);
            if (FAILED(hr)) {
                vlc_error(obj, "Could not Create the Texture ResourceView %d slice %d. (hr=0x%lX)", i, slice_index, hr);
                break;
            }
        }
    }

    if (i != DXGI_MAX_SHADER_VIEW)
    {
        while (--i >= 0)
        {
            ID3D11ShaderResourceView_Release(renderSrc[i]);
            renderSrc[i] = NULL;
        }
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void SetDriverString(vlc_object_t *obj, d3d11_device_t *d3d_dev, const WCHAR *szData)
{
    int wddm, d3d_features, revision, build;
    /* see https://docs.microsoft.com/en-us/windows-hardware/drivers/display/wddm-2-1-features#driver-versioning */
    if (swscanf(szData, TEXT("%d.%d.%d.%d"), &wddm, &d3d_features, &revision, &build) != 4)
    {
        msg_Warn(obj, "the adapter DriverVersion '%ls' doesn't match the expected format", szData);
        return;
    }
    d3d_dev->WDDM.wddm         = wddm;
    d3d_dev->WDDM.d3d_features = d3d_features;
    d3d_dev->WDDM.revision     = revision;
    d3d_dev->WDDM.build        = build;
    msg_Dbg(obj, "%s WDDM driver %d.%d.%d.%d", DxgiVendorStr(d3d_dev->adapterDesc.VendorId), wddm, d3d_features, revision, build);
    if (d3d_dev->adapterDesc.VendorId == GPU_MANUFACTURER_INTEL && revision >= 100)
    {
        /* new Intel driver format */
        d3d_dev->WDDM.build += (revision - 100) * 1000;
    }
}

static void D3D11_GetDriverVersion(vlc_object_t *obj, d3d11_device_t *d3d_dev)
{
    HRESULT hr;
    IWbemLocator *pLoc = NULL;
    IWbemServices *pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;

    BSTR bRootNamespace = SysAllocString(L"ROOT\\CIMV2");
    BSTR bWQL = SysAllocString(L"WQL");

    WCHAR lookup[256];
    _snwprintf(lookup, ARRAY_SIZE(lookup),
               L"SELECT * FROM Win32_VideoController WHERE PNPDeviceID LIKE 'PCI\\\\VEN_%04X&DEV_%04X&SUBSYS_%08X&REV_%02X%%'",
               d3d_dev->adapterDesc.VendorId, d3d_dev->adapterDesc.DeviceId,
               d3d_dev->adapterDesc.SubSysId, d3d_dev->adapterDesc.Revision);
    BSTR bVideoController = SysAllocString(lookup);

    hr =  CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        msg_Dbg(obj, "Unable to initialize COM library");
        return;
    }

    MULTI_QI res = { 0 };
    res.pIID = &IID_IWbemLocator;
#if !BUILD_FOR_UAP
    hr = CoCreateInstanceEx(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, 0,
                                1, &res);
#else // BUILD_FOR_UAP
    hr = CoCreateInstanceFromApp(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, 0,
                                1, &res);
#endif // BUILD_FOR_UAP
    if (FAILED(hr) || FAILED(res.hr))
    {
        msg_Dbg(obj, "Failed to create IWbemLocator object");
        goto done;
    }
    pLoc = (IWbemLocator *)res.pItf;

    hr = IWbemLocator_ConnectServer(pLoc, bRootNamespace,
                                    NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    if (FAILED(hr))
    {
        msg_Dbg(obj, "Could not connect to namespace");
        goto done;
    }

#if !BUILD_FOR_UAP
    hr = CoSetProxyBlanket(
        (IUnknown*)pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );
    if (FAILED(hr))
    {
        msg_Dbg(obj, "Could not set proxy blanket");
        goto done;
    }
#endif // !BUILD_FOR_UAP

    hr = IWbemServices_ExecQuery( pSvc, bWQL, bVideoController,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    if (FAILED(hr) || !pEnumerator)
    {
        msg_Dbg(obj, "Query for Win32_VideoController failed");
        goto done;
    }

    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;
    hr = IEnumWbemClassObject_Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn);
    if (!uReturn)
    {
        msg_Warn(obj, "failed to find the device");
        goto done;
    }

    VARIANT vtProp;
    VariantInit(&vtProp);

    hr = IWbemClassObject_Get(pclsObj, L"DriverVersion", 0, &vtProp, 0, 0);
    if ( FAILED( hr ) )
    {
        msg_Warn(obj, "failed to read the driver version");
        goto done;
    }

    SetDriverString(obj, d3d_dev, vtProp.bstrVal);

    VariantClear(&vtProp);
    IWbemClassObject_Release(pclsObj);

done:
    SysFreeString(bRootNamespace);
    SysFreeString(bWQL);
    SysFreeString(bVideoController);
    if (pEnumerator)
        IEnumWbemClassObject_Release(pEnumerator);
    if (pSvc)
        IWbemServices_Release(pSvc);
    if (pLoc)
        IWbemLocator_Release(pLoc);
    CoUninitialize();
}

typedef struct
{
#ifndef VLC_WINSTORE_APP
    HINSTANCE                 hdll;         /* handle of the opened d3d11 dll */
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE                 dxgidebug_dll;
    HRESULT (WINAPI * pf_DXGIGetDebugInterface)(const GUID *riid, void **ppDebug);
#endif
#endif
} d3d11_handle_t;

typedef struct {
    struct {
        void                            *opaque;
        libvlc_video_output_cleanup_cb  cleanupDeviceCb;
    } external;

    d3d11_handle_t                      hd3d;
    d3d11_decoder_device_t              dec_device;
} d3d11_decoder_device;

static int D3D11_Create(vlc_object_t *obj, d3d11_handle_t *hd3d)
{
#ifndef VLC_WINSTORE_APP
    hd3d->hdll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!hd3d->hdll)
    {
        msg_Warn(obj, "cannot load d3d11.dll, aborting");
        return VLC_EGENERIC;
    }

# if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    hd3d->dxgidebug_dll = NULL;
    hd3d->pf_DXGIGetDebugInterface = NULL;
    if (IsDebuggerPresent())
    {
        hd3d->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
        if (hd3d->dxgidebug_dll)
        {
            hd3d->pf_DXGIGetDebugInterface =
                    (void *)GetProcAddress(hd3d->dxgidebug_dll, "DXGIGetDebugInterface");
            if (unlikely(!hd3d->pf_DXGIGetDebugInterface))
            {
                FreeLibrary(hd3d->dxgidebug_dll);
                hd3d->dxgidebug_dll = NULL;
            }
        }
    }
# endif // !NDEBUG && HAVE_DXGIDEBUG_H
#endif // !VLC_WINSTORE_APP
    return VLC_SUCCESS;
}

static void D3D11_Destroy(d3d11_handle_t *hd3d)
{
#ifndef VLC_WINSTORE_APP
    if (hd3d->hdll)
        FreeLibrary(hd3d->hdll);

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (hd3d->dxgidebug_dll)
        FreeLibrary(hd3d->dxgidebug_dll);
#endif
#endif
}

void D3D11_ReleaseDevice(d3d11_decoder_device_t *dev_sys)
{
    d3d11_decoder_device *sys = container_of(dev_sys, d3d11_decoder_device, dec_device);
    d3d11_device_t *d3d_dev = &dev_sys->d3d_dev;
    if (d3d_dev->d3dcontext)
    {
        ID3D11DeviceContext_Flush(d3d_dev->d3dcontext);
        ID3D11DeviceContext_Release(d3d_dev->d3dcontext);
        d3d_dev->d3dcontext = NULL;
    }
    if (d3d_dev->d3ddevice)
    {
        ID3D11Device_Release(d3d_dev->d3ddevice);
        d3d_dev->d3ddevice = NULL;
    }
#if defined(HAVE_ID3D11VIDEODECODER)
    if( d3d_dev->mutex_owner && d3d_dev->context_mutex != INVALID_HANDLE_VALUE )
    {
        CloseHandle( d3d_dev->context_mutex );
        d3d_dev->context_mutex = INVALID_HANDLE_VALUE;
    }
#endif

    if ( sys->external.cleanupDeviceCb )
        sys->external.cleanupDeviceCb( sys->external.opaque );

    D3D11_LogResources( &sys->dec_device );
    D3D11_Destroy( &sys->hd3d );
}

static HRESULT D3D11_CreateDeviceExternal(vlc_object_t *obj, ID3D11DeviceContext *d3d11ctx,
                                          HANDLE context_lock, d3d11_device_t *out)
{
    if (unlikely(d3d11ctx == NULL))
    {
        msg_Err(obj, "missing external ID3D11DeviceContext");
        return S_FALSE;
    }

    HRESULT hr;
    ID3D11DeviceContext_GetDevice( d3d11ctx, &out->d3ddevice );

    if (!(ID3D11Device_GetCreationFlags(out->d3ddevice) & D3D11_CREATE_DEVICE_VIDEO_SUPPORT))
    {
        msg_Warn(obj, "the provided D3D11 device doesn't support decoding");
    }

    IDXGIAdapter *pAdapter = D3D11DeviceAdapter(out->d3ddevice);
    if (unlikely(!pAdapter))
    {
        msg_Warn(obj, "can't get adapter from device %p", (void*)out->d3ddevice);
        ID3D11Device_Release(out->d3ddevice);
        out->d3ddevice = NULL;
        return E_FAIL;
    }
    hr = IDXGIAdapter_GetDesc(pAdapter, &out->adapterDesc);
    IDXGIAdapter_Release(pAdapter);
    if (FAILED(hr))
        msg_Warn(obj, "can't get adapter description");

    ID3D11DeviceContext_AddRef( d3d11ctx );
    out->d3dcontext = d3d11ctx;
    out->mutex_owner = false;
    out->feature_level = ID3D11Device_GetFeatureLevel(out->d3ddevice );

    out->context_mutex = context_lock;
    if (context_lock == NULL || context_lock == INVALID_HANDLE_VALUE)
    {
        msg_Warn(obj, "external ID3D11DeviceContext mutex not provided, using internal one");
        out->mutex_owner = true;
        out->context_mutex = CreateMutexEx( NULL, NULL, 0, SYNCHRONIZE );
    }

    D3D11_GetDriverVersion(obj, out);
    return S_OK;
}

static HRESULT CreateDevice(vlc_object_t *obj, d3d11_handle_t *hd3d,
                            IDXGIAdapter *adapter,
                            bool hw_decoding, d3d11_device_t *out)
{
#ifndef VLC_WINSTORE_APP
# define D3D11CreateDevice(a,b,c,d,e,f,g,h,i,j)   pf_CreateDevice(a,b,c,d,e,f,g,h,i,j)
    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(hd3d->hdll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(obj, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        return E_NOINTERFACE;
    }
#endif /* VLC_WINSTORE_APP */

    HRESULT hr = E_NOTIMPL;
    UINT creationFlags = 0;

    if (hw_decoding || !obj->force)
        creationFlags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

#if !defined(NDEBUG)
    if (IsDebuggerPresent())
    {
        HINSTANCE sdklayer_dll = LoadLibraryW(L"d3d11_1sdklayers.dll");
        if (sdklayer_dll) {
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
            FreeLibrary(sdklayer_dll);
        }
    }
#endif

    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
#if 0 /* ifndef NDEBUG */
        D3D_DRIVER_TYPE_REFERENCE,
#endif
    };

    static D3D_FEATURE_LEVEL D3D11_features[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
    };

    for (UINT driver = 0; driver < (adapter ? 1 : ARRAY_SIZE(driverAttempts)); driver++) {
        hr = D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : driverAttempts[driver],
                               NULL, creationFlags,
                               D3D11_features, ARRAY_SIZE(D3D11_features), D3D11_SDK_VERSION,
                               &out->d3ddevice, &out->feature_level, &out->d3dcontext);
        if (SUCCEEDED(hr)) {
            msg_Dbg(obj, "Created the D3D11 device type %d level %x.",
                    driverAttempts[driver], out->feature_level);
            if (adapter != NULL)
                hr = IDXGIAdapter_GetDesc(adapter, &out->adapterDesc);
            else
            {
                IDXGIAdapter *adap = D3D11DeviceAdapter(out->d3ddevice);
                if (adap == NULL)
                    hr = E_FAIL;
                else
                {
                    hr = IDXGIAdapter_GetDesc(adap, &out->adapterDesc);
                    IDXGIAdapter_Release(adap);
                }
            }
            if (hr)
                msg_Warn(obj, "can't get adapter description");

            D3D11_GetDriverVersion( obj, out );
            /* we can work with legacy levels but only if forced */
            if ( obj->force || out->feature_level >= D3D_FEATURE_LEVEL_11_0 )
                break;
            msg_Warn(obj, "Incompatible feature level %x", out->feature_level);
            ID3D11DeviceContext_Release(out->d3dcontext);
            ID3D11Device_Release(out->d3ddevice);
            out->d3dcontext = NULL;
            out->d3ddevice = NULL;
            hr = E_NOTIMPL;
        }
    }

    if (hw_decoding && SUCCEEDED(hr))
    {
        out->context_mutex = CreateMutexEx( NULL, NULL, 0, SYNCHRONIZE );
        out->mutex_owner = true;
    }
    else
        out->context_mutex = INVALID_HANDLE_VALUE;

    return hr;
}

d3d11_decoder_device_t *(D3D11_CreateDevice)(vlc_object_t *obj,
                                      IDXGIAdapter *adapter,
                                      bool hw_decoding, bool forced)
{
    d3d11_decoder_device *sys = vlc_obj_malloc(obj, sizeof(*sys));
    if (unlikely(sys==NULL))
        return NULL;

    int ret = D3D11_Create(obj, &sys->hd3d);
    if (ret != VLC_SUCCESS)
    {
        vlc_obj_free( obj, sys );
        return NULL;
    }

    sys->external.cleanupDeviceCb = NULL;
    HRESULT hr = E_FAIL;
    {
        libvlc_video_engine_t engineType = var_InheritInteger( obj, "vout-cb-type" );
        libvlc_video_output_setup_cb setupDeviceCb = NULL;
        if (engineType == libvlc_video_engine_d3d11)
            setupDeviceCb = var_InheritAddress( obj, "vout-cb-setup" );
        if ( setupDeviceCb != NULL)
        {
            /* decoder device coming from the external app */
            sys->external.opaque          = var_InheritAddress( obj, "vout-cb-opaque" );
            sys->external.cleanupDeviceCb = var_InheritAddress( obj, "vout-cb-cleanup" );
            libvlc_video_setup_device_cfg_t cfg = {
                .hardware_decoding = true, /* always favor hardware decoding */
            };
            libvlc_video_setup_device_info_t out = { .d3d11.device_context = NULL };
            if (!setupDeviceCb( &sys->external.opaque, &cfg, &out ))
            {
                if (sys->external.cleanupDeviceCb)
                    sys->external.cleanupDeviceCb( sys->external.opaque );
                goto error;
            }
            hr = D3D11_CreateDeviceExternal(obj, out.d3d11.device_context, out.d3d11.context_mutex,
                                            &sys->dec_device.d3d_dev);
        }
        else if ( engineType == libvlc_video_engine_disable ||
                  engineType == libvlc_video_engine_d3d11 )
        {
            /* internal decoder device */
#ifndef VLC_WINSTORE_APP
            if (!forced)
            {
                /* Allow using D3D11 automatically starting from Windows 8.1 */
                bool isWin81OrGreater = false;
                HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
                if (likely(hKernel32 != NULL))
                    isWin81OrGreater = GetProcAddress(hKernel32, "IsProcessCritical") != NULL;
                if (!isWin81OrGreater)
                    goto error;
            }
#endif /* !VLC_WINSTORE_APP */

            hr = CreateDevice( obj, &sys->hd3d, adapter, hw_decoding, &sys->dec_device.d3d_dev );
        }
        else
            goto error;
    }

error:
    if (FAILED(hr))
    {
        D3D11_LogResources( &sys->dec_device );
        D3D11_Destroy(&sys->hd3d);
        vlc_obj_free( obj, sys );
        return NULL;
    }
    return &sys->dec_device;
}

IDXGIAdapter *D3D11DeviceAdapter(ID3D11Device *d3ddev)
{
    IDXGIDevice *pDXGIDevice;
    void *pv;
    HRESULT hr = ID3D11Device_QueryInterface(d3ddev, &IID_IDXGIDevice, &pv);
    if (FAILED(hr)) {
        return NULL;
    }
    pDXGIDevice = pv;

    IDXGIAdapter *p_adapter;
    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &p_adapter);
    IDXGIDevice_Release(pDXGIDevice);
    if (FAILED(hr)) {
        return NULL;
    }
    return p_adapter;
}

bool isXboxHardware(const d3d11_device_t *d3ddev)
{
    bool result = false;

    if (d3ddev->adapterDesc.VendorId == 0 &&
        d3ddev->adapterDesc.DeviceId == 0 &&
        !wcscmp(L"ROOT\\SraKmd\\0000", d3ddev->adapterDesc.Description))
        result = true;

    return result;
}

/**
 * Performs a check on each value of the WDDM version. Any value that is OK will
 * consider the driver valid (OR on each value)
 */
int D3D11CheckDriverVersion(const d3d11_device_t *d3d_dev, UINT vendorId, const struct wddm_version *min_ver)
{
    if (vendorId && d3d_dev->adapterDesc.VendorId != vendorId)
        return VLC_SUCCESS;

    if (min_ver->wddm)
    {
        if (d3d_dev->WDDM.wddm > min_ver->wddm)
            return VLC_SUCCESS;
        if (d3d_dev->WDDM.wddm != min_ver->wddm)
            return VLC_EGENERIC;
    }
    if (min_ver->d3d_features)
    {
        if (d3d_dev->WDDM.d3d_features > min_ver->d3d_features)
            return VLC_SUCCESS;
        if (d3d_dev->WDDM.d3d_features != min_ver->d3d_features)
            return VLC_EGENERIC;
    }
    if (min_ver->revision)
    {
        if (d3d_dev->WDDM.revision > min_ver->revision)
            return VLC_SUCCESS;
        if (d3d_dev->WDDM.revision != min_ver->revision)
            return VLC_EGENERIC;
    }
    if (min_ver->build)
    {
        if (d3d_dev->WDDM.build > min_ver->build)
            return VLC_SUCCESS;
        if (d3d_dev->WDDM.build != min_ver->build)
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/* test formats that should work but sometimes have issues on some platforms */
static bool CanReallyUseFormat(vlc_object_t *obj, d3d11_device_t *d3d_dev,
                               vlc_fourcc_t i_chroma, DXGI_FORMAT dxgi)
{
    bool result = true;
    if (dxgi == DXGI_FORMAT_UNKNOWN)
        return true;

    if (is_d3d11_opaque(i_chroma))
        return true;

    ID3D11Texture2D *texture = NULL;
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; //D3D11_RESOURCE_MISC_SHARED;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    texDesc.ArraySize = 1;
    texDesc.Format = dxgi;
    texDesc.Height = 144;
    texDesc.Width = 176;
    HRESULT hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &texture );
    if (FAILED(hr))
    {
        msg_Dbg(obj, "cannot allocate a writable texture type %s. (hr=0x%lX)", DxgiFormatToStr(dxgi), hr);
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource*)texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        msg_Err(obj, "The texture type %s cannot be mapped. (hr=0x%lX)", DxgiFormatToStr(dxgi), hr);
        result = false;
        goto done;
    }
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource*)texture, 0);

    if (dxgi == DXGI_FORMAT_YUY2)
    {
        const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( i_chroma );
        if( !p_chroma_desc )
        {
            msg_Err(obj, "No pixel format for %4.4s", (const char*)&i_chroma);
            result = false;
            goto done;
        }

        if (mappedResource.RowPitch >= 2 * (texDesc.Width * p_chroma_desc->p[0].w.num / p_chroma_desc->p[0].w.den * p_chroma_desc->pixel_size))
        {
            msg_Err(obj, "Bogus %4.4s pitch detected type %s. %d should be %d", (const char*)&i_chroma,
                          DxgiFormatToStr(dxgi), mappedResource.RowPitch,
                          (texDesc.Width * p_chroma_desc->p[0].w.num / p_chroma_desc->p[0].w.den * p_chroma_desc->pixel_size));
            result = false;
            goto done;
        }

    }
done:
    ID3D11Texture2D_Release(texture);

    return result;
}

bool D3D11_DeviceSupportsFormat(d3d11_device_t *d3d_dev, DXGI_FORMAT format, UINT supportFlags)
{
    UINT i_formatSupport;
    return SUCCEEDED( ID3D11Device_CheckFormatSupport(d3d_dev->d3ddevice, format,
                                                      &i_formatSupport) )
            && ( i_formatSupport & supportFlags ) == supportFlags;
}


const d3d_format_t *(FindD3D11Format)(vlc_object_t *o,
                                    d3d11_device_t *d3d_dev,
                                    vlc_fourcc_t i_src_chroma,
                                    int rgb_yuv,
                                    uint8_t bits_per_channel,
                                    uint8_t widthDenominator,
                                    uint8_t heightDenominator,
                                    int cpu_gpu,
                                    UINT supportFlags)
{
    supportFlags |= D3D11_FORMAT_SUPPORT_TEXTURE2D;
    for (const d3d_format_t *output_format = DxgiGetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if (i_src_chroma && i_src_chroma != output_format->fourcc)
            continue;
        if (bits_per_channel && bits_per_channel > output_format->bitsPerChannel)
            continue;
        int cpu_gpu_fmt = is_d3d11_opaque(output_format->fourcc) ? DXGI_CHROMA_GPU : DXGI_CHROMA_CPU;
        if ((cpu_gpu & cpu_gpu_fmt)==0)
            continue;
        int format = vlc_fourcc_IsYUV(output_format->fourcc) ? DXGI_YUV_FORMAT : DXGI_RGB_FORMAT;
        if ((rgb_yuv & format)==0)
            continue;
        if (widthDenominator && widthDenominator < output_format->widthDenominator)
            continue;
        if (heightDenominator && heightDenominator < output_format->heightDenominator)
            continue;

        DXGI_FORMAT textureFormat;
        if (output_format->formatTexture == DXGI_FORMAT_UNKNOWN)
            textureFormat = output_format->resourceFormat[0];
        else
            textureFormat = output_format->formatTexture;

        if( D3D11_DeviceSupportsFormat( d3d_dev, textureFormat, supportFlags ) &&
            CanReallyUseFormat(o, d3d_dev, output_format->fourcc, output_format->formatTexture) )
            return output_format;
    }
    return NULL;
}

void D3D11_PictureAttach(picture_t *pic, ID3D11Texture2D *slicedTexture, const d3d_format_t *cfg)
{
    struct d3d11_pic_context *pic_ctx = D3D11_PICCONTEXT_FROM_PICCTX(pic->context);
    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc(slicedTexture, &texDesc);

    if (texDesc.CPUAccessFlags != 0)
    {
        const video_format_t *fmt = &pic->format;

        const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( fmt->i_chroma );
        if( !p_chroma_desc )
            return;

        for( unsigned i = 0; i < p_chroma_desc->plane_count; i++ )
        {
            plane_t *p = &pic->p[i];

            p->i_lines         = fmt->i_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
            p->i_visible_lines = fmt->i_visible_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
            p->i_pitch         = fmt->i_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
            p->i_visible_pitch = fmt->i_visible_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
            p->i_pixel_pitch   = p_chroma_desc->pixel_size;
        }
    }

    for (unsigned plane = 0; plane < DXGI_MAX_SHADER_VIEW; plane++)
    {
        if (!cfg->resourceFormat[plane])
        {
            pic_ctx->picsys.texture[plane] = NULL;
        }
        else
        {
            pic_ctx->picsys.texture[plane] = slicedTexture;
            ID3D11Texture2D_AddRef(slicedTexture);
        }
    }
}

#undef AllocateTextures
int AllocateTextures( vlc_object_t *obj, d3d11_device_t *d3d_dev,
                      const d3d_format_t *cfg, const video_format_t *fmt, bool shared,
                      ID3D11Texture2D *textures[],
                      plane_t out_planes[] )
{
    plane_t planes[PICTURE_PLANE_MAX];
    unsigned plane, plane_count;
    HRESULT hr;
    ID3D11Texture2D *slicedTexture = NULL;
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    if (shared)
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.ArraySize = 1;
    if (is_d3d11_opaque(fmt->i_chroma)) {
        texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = 0;
    } else {
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }

    const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( fmt->i_chroma );
    if( !p_chroma_desc )
        return VLC_EGENERIC;

    if (cfg->formatTexture == DXGI_FORMAT_UNKNOWN) {
        if (p_chroma_desc->plane_count == 0)
        {
            msg_Dbg(obj, "failed to get the pixel format planes for %4.4s", (char *)&fmt->i_chroma);
            return VLC_EGENERIC;
        }
        assert(p_chroma_desc->plane_count <= DXGI_MAX_SHADER_VIEW);
        plane_count = p_chroma_desc->plane_count;

        texDesc.Format = cfg->resourceFormat[0];
        assert(cfg->resourceFormat[1] == cfg->resourceFormat[0]);
        assert(cfg->resourceFormat[2] == cfg->resourceFormat[0]);

    } else {
        plane_count = __MAX(1, p_chroma_desc->plane_count);
        texDesc.Format = cfg->formatTexture;
        texDesc.Height = fmt->i_height;
        texDesc.Width = fmt->i_width;

        hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &slicedTexture );
        if (FAILED(hr)) {
            msg_Err(obj, "CreateTexture2D failed. (hr=0x%lX)", hr);
            goto error;
        }
    }
    for( unsigned i = 0; i < p_chroma_desc->plane_count; i++ )
    {
        plane_t *p = &planes[i];

        p->i_lines         = fmt->i_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
        p->i_visible_lines = fmt->i_visible_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
        p->i_pitch         = fmt->i_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
        p->i_visible_pitch = fmt->i_visible_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
        p->i_pixel_pitch   = p_chroma_desc->pixel_size;
    }

    for (plane = 0; plane < plane_count; plane++)
    {
        if (slicedTexture) {
            textures[plane] = slicedTexture;
            ID3D11Texture2D_AddRef(slicedTexture);
        } else {
            texDesc.Height = planes[plane].i_lines;
            texDesc.Width  = planes[plane].i_pitch / p_chroma_desc->pixel_size;
            hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &textures[plane] );
            if (FAILED(hr)) {
                msg_Err(obj, "CreateTexture2D failed for plane %d. (hr=0x%lX)", plane, hr);
                goto error;
            }
        }
    }
    if (out_planes)
        for (plane = 0; plane < p_chroma_desc->plane_count; plane++)
        {
            out_planes[plane] = planes[plane];
        }
    for (; plane < DXGI_MAX_SHADER_VIEW; plane++) {
        if (!cfg->resourceFormat[plane])
            textures[plane] = NULL;
        else
        {
            textures[plane] = textures[0];
            ID3D11Texture2D_AddRef(textures[plane]);
        }
    }

    if (slicedTexture)
        ID3D11Texture2D_Release(slicedTexture);
    return VLC_SUCCESS;
error:
    if (slicedTexture)
        ID3D11Texture2D_Release(slicedTexture);
    return VLC_EGENERIC;
}

void D3D11_LogResources(d3d11_decoder_device_t *dev_sys)
{
#ifndef VLC_WINSTORE_APP
# if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    d3d11_decoder_device *sys = container_of(dev_sys, d3d11_decoder_device, dec_device);
    d3d11_handle_t *hd3d = &sys->hd3d;
    if (hd3d->pf_DXGIGetDebugInterface)
    {
        void *pv;
        if (SUCCEEDED(hd3d->pf_DXGIGetDebugInterface(&IID_IDXGIDebug, &pv)))
        {
            IDXGIDebug *pDXGIDebug = pv;
            IDXGIDebug_ReportLiveObjects(pDXGIDebug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            IDXGIDebug_Release(pDXGIDebug);
        }
    }
# endif
#endif
}

const struct vlc_video_context_operations d3d11_vctx_ops = {
    NULL,
};

vlc_video_context *D3D11CreateVideoContext(vlc_decoder_device *dec_dev, DXGI_FORMAT vctx_fmt)
{
    vlc_video_context *vctx = vlc_video_context_Create( dec_dev, VLC_VIDEO_CONTEXT_D3D11VA,
                                          sizeof(d3d11_video_context_t), &d3d11_vctx_ops );
    if (unlikely(vctx == NULL))
        return NULL;

    d3d11_video_context_t *priv = GetD3D11ContextPrivate(vctx);
    priv->format = vctx_fmt;
    return vctx;
}

void d3d11_pic_context_destroy(picture_context_t *ctx)
{
    struct d3d11_pic_context *pic_ctx = D3D11_PICCONTEXT_FROM_PICCTX(ctx);
    ReleaseD3D11PictureSys(&pic_ctx->picsys);
    if (pic_ctx->picsys.sharedHandle != INVALID_HANDLE_VALUE && pic_ctx->picsys.ownHandle)
        CloseHandle(pic_ctx->picsys.sharedHandle);
    free(pic_ctx);
}

picture_context_t *d3d11_pic_context_copy(picture_context_t *ctx)
{
    struct d3d11_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    *pic_ctx = *D3D11_PICCONTEXT_FROM_PICCTX(ctx);
    vlc_video_context_Hold(pic_ctx->s.vctx);
    AcquireD3D11PictureSys(&pic_ctx->picsys);
    return &pic_ctx->s;
}

picture_t *D3D11_AllocPicture(vlc_object_t *obj,
                              const video_format_t *fmt, vlc_video_context *vctx_out,
                              bool shared, const d3d_format_t *cfg)
{
    if (unlikely(cfg == NULL))
        return NULL;

    struct d3d11_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        return NULL;
    pic_ctx->picsys.sharedHandle = INVALID_HANDLE_VALUE;

    picture_t *pic = picture_NewFromFormat( fmt );
    if (unlikely(pic == NULL))
    {
        free(pic_ctx);
        return NULL;
    }

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext(vctx_out);
    if (AllocateTextures(obj, &dev_sys->d3d_dev, cfg,
                         fmt, shared, pic_ctx->picsys.texture, NULL) != VLC_SUCCESS)
    {
        picture_Release(pic);
        free(pic_ctx);
        return NULL;
    }

    D3D11_AllocateResourceView(vlc_object_logger(obj), dev_sys->d3d_dev.d3ddevice, cfg, pic_ctx->picsys.texture, 0, pic_ctx->picsys.renderSrc);

    if (shared)
    {
        HRESULT hr;
        void *pv;
        IDXGIResource1 *sharedResource;
        hr = ID3D11Texture2D_QueryInterface(pic_ctx->picsys.texture[0], &IID_IDXGIResource1,&pv);
        if (likely(SUCCEEDED(hr)))
        {
            sharedResource = pv;
            IDXGIResource1_CreateSharedHandle(sharedResource, NULL,
                                              DXGI_SHARED_RESOURCE_READ/*|DXGI_SHARED_RESOURCE_WRITE*/,
                                              NULL, &pic_ctx->picsys.sharedHandle);
            pic_ctx->picsys.ownHandle = true;
            IDXGIResource1_Release(sharedResource);
        }
    }

    pic_ctx->s = (picture_context_t) {
        d3d11_pic_context_destroy, d3d11_pic_context_copy,
        vlc_video_context_Hold(vctx_out),
    };
    pic->context = &pic_ctx->s;
    return pic;
}
