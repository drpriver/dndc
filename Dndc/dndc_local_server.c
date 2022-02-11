#include "dndc.h"
#include "dndc_long_string.h"
#include "file_util.h"
#include "str_util.h"
#include "allocator.h"
#include "mallocator.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "windowsheader.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
// Move to header?
typedef long long ssize_t;

#else

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#endif

//
// This is a simple "http" server, intended for browsing .dnd files on your
// local system. This simplifies using .dnd files as you no longer need to
// compile them and thus also don't need a build system.
//
// This doesn't parse http requests at all. It just assumes everything is a GET
// and tries to respond with the corresponding file.
//

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

#if defined(__linux__)
void*_Nullable memmem(const void*_, size_t, const void*, size_t);
#endif
#if defined(_WIN32)
static
const void*_Nullable
memmem(const void* hay_, size_t haysz, const void* needle_, size_t needlesz){
    if(!hay_ || !haysz || !needle_ || !needlesz) return NULL;
    const char* hay = hay_;
    const char* needle = needle_;
    char first = *needle;
    const char* hayend = hay+haysz;
    for(;;){
        const char* c = memchr(hay, first, hayend-hay);
        if(!c) return NULL;
        if(hayend - c < (ssize_t)needlesz) return NULL;
        if(memcmp(c, needle, needlesz) == 0)
            return c;
        hay = c+1;
    }
}

// Hacky helper for stringifying error codes
// Leaks the error message, but whatever
const char* wsaerror(void){
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER;
    int err = WSAGetLastError();
    char* result = NULL;
    DWORD ret = FormatMessageA(flags, NULL, err, 0, (void*)&result, 0, NULL);
    if(!result) return "Error when formatting error";
    result[ret-1] = 0;
    return result;
}
#endif

static
TextFileResult
read_relative_file_with_suffix_conversion(LongString directory, StringView path, StringView suffix);

static
LongString
compile_file(DndcErrorFunc*func, void*_Nullable p, LongString directory, uint64_t flags, StringView path, LongString text, int *error);

static void vlogit(DndcErrorFunc*, void*_Nullable, int lvl, const char* msg, va_list args);

#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static void info(DndcErrorFunc*, void*_Nullable, const char* msg, ...);

#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static void debug(DndcErrorFunc*, void*_Nullable, const char* msg, ...);

#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static void error(DndcErrorFunc*, void*_Nullable, const char* msg, ...);

static
TextFileResult
read_relative_file_with_suffix_conversion(LongString directory, StringView path, StringView suffix){
    char buff[1024];
    snprintf(buff, sizeof buff, "%s/%.*s%.*s", directory.text, (int)path.length, path.text, (int)suffix.length, suffix.text);
    return read_file(buff, get_mallocator());
}

static
LongString
compile_file(DndcErrorFunc* func, void*_Nullable logdata, LongString directory, uint64_t flags, StringView path, LongString text, int *error){
    const char* slash = NULL;
    const char* p = path.text;
    for(;p;){
        const char* s = memchr(p, '/', path.text+path.length - p);
        if(!s) break;
        slash = s;
        p = slash+1;
    }
    char buff[1024];
    StringView base = LS_to_SV(directory);
    if(slash){
        int n = snprintf(buff, sizeof buff, "%s/%.*s", directory.text, (int)(slash-path.text), path.text);
        base = (StringView){.length = n, .text = buff};
    }
    LongString result;
    *error = dndc_compile_dnd_file(flags, base, LS_to_SV(text), SV(""), SV(""), &result, NULL, NULL, func, logdata, NULL, NULL, NULL, LS(""));
    return result;
}

#ifdef _WIN32
//
// winsock is just different enough that I'd rather keep the implementation separate.
static
int
handle_request(DndcErrorFunc* func, void*_Nullable p, uint64_t flags, LongString directory, SOCKET accsd, LongString request);

struct DndServer{
    SOCKET sd;
    DndcErrorFunc* func;
    void*_Nullable p;
};

DndServer*_Nullable
dnd_server_create(int* port, DndcErrorFunc* func, void*_Nullable p){
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2,2), &wsadata);
    if(err){
        error(func, p, "some error on startup or something");
        return NULL;
    }
    SOCKET listensocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listensocket == INVALID_SOCKET){
        error(func, p, "Error in socket");
        goto cleanup;
    }
    BOOL opt = 1;
    err = setsockopt(listensocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof opt);
    if(err){
        error(func, p, "setsockopt for SO_REUSEADDR failed: %s (%d)", wsaerror(), WSAGetLastError());
        goto cleanup;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.S_un.S_addr=htonl(INADDR_LOOPBACK),
        .sin_port = htons(*port),
    };

    err = bind(listensocket, (struct sockaddr*)&addr, sizeof addr);
    if(err){
        error(func, p, "bind error (%d): %s", WSAGetLastError(), wsaerror());
        error(func, p, "port was: %d", *port);
        goto cleanup;
    }

    int addrlen = sizeof addr;
    err = getsockname(listensocket, (struct sockaddr*)&addr, &addrlen);
    if(err){
        error(func, p, "getsockname error: %s (%d)", wsaerror(), WSAGetLastError());
        goto cleanup;
    }

    err = listen(listensocket, SOMAXCONN);
    if(err){
        error(func, p, "listen error: %s (%d)", wsaerror(), WSAGetLastError());
        goto cleanup;
    }
    info(func, p, "Serving at http://localhost:%d", (int)ntohs(addr.sin_port));
    *port = (int)ntohs(addr.sin_port);
    DndServer* server = malloc(sizeof *server);
    server->sd = listensocket;
    server->func = func;
    server->p = p;
    return server;

    cleanup:
    if(listensocket != INVALID_SOCKET)
        closesocket(listensocket);
    WSACleanup();
    return NULL;
}

int
dnd_server_serve(DndServer* server, uint64_t flags, LongString directory){
    SOCKET sd = server->sd;
    char buff[10000];
    struct sockaddr_in clientaddr = {0};
    for(;;){
        int shutdown = 0;
        int clientlen = sizeof(clientaddr);
        // debug("Waiting for accept...");
        SOCKET accsd = accept(sd, (struct sockaddr*)&clientaddr, &clientlen);
        // debug("Accepted...");
        if(accsd < 0){
            error(server->func, server->p, "accept failed: %s: %d", (int)accsd);
            closesocket(sd);
            WSACleanup();
            return 1;
        }
        ssize_t n = recv(accsd, buff, (sizeof buff)-1, 0);
        if(n < 0){
            error(server->func, server->p, "recv failed: %s: %zd", n);
            goto Close;
        }
        if(n == 0){
            info(server->func, server->p, "close connection");
            goto Close;
        }
        buff[n] = 0;
        shutdown = handle_request(server->func, server->p, flags, directory, accsd, (LongString){n, buff});
        Close:
        closesocket(accsd);
        if(shutdown) break;
    }
    closesocket(sd);
    WSACleanup();
    return 0;
}
static
int
handle_request(DndcErrorFunc*func, void*_Nullable p, uint64_t flags, LongString directory, SOCKET accsd, LongString request){
    // just assume everything is a GET, lol.
    StringView path = SV("index.dnd");
    StringView suffix = SV("");
    if(request.length > 6){
        LongString rest = {request.length-5, request.text+5};
        const char* space = strchr(rest.text, ' ');
        if(space && space != rest.text){
            path = (StringView){.text=rest.text, .length=space-rest.text};
            info(func, p, "Serving: %.*s", (int)path.length+1, path.text-1);
        }
        else {
            info(func, p, "Serving: %.*s", (int)path.length, path.text);
        }
    }
    if(SV_equals(path, SV("shutdown"))){
        #define MESS "HTTP/1.1 200 OK\r\n"
        send(accsd, MESS, (sizeof MESS)-1, 0);
        #undef MESS
        return 1;
    }
    if(endswith(path, SV(".html"))){
        path.length -= 5;
        suffix = SV(".dnd");
    }
    if(memmem(path.text, path.length, "..", 2)){
        error(func, p, ".. not allowed: '%.*s'", (int)path.length, path.text);
        goto LErr;
    }
    // debug("path: %.*s", (int)path.length, path.text);
    // debug("suffix: %.*s", (int)suffix.length, suffix.text);
    if(endswith(path, SV(".dnd")) || SV_equals(suffix, SV(".dnd"))){
        TextFileResult tfr = read_relative_file_with_suffix_conversion(directory, path, suffix);
        if(tfr.errored){
            error(func, p, "Error reading '%.*s': %d", (int)path.length, path.text, tfr.native_error);
            goto LErr;
        }
        int err = 0;
        LongString html = compile_file(func, p, directory, flags, path, tfr.result, &err);
        if(err){
            goto LErr;
        }
        char buff[1024];
        int n = snprintf(buff, sizeof buff, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", html.length);
        send(accsd, buff, n, 0);
        send(accsd, html.text, html.length, 0);
        dndc_free_string(tfr.result);
        dndc_free_string(html);
        return 0;
    }
    else {
        TextFileResult tfr = read_relative_file_with_suffix_conversion(directory, path, suffix);
        if(tfr.errored){
            error(func, p, "Error reading '%.*s': %d", (int)path.length, path.text, tfr.native_error);
            goto LErr;
        }
        char buff[1024];
        int n = snprintf(buff, sizeof buff, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", tfr.result.length);
        send(accsd, buff, n, 0);
        send(accsd, tfr.result.text, tfr.result.length, 0);
        dndc_free_string(tfr.result);
        return 0;
    }
    LErr:
    #define MESS "HTTP/1.1 404 Not-Found\r\n\r\n" \
        "<div align=center style=\"margin-top:10%; font-family: sans-serif;\">" \
        "<h1><span style=\"color:red;\">404</span>'ed! Not Found!</h1>" \
        "</div>" \
        "\r\n"
    send(accsd, MESS, (sizeof MESS)-1, 0);
    #undef MESS
    return 0;
}


#else

static
int
handle_request(DndcErrorFunc*func, void*_Nullable p, uint64_t flags, LongString directory, int accsd, LongString request);

struct DndServer {
    int sd;
    DndcErrorFunc* func;
    void*_Nullable p;
};

typedef struct DndServer DndServer;

DndServer*_Nullable
dnd_server_create(DndcErrorFunc* func, void*_Nullable p, int* port){
    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        error(func, p, "Socket failed: %s", strerror(errno));
        return NULL;
    }
    int opt = 1;
    int sso_err = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if(sso_err < 0){
        error(func, p, "setsockopt for SO_REUSEADDR failed: %s", strerror(errno));
        return NULL;
    }
    #if defined(__APPLE__)
    sso_err = setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof opt);
    if(sso_err < 0){
        error(func, p, "setsockopt for SO_NOSIGPIPE failed: %s", strerror(errno));
        return NULL;
    }
    #endif
    struct sockaddr_in addr = {
        #if defined(__APPLE__)
        .sin_len = sizeof(addr),
        #endif
        .sin_family = AF_INET,
        .sin_addr = {htonl(INADDR_LOOPBACK)},
        .sin_port = htons(*port),
    };
    int err = bind(sd, (struct sockaddr*)&addr, sizeof addr);
    if(err < 0){
        error(func, p, "Bind failed: %s", strerror(errno));
        close(sd);
        return NULL;
    }
    socklen_t addrlen = sizeof addr;
    err = getsockname(sd, (struct sockaddr*)&addr, &addrlen);
    if(err < 0){
        error(func, p, "getsockname failed: %s", strerror(errno));
        close(sd);
        return NULL;
    }
    err = listen(sd, SOMAXCONN);
    if(err < 0){
        error(func, p, "listen failed: %s", strerror(errno));
        close(sd);
        return NULL;
    }
    info(func, p, "Serving at http://localhost:%d", (int)ntohs(addr.sin_port));
    *port = (int)ntohs(addr.sin_port);
    DndServer* server = malloc(sizeof *server);
    server->sd = sd;
    server->func = func;
    server->p = p;
    return server;
}

int
dnd_server_serve(DndServer* server, uint64_t flags, LongString directory){
    int sd = server->sd;
    char buff[10000];
    struct sockaddr_in clientaddr = {0};
    DndcErrorFunc* func = server->func;
    void*_Nullable p = server->p;
    for(;;){
        socklen_t clientlen = sizeof(clientaddr);
        // debug("Waiting for accept...");
        int accsd = accept(sd, (struct sockaddr*)&clientaddr, &clientlen);
        // debug("Accepted...");
        if(accsd < 0){
            // error(func, p, "accept failed: %s", strerror(errno));
            close(sd);
            return 1;
        }
        ssize_t n = recv(accsd, buff, (sizeof buff)-1, 0);
        if(n < 0){
            error(func, p, "recv failed: %s", strerror(errno));
            goto Close;
        }
        if(n == 0){
            info(func, p, "close connection");
            goto Close;
        }
        buff[n] = 0;
        int shutdown = handle_request(server->func, server->p, flags, directory, accsd, (LongString){n, buff});
        Close:
        close(accsd);
        if(shutdown){
            debug(func, p, "got shutdown");
            break;
        }
    }
    debug(func, p, "Closing socket");
    close(sd);
    return 0;
}

static
int
handle_request(DndcErrorFunc* func, void*_Nullable p, uint64_t flags, LongString directory, int accsd, LongString request){
    // just assume everything is a GET, lol.
    StringView path = SV("index.dnd");
    StringView suffix = SV("");
    if(request.length > 6){
        LongString rest = {request.length-5, request.text+5};
        const char* space = strchr(rest.text, ' ');
        if(space && space != rest.text){
            path = (StringView){.text=rest.text, .length=space-rest.text};
            info(func, p, "Serving: %.*s", (int)path.length+1, path.text-1);
        }
        else {
            info(func, p, "Serving: %.*s", (int)path.length, path.text);
        }
    }
    if(endswith(path, SV(".html"))){
        path.length -= 5;
        suffix = SV(".dnd");
    }
    if(SV_equals(path, SV("shutdown"))){
        #define MESS "HTTP/1.1 200 OK\r\n"
        send(accsd, MESS, (sizeof MESS)-1, 0);
        #undef MESS
        return 1;
    }
    if(memmem(path.text, path.length, "..", 2)){
        error(func, p, ".. not allowed: '%.*s'", (int)path.length, path.text);
        goto LErr;
    }
    // debug("path: %.*s", (int)path.length, path.text);
    // debug("suffix: %.*s", (int)suffix.length, suffix.text);
    if(endswith(path, SV(".dnd")) || SV_equals(suffix, SV(".dnd"))){
        TextFileResult tfr = read_relative_file_with_suffix_conversion(directory, path, suffix);
        if(tfr.errored){
            error(func, p, "Error reading '%.*s': %s", (int)path.length, path.text, strerror(tfr.native_error));
            goto LErr;
        }
        int err = 0;
        LongString html = compile_file(func, p, directory, flags, path, tfr.result, &err);
        if(err){
            goto LErr;
        }
        char buff[1024];
        int n = snprintf(buff, sizeof buff, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", html.length);
        send(accsd, buff, n, 0);
        send(accsd, html.text, html.length, 0);
        dndc_free_string(tfr.result);
        dndc_free_string(html);
        return 0;
    }
    else {
        TextFileResult tfr = read_relative_file_with_suffix_conversion(directory, path, suffix);
        if(tfr.errored){
            error(func, p, "Error reading '%.*s': %s", (int)path.length, path.text, strerror(tfr.native_error));
            goto LErr;
        }
        char buff[1024];
        int n = snprintf(buff, sizeof buff, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", tfr.result.length);
        send(accsd, buff, n, 0);
        send(accsd, tfr.result.text, tfr.result.length, 0);
        dndc_free_string(tfr.result);
        return 0;
    }
    LErr:
    #define MESS "HTTP/1.1 404 Not-Found\r\n\r\n" \
        "<div align=center style=\"margin-top:10%; font-family: sans-serif;\">" \
        "<h1><span style=\"color:red;\">404</span>'ed! Not Found!</h1>" \
        "</div>" \
        "\r\n"
    send(accsd, MESS, (sizeof MESS)-1, 0);
    return 0;
}
#endif

static
void
vlogit(DndcErrorFunc* func, void*_Nullable p, int lvl, const char* msg, va_list args){
    char buff[4192];
    long len = vsnprintf(buff, sizeof buff, msg, args);
    func(p, lvl, "", 0, -1, -1, buff, len);
}

#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static
void
info(DndcErrorFunc* func, void*_Nullable p, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vlogit(func, p, DNDC_STATISTIC_MESSAGE, msg, args);
    va_end(args);
}

#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static
void
debug(DndcErrorFunc* func, void*_Nullable p, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vlogit(func,p, DNDC_DEBUG_MESSAGE, msg, args);
    va_end(args);
}

#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static
void
error(DndcErrorFunc* func, void*_Nullable p, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vlogit(func,p, DNDC_NODELESS_MESSAGE, msg, args);
    va_end(args);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif


#include "allocator.c"
