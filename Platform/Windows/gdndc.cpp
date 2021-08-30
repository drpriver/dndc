#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <Richedit.h>
#include <Stringapiset.h>
#include <commdlg.h>
#include <assert.h>
#include <wchar.h>
// #include <wchar.h>
#undef ERROR
#include "dndc_api_def.h"
#include "dndc.h"
typedef struct DndcLongString LongString;
typedef struct DndcLongStringUtf16 LongStringUtf16;
typedef struct DndcStringView StringView;
typedef struct DndcStringViewUtf16 StringViewUtf16;
#define LONGSTRING_DEFINED
#include "long_string.h"
#pragma comment(lib, "user32.lib")
// #pragma comment(lib, "WebView2Loader.dll.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Gdi32.lib")
// #pragma comment(lib, "advapi32.lib")
// #pragma comment(lib, "comdlg32.lib")
// #pragma comment(lib, "odbc32.lib")
// #pragma comment(lib, "odbccp32.lib")
// #pragma comment(lib, "ole32.lib")
// #pragma comment(lib, "oleaut32.lib")
// #pragma comment(lib, "user32.lib")
// #pragma comment(lib, "uuid.lib")
// #pragma comment(lib, "winspool.lib")

static wchar_t win_class[] = L"DesktopApp";

static wchar_t title[] = L"Gdndc";
static wchar_t filepath[1024];

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static wil::com_ptr<ICoreWebView2Controller> webviewController;

// Pointer to WebView window
static wil::com_ptr<ICoreWebView2> webviewWindow;
static wil::com_ptr<ICoreWebView2_3> webviewWindow_3;
static HWND textedit_handle;
// mutable on purpose
static int TEXTEDIT_WIDTH = 1100;
enum {ID_EDIT=1};

#if 1
void print_error(const wchar_t* where_failed){
    // Retrieve the system error message for the last-error code
    wchar_t* MsgBuf;
    DWORD dw = GetLastError();

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (wchar_t*) &MsgBuf,
        0, NULL );

    // Display the error message and exit the process

    wchar_t* DisplayBuf = (wchar_t*)LocalAlloc(LMEM_ZEROINIT, (lstrlenW(MsgBuf) + lstrlenW(where_failed) + 40) * sizeof(wchar_t));
    StringCchPrintfW(DisplayBuf, LocalSize(DisplayBuf) / sizeof(wchar_t), L"%s failed with error %d: %s", where_failed, dw, MsgBuf);
    MessageBoxW(NULL, DisplayBuf, L"Error", MB_OK);

    LocalFree(MsgBuf);
    LocalFree(DisplayBuf);
    }
#else
#define print_error(mess) (void)mess
#endif

static
LongString
get_utf8_string_from_window(HWND handle){
    // TODO: error handling
    int nchars = GetWindowTextLengthW(handle); // length in "characters"
    nchars +=1 ; // for luck
    wchar_t* buffer;
    size_t size = sizeof(*buffer)*nchars;
    buffer = (wchar_t*)malloc(size);
    if(!buffer)
        return {};
    buffer[nchars-1] = 0;
    // TODO: error handling
    GetWindowTextW(handle, buffer, size);
    // TODO: error handling
    size_t needed_size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
    auto result = (char*)malloc(needed_size);
    // TODO: error handling
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result, needed_size, NULL, NULL);
    free(buffer);
    return {.length=needed_size-1, .text=result};
}

struct WinString {
    wchar_t* text;
    size_t nchars_with_zero; // includes terminating null character
};

static void choose_open_file(HWND);
static bool save_file(HWND);
static bool save_as_file(HWND);
static bool sortof_atomically_write_file(LongString text, WinString path);
static bool choose_font(HWND);
static
WinString
make_windows_string_from_utf8_string(const char* text){
    int n_needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t* result = (wchar_t*)malloc(n_needed * sizeof(*result));
    if(!result){
        return {NULL, 0};
        }
    int n_written = MultiByteToWideChar(CP_UTF8, 0, text, -1, result, n_needed);
    return {result, (size_t)n_written};
}

static struct {
    const char* text;
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
        const char* text = NULL;
        EnterCriticalSection(&worker_data.lock);
        BOOL res = SleepConditionVariableCS(&worker_data.cond, &worker_data.lock, INFINITE);
        assert(res);
        text = worker_data.text;
        worker_data.text = NULL;
        if(worker_data.should_quit){
            LeaveCriticalSection(&worker_data.lock);
            return 0;
            }
        LeaveCriticalSection(&worker_data.lock);
        if(text){
            LongString source = {.length=strlen(text), .text=text};
            LongString output = {};
            int fail = dndc_compile_dnd_file(DNDC_FLAGS_NONE
                    | DNDC_ALLOW_BAD_LINKS
                    | DNDC_NO_THREADS
                    | DNDC_STRIP_WHITESPACE
                    | DNDC_DONT_PRINT_ERRORS
                    | DNDC_SUPPRESS_WARNINGS
                    ,
                    LS(""),
                    source,
                    LS("this.html"),
                    &output,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL
                    );
            if(!fail){
                {
                auto fh = CreateFileW(L"D:/DndC/html/foo.html", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if(fh != INVALID_HANDLE_VALUE){
                    DWORD bytes_written;
                    BOOL write_success = WriteFile(
                            fh,
                            output.text,
                            output.length,
                            &bytes_written,
                            NULL);
                    assert(bytes_written == output.length);
                    (void)bytes_written;
                    (void)write_success;
                    CloseHandle(fh);
                    PostMessage(mainwindow, WM_USER, 0, 0);
                    }
                else {
                    print_error(L"Failed to write the html");
                    }
                }
                dndc_free_string(output);
                }
            else {
                // MessageBox(NULL, TEXT("Unable to compile the dndc file"), TEXT("Error"), MB_OK);
                }
            free((void*)text);
            }
        }
    return 0;
    }

static
void
set_text(const char* text){
    EnterCriticalSection(&worker_data.lock);
    if(worker_data.text){
        free((void*)worker_data.text);
        }
    worker_data.text = text;
    LeaveCriticalSection(&worker_data.lock);
    WakeConditionVariable(&worker_data.cond);
    }

enum {
    IDM_MESSAGE_BASE = 40000,
    IDM_FILE_NEW     ,
    IDM_FILE_OPEN    ,
    IDM_FILE_SAVE    ,
    IDM_FILE_SAVE_AS ,
    IDM_APP_EXIT     ,
    IDM_EDIT_UNDO    ,
    IDM_EDIT_REDO    ,
    IDM_EDIT_CUT     ,
    IDM_EDIT_COPY    ,
    IDM_EDIT_PASTE   ,
    IDM_FORMAT_FONT  ,
};

static void make_menus(HWND);
static void handle_edit(HWND);
static void make_webview(HWND);

int main(){
    LPWSTR versionInfo;
    auto res = GetAvailableCoreWebView2BrowserVersionString(NULL, &versionInfo);
    // TODO: tell user to install runtime
    (void)res;
    HINSTANCE app_instance = GetModuleHandle(NULL);
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

	if(!RegisterClassEx(&wcex)){
		return 1;
	}

	// Store instance handle in our global variable
	mainwindow = CreateWindow(
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

	if(!mainwindow){
		MessageBoxW(NULL,
			L"Call to CreateWindow failed!",
			L"Foo",
			NULL);
		return 1;
	}

    InitializeCriticalSection(&worker_data.lock);
    InitializeConditionVariable(&worker_data.cond);
    HANDLE thread = CreateThread(NULL, 0, thread_worker, NULL, 0, NULL);
    (void)thread;
    make_menus(mainwindow);
    make_webview(mainwindow);
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

LRESULT
CALLBACK
WndProc(HWND mainwindow_handle, UINT message, WPARAM wParam, LPARAM lParam){
	switch(message){
    case WM_CREATE:{
        RECT bounds;
        GetClientRect(mainwindow_handle, &bounds);
        textedit_handle = CreateWindowEx(0,
                MSFTEDIT_CLASS,
                NULL,
            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
            0, 0, TEXTEDIT_WIDTH, 0,
            mainwindow_handle, (HMENU)ID_EDIT, ((CREATESTRUCT*)lParam)->hInstance, NULL);
        CHARFORMATW fmt1 = {
            .cbSize = sizeof(fmt1),
            .dwMask = CFM_FACE,
            // .szFaceName = L"Consolas",
            .szFaceName = L"Cascadia Mono",
            };
        LRESULT font_success = SendMessage(textedit_handle, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&fmt1);
        if(!font_success){
            CHARFORMATW fmt2 = {
                .cbSize = sizeof(fmt2),
                .dwMask = CFM_FACE,
                .szFaceName = L"Consolas",  // I think Consolas is guaranteed to be installed?
                };
            font_success = SendMessage(textedit_handle, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&fmt2);
            }
        SendMessage(textedit_handle, EM_SETEVENTMASK, 0, ENM_CHANGE);
        if(webviewController != nullptr){
            bounds.left += TEXTEDIT_WIDTH;
            webviewController->put_Bounds(bounds);
        }
        PostMessage(mainwindow_handle, WM_USER+1, 0, 0);
        }return 0;

    case WM_SETFOCUS:{
        SetFocus(textedit_handle);
        }return 0;
    case WM_USER+1:{
        choose_open_file(textedit_handle);
        }return 0;
    case WM_USER:{
        if(webviewWindow)
            webviewWindow->ExecuteScript(L"location.reload();", NULL);
        }return 0;
    case WM_COMMAND:{
        switch(LOWORD(wParam)){
            case ID_EDIT:{
                switch(HIWORD(wParam)){
                    case EN_CHANGE:{
                        handle_edit((HWND)lParam);
                        }break;
                    }
                }break; // fallthrough to defwindowproc
            case IDM_FILE_NEW:{
                }return 0;
            case IDM_FILE_OPEN:{
                choose_open_file(textedit_handle);
                }return 0;
            case IDM_FILE_SAVE:{
                save_file(textedit_handle);
                }return 0;
            case IDM_FILE_SAVE_AS:{
                save_as_file(textedit_handle);
                }return 0;
            case IDM_EDIT_UNDO:
                SendMessage(textedit_handle, WM_UNDO, 0, 0);
                return 0;
            case IDM_EDIT_REDO:
                SendMessage(textedit_handle, EM_REDO, 0, 0);
                return 0;
            case IDM_EDIT_CUT:
                SendMessage(textedit_handle, WM_CUT, 0, 0);
                return 0;
            case IDM_EDIT_COPY:
                SendMessage(textedit_handle, WM_COPY, 0, 0);
                return 0;
            case IDM_EDIT_PASTE:
                SendMessage(textedit_handle, WM_PASTE, 0, 0);
                return 0;
            case IDM_APP_EXIT:
                SendMessage(mainwindow_handle, WM_CLOSE, 0, 0);
                return 0;
            case IDM_FORMAT_FONT:
                choose_font(textedit_handle);
                return 0;
            }
        }break; // default
	case WM_SIZE:{
        RECT bounds;
        GetClientRect(mainwindow_handle, &bounds);
        if(webviewController != nullptr){
            bounds.left += TEXTEDIT_WIDTH;
            webviewController->put_Bounds(bounds);
        }
        MoveWindow(textedit_handle, 0, 0, TEXTEDIT_WIDTH, HIWORD(lParam), TRUE);
        }return 0;
	case WM_DESTROY:{
		PostQuitMessage(0);
        }return 0;
    }
    return DefWindowProc(mainwindow_handle, message, wParam, lParam);
}

static
void
make_menus(HWND window){
    HMENU menu = CreateMenu();
    HMENU popup = CreateMenu();
    AppendMenuW(popup, MF_STRING,    IDM_FILE_NEW,     L"&New");
    AppendMenuW(popup, MF_STRING,    IDM_FILE_OPEN,    L"&Open...");
    AppendMenuW(popup, MF_STRING,    IDM_FILE_SAVE,    L"&Save");
    AppendMenuW(popup, MF_STRING,    IDM_FILE_SAVE_AS, L"Save &As...");
    AppendMenuW(popup, MF_SEPARATOR, 0,                NULL);
    AppendMenuW(popup, MF_STRING,    IDM_APP_EXIT,     L"E&xit");

    AppendMenuW(menu, MF_POPUP, (uintptr_t)popup, L"&File");

    popup = CreateMenu();
    AppendMenuW(popup, MF_STRING,    IDM_EDIT_UNDO,  L"&Undo");
    AppendMenuW(popup, MF_STRING,    IDM_EDIT_REDO,  L"Redo");
    AppendMenuW(popup, MF_SEPARATOR, 0,              NULL);
    AppendMenuW(popup, MF_STRING,    IDM_EDIT_CUT,   L"Cu&t");
    AppendMenuW(popup, MF_STRING,    IDM_EDIT_COPY,  L"&Copy");
    AppendMenuW(popup, MF_STRING,    IDM_EDIT_PASTE, L"&Paste");

    AppendMenuW(menu, MF_POPUP, (uintptr_t)popup, L"&Edit");

    popup = CreateMenu();
    AppendMenuW(popup, MF_STRING,    IDM_FORMAT_FONT, L"F&ont");

    AppendMenuW(menu, MF_POPUP, (uintptr_t)popup, L"&Format");

    SetMenu(window, menu);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
    }

static
void
handle_edit(HWND textedit){
    auto text = get_utf8_string_from_window(textedit);
    set_text(text.text);
    }

static
void
choose_open_file(HWND textedit){
    wchar_t filestr[1024] = {};
    OPENFILENAMEW openfilename = {
        .lStructSize = sizeof(openfilename),
        .hwndOwner = textedit,
        .lpstrFilter = L"DND Files\0*.dnd\0\0",
        .nFilterIndex = 1,
        .lpstrFile = filestr,
        .nMaxFile = 1024,
        .Flags = OFN_CREATEPROMPT | OFN_EXPLORER | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST,
        .lpstrDefExt = L"dnd",
        };
    BOOL ok_clicked = GetOpenFileNameW(&openfilename);
    if(ok_clicked){
        auto handle = CreateFileW(
            filestr,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if(handle != INVALID_HANDLE_VALUE){
            LARGE_INTEGER size;
            BOOL size_success = GetFileSizeEx(handle, &size);
            if(size_success){
                char* text = (char*)malloc((size.QuadPart+1)*sizeof(*text));
                if(text){
                    DWORD nread;
                    BOOL read_success = ReadFile(handle, text, size.QuadPart*sizeof(*text), &nread, NULL);
                    if(read_success){
                        text[size.QuadPart] = '\0';
                        auto ws = make_windows_string_from_utf8_string(text);
                        if(ws.text)
                            SetWindowTextW(textedit, ws.text);
                        else
                            SetWindowTextW(textedit, L"\0");
                        static_assert(sizeof(filepath) == sizeof(filestr));
                        memcpy(filepath, filestr, sizeof(filestr));
                        free(ws.text);
                        }
                    }
                free(text);
                }
            }
        CloseHandle(handle);
        }
    }

static
bool
save_file(HWND textedit){
    if(!filepath[0]){
        return save_as_file(textedit);
        }
    auto text = get_utf8_string_from_window(textedit);
    WinString path = {.text=filepath, .nchars_with_zero = wcslen(filepath)+1};
    auto result = sortof_atomically_write_file(text, path);
    free((void*)text.text);
    return result;
    }
static
bool
sortof_atomically_write_file(LongString text, WinString path){
    {
    // First thing to do is to try to create the file if it does not exist.
    // Only if it does exist do we try to overwrite it using ReplaceFile.
    auto fh = CreateFileW(
            path.text,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    if(fh != INVALID_HANDLE_VALUE){
        DWORD written;
        auto success = WriteFile(fh, text.text, text.length, &written, NULL);
        CloseHandle(fh);
        return success? true : false;
        }
    }
    // Ok, so we tried to create the file and failed. Therefore it already
    // exists. Write to a temp file and then replace the original.
    wchar_t tmppath[1024];
    wchar_t tmppath2[1024];
    if(path.nchars_with_zero > 1022){ // space for '\0' and trailing char
        return false;
        }
    memcpy(tmppath, path.text, path.nchars_with_zero*sizeof(path.text[0]));
    memcpy(tmppath2, path.text, path.nchars_with_zero*sizeof(path.text[0]));
    // append a 't'
    tmppath[path.nchars_with_zero-1]  = L't';
    tmppath[path.nchars_with_zero]    = L'\0';
    // append a 'b'
    tmppath2[path.nchars_with_zero-1] = L'b';
    tmppath2[path.nchars_with_zero]   = L'\0';
    auto tmpfile = CreateFileW(
            tmppath,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    if(tmpfile == INVALID_HANDLE_VALUE){
        return false;
        }
    DWORD written;
    auto success = WriteFile(tmpfile, text.text, text.length, &written, NULL);
    CloseHandle(tmpfile);
    if(!success){
        return false;
        }
    auto replaced = ReplaceFileW(path.text, tmppath, tmppath2, 0, 0, 0);
    if(replaced == 0){
        print_error(L"replace failed");
        DeleteFileW(tmppath2);
        return false;
        }
    DeleteFileW(tmppath2);
    return true;
    }

static
bool
save_as_file(HWND textedit){
    wchar_t filestr[1024] = {};
    OPENFILENAMEW openfilename = {
        .lStructSize = sizeof(openfilename),
        .hwndOwner = textedit,
        .lpstrFilter = L"DND Files\0*.dnd\0\0",
        .nFilterIndex = 1,
        .lpstrFile = filestr,
        .nMaxFile = 1024,
        .Flags =  OFN_EXPLORER | OFN_NOREADONLYRETURN,
        .lpstrDefExt = L"dnd",
        };
    BOOL ok_clicked = GetSaveFileNameW(&openfilename);
    if(!ok_clicked)
        return false;
    static_assert(sizeof(filepath) == sizeof(filestr));
    memcpy(filepath, filestr, sizeof(filestr));
    auto text = get_utf8_string_from_window(textedit);
    WinString path = {.text=filepath, .nchars_with_zero = wcslen(filepath)+1};
    auto result = sortof_atomically_write_file(text, path);
    free((void*)text.text);
    return result;
    }


static
void
make_webview(HWND window){
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, // PCWSTR browserExecutableFolder
        nullptr, // PCWSTR userDataFolder: TODO: with NULL, this shits out stuff next to the exe, which is retarded
        nullptr, // ICoreWebView2EnvironmentOptions* environmentOptions
        // below is the environmentCreatedHandler argument
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [window](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                (void)result;
                // Create a CoreWebView2Controller and get the associated
                // CoreWebView2 whose parent is the main window hWnd
                env->CreateCoreWebView2Controller(window, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [window](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
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
                    GetClientRect(window, &bounds);
                    bounds.left += TEXTEDIT_WIDTH;
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
    }

static
bool
choose_font(HWND textedit){
    static LOGFONT font;
    CHOOSEFONTW fontstruct = {
        .lStructSize = sizeof(fontstruct),
        .hwndOwner = textedit,
        .lpLogFont = &font,
        .Flags = CF_FIXEDPITCHONLY | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCALABLEONLY,
        };
    BOOL ok = ChooseFontW(&fontstruct);
    if(!ok)
        return false;
    HFONT new_font_handle = CreateFontIndirect(&font);
    SendMessage(textedit, WM_SETFONT, (WPARAM)new_font_handle, 0);
    static HFONT font_handle;
    DeleteObject(font_handle);
    font_handle = new_font_handle;
    return true;
    }
