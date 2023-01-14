#ifndef NATIVE_PICKER_C
#define NATIVE_PICKER_C
#include "native_picker.h"

#if defined(__APPLE__)
// This is fun: call into appkit just using objc_msgSend
// so that we can get a directory picker in C without
// needing to compile as objective C.
// Needs to link against Cocoa (or AppKit I guess, whatever).

#include <string.h>
#include <objc/message.h>
#include <CoreFoundation/CoreFoundation.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
extern id NSApp; // Linker needs to see us actually using something from AppKit
#ifndef SELUID
#define SELUID(str) sel_getUid(str)
#endif


static
warn_unused
int
native_gui_pick_directory(LongString* directory){
    typedef void(BoolSetter)(id, SEL, BOOL);
    BoolSetter *boolsetter = (BoolSetter*)objc_msgSend;

    // The app's activationPolicy needs to be set to accessory
    Class appc = objc_getClass("NSApplication");
    id app = ((id(*)(Class, SEL))objc_msgSend)(appc, SELUID("sharedApplication"));

    enum {/*NS*/ApplicationActivationPolicyAccessory=1};
    ((void(*)(id, SEL, long))objc_msgSend)(app, SELUID("setActivationPolicy:"), ApplicationActivationPolicyAccessory);

    // Activate the app so the picker will become key.
    boolsetter(NSApp, SELUID("activateIgnoringOtherApps:"), YES);

    Class nsop = objc_getClass("NSOpenPanel");
    id panel = ((id(*)(Class, SEL))objc_msgSend)(nsop, SELUID("openPanel"));
    boolsetter(panel, SELUID("setCanChooseFiles:"), NO);
    boolsetter(panel, SELUID("setCanChooseDirectories:"), YES);
    boolsetter(panel, SELUID("setFloatingPanel:"), YES);
    boolsetter(panel, SELUID("setAllowsMultipleSelection:"), NO);

    long response = ((long(*)(id, SEL))objc_msgSend)(panel, SELUID("runModal"));
    // NSLog(CFSTR("response: %ld"), response);
    enum {/*NS*/ModalResponseOK = 1};
    if(response != ModalResponseOK) return 1;

    CFArrayRef urls = ((CFArrayRef(*)(id, SEL))objc_msgSend)(panel, SELUID("URLs"));
    // NSLog(CFSTR("URLS: %@"), urls);
    CFURLRef directoryURL = (CFURLRef)CFArrayGetValueAtIndex(urls, 0);
    CFStringRef path = CFURLCopyFileSystemPath(directoryURL, kCFURLPOSIXPathStyle); // new ref

    const char* s = CFStringGetCStringPtr(path, kCFStringEncodingUTF8);
    char buff[1024];
    if(!s){
        if(CFStringGetCString(path, buff, sizeof buff, kCFStringEncodingUTF8))
            s = buff;
        else {
            CFRelease(path);
            return 1;
        }
    }
    assert(s);
    size_t len = strlen(s);
    char* copy = strdup(s);
    directory->text = copy;
    directory->length = len;
    CFRelease(path);
    // Probably leaking some objects in this function.
    return 0;
}
#elif defined(__linux__)
#include <stdio.h>
#include <string.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
native_gui_pick_directory(LongString* directory){
    // Easiest to just shell out to zenity
    // Avoids the need to actually link against gtk or whatever.
    // TODO: non-gnome systems
    FILE* proc = popen("zenity --file-selection --directory --filename=.", "r");
    if(!proc) return 1;
    char buff[1024];
    char* g = fgets(buff, sizeof buff, proc);
    pclose(proc);
    if(!g) return 1;
    size_t len = strlen(buff);
    if(len <= 1) return 1;
    if(buff[len-1]=='\n') buff[--len] = '\0';
    char* fn = strdup(buff);
    directory->text = fn;
    directory->length = len;
    return 0;
}

#elif defined(_WIN32)
#include <assert.h>
#include <shobjidl.h>
#include <stdlib.h>
#include "Platform/Windows/windowsheader.h"
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
native_gui_pick_directory(LongString* directory){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Create dialog
    IFileOpenDialog* filedialog = NULL;
    HRESULT result = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL, &IID_IFileOpenDialog, (void**)&filedialog);
    if(!filedialog) {
        CoUninitialize();
        return 1;
    }
    DWORD options = 0;
    filedialog->lpVtbl->GetOptions(filedialog, &options);
    // Add in FOS_PICKFOLDERS which hides files and only allows selection of folders
    filedialog->lpVtbl->SetOptions(filedialog, options | FOS_PICKFOLDERS);
    // Show the dialog to the user
    result = filedialog->lpVtbl->Show(filedialog, NULL);
    if(!SUCCEEDED(result))
        goto Lbad;
    // Get the folder name
    IShellItem* shellitem = NULL;

    result = filedialog->lpVtbl->GetResult(filedialog, &shellitem);
    if(!SUCCEEDED(result))
        goto Lshellitemerror;

    wchar_t *path = NULL;
    result = shellitem->lpVtbl->GetDisplayName(shellitem, SIGDN_DESKTOPABSOLUTEPARSING, &path);
    if(!SUCCEEDED(result))
        goto Lshellitemerror;

    size_t bytes_needed = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    char* d = malloc(bytes_needed);
    directory->text = d;
    // d[bytes_needed] = 0;
    directory->length = bytes_needed-1;
    WideCharToMultiByte(CP_UTF8, 0, path, -1, d, bytes_needed, NULL, NULL);
    assert(d[bytes_needed-1] == 0);
    CoTaskMemFree(path);
    shellitem->lpVtbl->Release(shellitem);

    if(0){
        Lshellitemerror:
        shellitem->lpVtbl->Release(shellitem);
        goto Lbad;
    }

    filedialog->lpVtbl->Release(filedialog);

    CoUninitialize();

    return 0;

    Lbad:
    filedialog->lpVtbl->Release(filedialog);
    CoUninitialize();
    return 1;
}
#else // wasm or something

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
native_gui_pick_directory(LongString* directory){
    (void)directory;
    return 1;
}

#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
