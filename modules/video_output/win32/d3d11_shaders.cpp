/*****************************************************************************
 * d3d11_shaders.c: Direct3D11 Shaders
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#include <cassert>

#include "d3d11_shaders.h"
#include "d3d_dynamic_shader.h"

using Microsoft::WRL::ComPtr;

HRESULT (D3D11_CompilePixelShaderBlob)(vlc_object_t *o, const d3d_shader_compiler_t *compiler,
                                   d3d11_device_t *d3d_dev,
                                   const display_info_t *display,
                                   video_transfer_func_t transfer,
                                   bool src_full_range,
                                   const d3d11_quad_t *quad, d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET])
{
    size_t shader_views[DXGI_MAX_RENDER_TARGET];
    return D3D_CompilePixelShader(o, compiler, d3d_dev->feature_level,
                                  display, transfer,
                                  src_full_range, quad->generic.textureFormat, pPSBlob, shader_views);
}

HRESULT D3D11_SetQuadPixelShader(vlc_object_t *o, d3d11_device_t *d3d_dev,
                                bool sharp,
                                d3d11_quad_t *quad, d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET])
{
    D3D11_SAMPLER_DESC sampDesc = { };
    sampDesc.Filter = sharp ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr;
    hr = d3d_dev->d3ddevice->CreateSamplerState(&sampDesc, &quad->SamplerStates[0]);
    if (FAILED(hr)) {
        msg_Err(o, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
        return hr;
    }

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = d3d_dev->d3ddevice->CreateSamplerState(&sampDesc, &quad->SamplerStates[1]);
    if (FAILED(hr)) {
        msg_Err(o, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
        quad->SamplerStates[0].Reset();
        return hr;
    }

    hr = d3d_dev->d3ddevice->CreatePixelShader(
                                        pPSBlob[0].buffer, pPSBlob[0].buf_size,
                                        NULL, &quad->d3dpixelShader[0]);

    D3D_ShaderBlobRelease(&pPSBlob[0]);

    if (pPSBlob[1].buffer)
    {
        hr = d3d_dev->d3ddevice->CreatePixelShader(
                                            pPSBlob[1].buffer, pPSBlob[1].buf_size,
                                            NULL, &quad->d3dpixelShader[1]);

        D3D_ShaderBlobRelease(&pPSBlob[1]);
    }
    return hr;
}

HRESULT D3D11_CreateRenderTargets( d3d11_device_t *d3d_dev, ID3D11Resource *texture,
                                   const d3d_format_t *cfg, ComPtr<ID3D11RenderTargetView> output[DXGI_MAX_RENDER_TARGET] )
{
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    for (size_t i=0; i<DXGI_MAX_SHADER_VIEW; i++)
    {
        if (cfg->resourceFormat[i])
        {
            renderTargetViewDesc.Format = cfg->resourceFormat[i];
            HRESULT hr = d3d_dev->d3ddevice->CreateRenderTargetView(texture,
                                                             &renderTargetViewDesc, &output[i]);
            if (FAILED(hr))
            {
                return hr;
            }
        }
    }
    return S_OK;
}

void D3D11_ClearRenderTargets(d3d11_device_t *d3d_dev, const d3d_format_t *cfg,
                              ComPtr<ID3D11RenderTargetView> targets[DXGI_MAX_RENDER_TARGET])
{
    union DXGI_Color black[DXGI_MAX_RENDER_TARGET];
    size_t colorCount[DXGI_MAX_RENDER_TARGET];
    DXGI_GetBlackColor(cfg, black, colorCount);

    if (colorCount[0])
    {
        d3d_dev->d3dcontext->ClearRenderTargetView(targets[0].Get(), black[0].array);
    }
    if (colorCount[1])
    {
        d3d_dev->d3dcontext->ClearRenderTargetView(targets[1].Get(), black[1].array);
    }
}

HRESULT (D3D11_CreateVertexShader)(vlc_object_t *obj, d3d_shader_blob *pVSBlob,
                                   d3d11_device_t *d3d_dev, d3d11_vertex_shader_t *output)
{
    HRESULT hr;
    hr = d3d_dev->d3ddevice->CreateVertexShader(pVSBlob->buffer, pVSBlob->buf_size,
                                                NULL, &output->shader);

    if(FAILED(hr)) {
        msg_Err(obj, "Failed to create the flat vertex shader. (hr=0x%lX)", hr);
        goto error;
    }

    // must match d3d_vertex_t
    static D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = d3d_dev->d3ddevice->CreateInputLayout(layout, 2, pVSBlob->buffer,
                                    pVSBlob->buf_size, &output->layout);

    if(FAILED(hr)) {
        msg_Err(obj, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
        goto error;
    }

error:
    D3D_ShaderBlobRelease(pVSBlob);
    return hr;
}

void D3D11_ReleaseVertexShader(d3d11_vertex_shader_t *shader)
{
    shader->layout.Reset();
    shader->shader.Reset();
}

HRESULT D3D11_CompileVertexShaderBlob(vlc_object_t *obj, const d3d_shader_compiler_t *compiler,
                                      d3d11_device_t *d3d_dev, bool flat, d3d_shader_blob *pVSBlob)
{
    return D3D_CompileVertexShader(obj, compiler, d3d_dev->feature_level, flat, pVSBlob);
}
