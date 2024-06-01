/* compile: cc win_player.c -o win_player.exe -L<path/libvlc> -lvlc */

#include <windows.h>
#include <assert.h>

#include <vlc/vlc.h>

#define SCREEN_WIDTH  1500
#define SCREEN_HEIGHT  900

struct vlc_context
{
    libvlc_instance_t     *p_libvlc;
    libvlc_media_player_t *p_mediaplayer;
};


static const char *AspectRatio = NULL;

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if( message == WM_CREATE )
    {
        /* Store p_mediaplayer for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hWnd, GWLP_USERDATA );
    if( p_user_data == 0 )
        return DefWindowProc(hWnd, message, wParam, lParam);
    struct vlc_context *ctx = (struct vlc_context *)p_user_data;

    switch(message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_DROPFILES:
            {
                HDROP hDrop = (HDROP)wParam;
                char file_path[MAX_PATH];
                libvlc_media_player_stop_async( ctx->p_mediaplayer );

                if (DragQueryFile(hDrop, 0, file_path, sizeof(file_path)))
                {
                    libvlc_media_t *p_media = libvlc_media_new_path( file_path );
                    libvlc_media_t *p_old_media = libvlc_media_player_get_media( ctx->p_mediaplayer );
                    libvlc_media_player_set_media( ctx->p_mediaplayer, p_media );
                    libvlc_media_release( p_old_media );

                    libvlc_media_player_play( ctx->p_mediaplayer );
                }
                DragFinish(hDrop);
            }
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            {
                int key = tolower( MapVirtualKey( (UINT)wParam, 2 ) );
                if (key == 'a')
                {
                    if (AspectRatio == NULL)
                        AspectRatio = "16:10";
                    else if (strcmp(AspectRatio,"16:10")==0)
                        AspectRatio = "16:9";
                    else if (strcmp(AspectRatio,"16:9")==0)
                        AspectRatio = "4:3";
                    else if (strcmp(AspectRatio,"4:3")==0)
                        AspectRatio = "185:100";
                    else if (strcmp(AspectRatio,"185:100")==0)
                        AspectRatio = "221:100";
                    else if (strcmp(AspectRatio,"221:100")==0)
                        AspectRatio = "235:100";
                    else if (strcmp(AspectRatio,"235:100")==0)
                        AspectRatio = "239:100";
                    else if (strcmp(AspectRatio,"239:100")==0)
                        AspectRatio = "5:3";
                    else if (strcmp(AspectRatio,"5:3")==0)
                        AspectRatio = "5:4";
                    else if (strcmp(AspectRatio,"5:4")==0)
                        AspectRatio = "1:1";
                    else if (strcmp(AspectRatio,"1:1")==0)
                        AspectRatio = NULL;
                    libvlc_video_set_aspect_ratio( ctx->p_mediaplayer, AspectRatio );
                }
                break;
            }
        default: break;
    }

    return DefWindowProc (hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    WNDCLASSEX wc;
    char *file_path;
    struct vlc_context Context;
    libvlc_media_t *p_media;
    (void)hPrevInstance;
    HWND hWnd;

    /* remove "" around the given path */
    if (lpCmdLine[0] == '"')
    {
        file_path = _strdup( lpCmdLine+1 );
        if (file_path[strlen(file_path)-1] == '"')
            file_path[strlen(file_path)-1] = '\0';
    }
    else
        file_path = _strdup( lpCmdLine );

    Context.p_libvlc = libvlc_new( 0, NULL );
    p_media = libvlc_media_new_path( file_path );
    free( file_path );
    Context.p_mediaplayer = libvlc_media_player_new_from_media(
                                                   Context.p_libvlc, p_media );

    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "WindowClass";

    RegisterClassEx(&wc);

    RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindowEx(0,
                          "WindowClass",
                          "libvlc Demo app",
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          wr.right - wr.left,
                          wr.bottom - wr.top,
                          NULL,
                          NULL,
                          hInstance,
                          &Context);
    DragAcceptFiles(hWnd, TRUE);

    libvlc_media_player_set_hwnd(Context.p_mediaplayer, hWnd);

    ShowWindow(hWnd, nCmdShow);

    libvlc_media_player_play( Context.p_mediaplayer );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if(msg.message == WM_QUIT)
            break;
    }

    libvlc_media_player_stop_async( Context.p_mediaplayer );

    libvlc_media_release( libvlc_media_player_get_media( Context.p_mediaplayer ) );
    libvlc_media_player_release( Context.p_mediaplayer );
    libvlc_release( Context.p_libvlc );

    return (int)msg.wParam;
}
