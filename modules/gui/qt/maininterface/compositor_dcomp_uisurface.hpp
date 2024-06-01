/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef COMPOSITOR_DCOMP_UISURFACE_H
#define COMPOSITOR_DCOMP_UISURFACE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <memory>

#include <vlc_common.h>
#include <vlc_interface.h> /* intf_thread_t */

#include <windows.h>

#define D3D11_NO_HELPERS  // avoid tons of warnings
#include <d3d11_1.h>
#include <dcomp.h>
#include <wrl.h>

#include <qconfig.h>
//we link statically with ANGLE
#if defined(QT_STATIC) && !defined(KHRONOS_STATIC)
# define KHRONOS_STATIC 1
#endif

#include <QObject>
#include <QApplication>
#include <QBasicTimer>
#include <QMainWindow>
#include <QQuickWindow>
#include <QQuickView>
#include <QQuickItem>
#include <QSGRectangleNode>
#include <QQuickRenderControl>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLTexture>

#include <QtANGLE/EGL/egl.h>
#include <QtANGLE/EGL/eglext.h>
#include <QtPlatformHeaders/QEGLNativeContext>

#include "qt.hpp"
#include "compositor.hpp"
#include "compositor_accessibility.hpp"

namespace vlc {

class CompositorOffscreenWindow;

//on screen window
class DCompRenderWindow : public QWindow, public AccessibleRenderWindow
{
    Q_OBJECT
public:
#ifndef QT_NO_ACCESSIBILITY
    QAccessibleInterface* accessibleRoot() const override;
#endif

    void setOffscreenWindow(CompositorOffscreenWindow* window);
    QQuickWindow* getOffscreenWindow() const override;

private:
    CompositorOffscreenWindow* m_offscreenWindow = nullptr;
};


class CompositorDCompositionRenderControl : public QQuickRenderControl
{
    Q_OBJECT
public:
    CompositorDCompositionRenderControl(QWindow *w)
        : m_window(w)
    {
    }

    QWindow *renderWindow(QPoint *) override
    {
        return m_window;
    }

private:
    QWindow *m_window;
};

class CompositorDCompositionUISurface : public QObject, public CompositorVideo::QmlUISurface
{
    Q_OBJECT
public:
    explicit CompositorDCompositionUISurface(qt_intf_t* p_intf,
                                             DCompRenderWindow* window,
                                             Microsoft::WRL::ComPtr<IDCompositionVisual> dcVisual,
                                             QObject *parent = nullptr);

    ~CompositorDCompositionUISurface();

    bool init();

    QQmlEngine* engine() const override { return m_qmlEngine; }
    QQuickWindow* getOffscreenWindow() const;

    void setContent(QQmlComponent* component,  QQuickItem* rootItem) override;

    QQuickItem * activeFocusItem() const override;

    void timerEvent(QTimerEvent *event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    bool initialiseD3DSwapchain(int width, int height);
    void releaseSharedTexture();
    bool updateSharedTexture(int width, int height);
    void resizeSwapchain(int width, int height);

    void requestUpdate();
    void render();
    void forceRender();

    void handleScreenChange();
    void forwardFocusObjectChanged(QObject* object);

    void createFbo();
    void destroyFbo();

    void updateSizes();
    void updatePosition();

private:
    friend class DCompRenderWindow;

    qt_intf_t* m_intf = nullptr;

    class OurD3DCompiler;
    std::shared_ptr<OurD3DCompiler> m_d3dCompiler;

    //Direct composition visual
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_dcUiVisual;

    //D3D11 rendering
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_d3dRenderTarget;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_d3dSwapChain;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_PS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_ShadersInputLayout;

    UINT m_vertexBufferStride = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_VertexBuffer;

    UINT m_quadIndexCount = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;


    //Shared texture D3D side
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_d3dInterimTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureShaderInput;
    HANDLE m_sharedTextureHandled = nullptr;

    //Shared texture D3D side
    Microsoft::WRL::ComPtr<ID3D11Device1> m_qtd3dDevice1;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_d3dInterimTextureQt;
    EGLSurface m_eglInterimTextureQt = 0;

    //Qt opengl context
    QOpenGLContext* m_context = nullptr;
    EGLDisplay m_eglDisplay  = 0;
    EGLContext m_eglCtx  = 0;
    EGLConfig m_eglConfig = 0;


    //offscreen surface and controller
    QOffscreenSurface* m_uiOffscreenSurface = nullptr;
    CompositorDCompositionRenderControl* m_uiRenderControl = nullptr;

    //the actual window where we render
    DCompRenderWindow* m_renderWindow = nullptr;

    //offscreen window for QML content
    CompositorOffscreenWindow* m_uiWindow = nullptr;
    QQmlEngine* m_qmlEngine = nullptr;
    QQmlComponent* m_qmlComponent = nullptr;
    QQuickItem* m_rootItem = nullptr;

    QSize m_surfaceSize;

    QBasicTimer m_renderTimer;
    bool m_renderPending = false;
};

} //namespace vlc

#endif // COMPOSITOR_DCOMP_UISURFACE_H
