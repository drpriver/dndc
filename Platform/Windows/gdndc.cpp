#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <Richedit.h>
#include <assert.h>
#include <Stringapiset.h>
#undef ERROR
#include "dndc.h"
#pragma comment(lib, "user32.lib")
// #pragma comment(lib, "WebView2Loader.dll.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "kernel32.lib")
// #pragma comment(lib, "Gdi32.lib")
// #pragma comment(lib, "advapi32.lib")
// #pragma comment(lib, "comdlg32.lib")
// #pragma comment(lib, "odbc32.lib")
// #pragma comment(lib, "odbccp32.lib")
// #pragma comment(lib, "ole32.lib")
// #pragma comment(lib, "oleaut32.lib")
// #pragma comment(lib, "user32.lib")
// #pragma comment(lib, "uuid.lib")
// #pragma comment(lib, "winspool.lib")

// The main window class name.
static TCHAR win_class[] = _T("DesktopApp");

// The string that appears in the application's title bar.
static TCHAR title[] = _T("WebView sample");

static HINSTANCE app_instance;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Pointer to WebViewController
static wil::com_ptr<ICoreWebView2Controller> webviewController;

// Pointer to WebView window
static wil::com_ptr<ICoreWebView2> webviewWindow;
static wil::com_ptr<ICoreWebView2_3> webviewWindow_3;
static HWND textedit_handle;
// mutable on purpose
static int TEXTEDIT_WIDTH = 800;
enum {ID_EDIT=1};

static
char*
get_utf8_string_from_window(HWND handle){
    // TODO: error handling
    int nchars = GetWindowTextLengthW(handle); // length in "characters"
    nchars +=1 ; // for luck
    wchar_t* buffer;
    size_t size = sizeof(*buffer)*nchars;
    buffer = (wchar_t*)malloc(size);
    if(!buffer)
        return NULL;
    buffer[nchars-1] = 0;
    // TODO: error handling
    GetWindowTextW(handle, buffer, size);
    // TODO: error handling
    size_t needed_size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
    auto result = (char*)malloc(needed_size);
    // TODO: error handling
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result, needed_size, NULL, NULL);
    free(buffer);
    return result;
}

struct WinString {
    wchar_t* text;
    int nchars; // includes terminating null character
};

static
WinString
make_windows_string_from_utf8_string(const char* text){
    int n_needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t* result = (wchar_t*)malloc(n_needed * sizeof(*result));
    if(!result){
        return {NULL, 0};
        }
    int n_written = MultiByteToWideChar(CP_UTF8, 0, text, -1, result, n_needed);
    return {result, n_written};
}

static struct {
    char* text;
    bool should_quit;
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE cond;
} worker_data;

static HWND mainwindow;

static
DWORD
thread_worker(void*){
    dndc_init_python();
    for(;;){
        EnterCriticalSection(&worker_data.lock);
        SleepConditionVariableCS(&worker_data.cond, &worker_data.lock, INFINITE);
        char* text = NULL;
        if(worker_data.text){
            text = worker_data.text;
            worker_data.text = NULL;
            }
        if(worker_data.should_quit){
            return 0;
            }
        LeaveCriticalSection(&worker_data.lock);
        if(text){
            LongString source = {.length=strlen(text), .text=text};
            LongString output = {};
            int fail = dndc_make_html(SV(""), source, &output);
            free(text);
            if(!fail){
                auto fh = CreateFileW(L"html/foo.html", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if(fh != INVALID_HANDLE_VALUE){
                    DWORD bytes_written;
                    BOOL write_success = WriteFile(
                            fh,
                            output.text,
                            output.length,
                            &bytes_written,
                            NULL);
                    (void)bytes_written;
                    (void)write_success;
                    CloseHandle(fh);
                    free((void*)output.text);
                    PostMessageW(mainwindow, WM_USER, 0, 0);
                    }
                }
            }
        }
    return 0;
    }

static
void
set_text(char* text){
    EnterCriticalSection(&worker_data.lock);
    if(worker_data.text){
        free(worker_data.text);
        }
    worker_data.text = text;
    LeaveCriticalSection(&worker_data.lock);
    WakeConditionVariable(&worker_data.cond);
    }

int main(){
    LPWSTR versionInfo;
    auto res = GetAvailableCoreWebView2BrowserVersionString(NULL, &versionInfo);
    (void)res;
    wprintf(L"versionInfo: %s\n", versionInfo);
    app_instance = GetModuleHandle(NULL);
    LoadLibraryW(L"Msftedit.dll");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	WNDCLASSEX wcex = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = app_instance,
        .hIcon = LoadIcon(app_instance, IDI_APPLICATION),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = win_class,
        .hIconSm = LoadIcon(app_instance, IDI_APPLICATION),
    };

	if (!RegisterClassEx(&wcex)){
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}

	// Store instance handle in our global variable
	HWND hWnd = CreateWindow(
		win_class,  // name of app
		title,      // title bar text
		WS_OVERLAPPEDWINDOW, // window style
		CW_USEDEFAULT, CW_USEDEFAULT, // initial position (x, y)
		CW_USEDEFAULT, CW_USEDEFAULT, // (width, height)
		NULL, // parent window
		NULL, // menu bar
		app_instance, // instance of this app
		NULL // lpCreateParams member of CREATESTRUCT for WM_CREATE message
	);
    mainwindow = hWnd;

	if (!hWnd)
	{
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Foo"),
			NULL);

		return 1;
	}

    InitializeCriticalSection(&worker_data.lock);
    InitializeConditionVariable(&worker_data.cond);
    HANDLE thread = CreateThread(NULL, 0, thread_worker, NULL, 0, NULL);
    (void)thread;

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, // PCWSTR browserExecutableFolder
        nullptr, // PCWSTR userDataFolder: TODO: with NULL, this shits out stuff next to the exe, which is retarded
        nullptr, // ICoreWebView2EnvironmentOptions* environmentOptions
        // below is the environmentCreatedHandler argument
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                (void)result;
                // Create a CoreWebView2Controller and get the associated
                // CoreWebView2 whose parent is the main window hWnd
                env->CreateCoreWebView2Controller(hWnd, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                    (void)result;
                    if (controller != nullptr) {
                        webviewController = controller;
                        webviewController->get_CoreWebView2(&webviewWindow);
                    }
                    auto qresult = webviewWindow->QueryInterface(__uuidof(ICoreWebView2_3), (void**)&webviewWindow_3);
                    // assume it succeeds
                    assert(qresult == S_OK);
                    webviewWindow_3->SetVirtualHostNameToFolderMapping(L".invalid.", L"..\\html", COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY);


                    // Add a few settings for the webview
                    // The demo step is redundant since the values are the default settings
                    ICoreWebView2Settings* Settings;
                    webviewWindow->get_Settings(&Settings);
                    Settings->put_IsScriptEnabled(TRUE);
                    Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                    Settings->put_IsWebMessageEnabled(TRUE);

                    // Resize WebView to fit the bounds of the parent window
                    RECT bounds;
                    GetClientRect(hWnd, &bounds);
                    if(bounds.right > TEXTEDIT_WIDTH){
                        bounds.right -= TEXTEDIT_WIDTH;
                    }
                    webviewController->put_Bounds(bounds);

                    // Schedule an async task to navigate
                    // webviewWindow->Navigate(L"https://.invalid./README.html");
                    webviewWindow->Navigate(L"https://.invalid./foo.html");
                    // auto navigation = webviewWindow->NavigateToString(L"<h1>Hello World!</hi>");

                    // Step 4 - Navigation events
                    // register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
                    EventRegistrationToken token;
                    webviewWindow->add_NavigationStarting(Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs * args) -> HRESULT {
                            (void)webview;
                            PWSTR uri;
                            args->get_Uri(&uri);
                            std::wstring source(uri);
                            if(source.length() < 18 or source.substr(0, 18) != L"https://.invalid./") {
                                args->put_Cancel(true);
                            }
                            CoTaskMemFree(uri);
                            return S_OK;
                        }).Get(), &token);
                    // Step 5 - Scripting
                    // Schedule an async task to add initialization script that freezes the Object object
                    // webviewWindow->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
                    // Schedule an async task to get the document URL
                    // webviewWindow->ExecuteScript(L"location.reload();",NULL);
                    // webviewWindow->ExecuteScript(L"window.document.URL;", Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                        // [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                            // LPCWSTR URL = resultObjectAsJson;
                            //doSomethingWithURL(URL);
                            // return S_OK;
                        // }).Get());
                    // Step 6 - Communication between host and web content

                    return S_OK;
                }).Get());
            return S_OK;
        }).Get());


	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    // DeleteCriticalSection(&worker_data.lock);

	return (int)msg.wParam;
}

LRESULT
CALLBACK
WndProc(HWND mainwindow_handle, UINT message, WPARAM wParam, LPARAM lParam){
	switch(message){
    case WM_CREATE:{
        RECT bounds;
        GetClientRect(mainwindow_handle, &bounds);
        textedit_handle = CreateWindowExW(0, MSFTEDIT_CLASS, NULL,
            ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
            bounds.right - TEXTEDIT_WIDTH, 0, TEXTEDIT_WIDTH, 0,
            mainwindow_handle, (HMENU)ID_EDIT, ((CREATESTRUCT*)lParam)->hInstance, NULL);
        CHARFORMATW fmt = {
            .cbSize = sizeof(fmt),
            .dwMask = CFM_FACE,
            .szFaceName = L"Consolas",
            };
        SendMessageW(textedit_handle, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&fmt);
        if(webviewController != nullptr){
            if(bounds.right > TEXTEDIT_WIDTH){
                bounds.right -= TEXTEDIT_WIDTH;
            }
            webviewController->put_Bounds(bounds);
        }
        }return 0;

    case WM_SETFOCUS:{
        SetFocus(textedit_handle);
        }return 0;
    case WM_USER:{
        if(webviewWindow)
            webviewWindow->ExecuteScript(L"location.reload();",NULL);
        }return 0;
    case WM_COMMAND:{
        if(LOWORD(wParam) == ID_EDIT){
            switch(HIWORD(wParam)){
                case EN_UPDATE:{
                    char* text = get_utf8_string_from_window((HWND)lParam);
                    set_text(text);
                    }break;
                }
            }
        }break; // default
	case WM_SIZE:{
        RECT bounds;
        GetClientRect(mainwindow_handle, &bounds);
        if(webviewController != nullptr){
            if(bounds.right > TEXTEDIT_WIDTH){
                bounds.right -= TEXTEDIT_WIDTH;
                }
            webviewController->put_Bounds(bounds);
        }
        MoveWindow(textedit_handle, LOWORD(lParam) - TEXTEDIT_WIDTH, 0, TEXTEDIT_WIDTH, HIWORD(lParam), TRUE);
        }return 0;
	case WM_DESTROY:{
		PostQuitMessage(0);
        }return 0;
    }
    return DefWindowProc(mainwindow_handle, message, wParam, lParam);
}
