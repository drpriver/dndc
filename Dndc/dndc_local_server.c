//
// Copyright © 2021-2022, David Priver
//
#include "dndc.h"
#include "dndc_long_string.h"
#include "Utils/file_util.h"
#include "Utils/str_util.h"
#include "Utils/path_util.h"
#include "Allocators/allocator.h"
#include "Allocators/mallocator.h"
#include "Utils/msb_url_helpers.h"
#include "common_macros.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "Platform/Windows/windowsheader.h"
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
typedef int SOCKET; // easier this way

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

typedef struct DndLogger DndLogger;
struct DndLogger {
    DndcLogFunc* func;
    void*_Nullable p;
    int loglevel; // higher is more verbose
};

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

// Helpers for stringifying error codes
static inline
const char*
wsaerror(void){
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER;
    int err = WSAGetLastError();
    char* result = NULL;
    DWORD ret = FormatMessageA(flags, NULL, err, 0, (void*)&result, 0, NULL);
    if(!result) return "Error when formatting error";
    result[ret-1] = 0;
    return result;
}

static inline
const char*
os_error_mess(DWORD err){
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER;
    char* result = NULL;
    DWORD ret = FormatMessageA(flags, NULL, err, 0, (void*)&result, 0, NULL);
    if(!result) return "Error when formatting error";
    result[ret-1] = 0;
    return result;
}
static inline
void
free_error_mess(const char* mess){
    PushDiagnostic(); SuppressDiscardQualifiers(); SuppressCastQual();
    LocalFree((void*)mess);
    PopDiagnostic();
}
#endif
#if !defined(_WIN32)
const char* os_error_mess(int eno){
    return strerror(eno);
}
static inline
void
free_error_mess(const char* mess){
    (void)mess; // strerror should not be freed
}
#endif

static
TextFileResult
read_relative_file_with_suffix_conversion(LongString directory, StringView path, StringView suffix);

static
LongString
compile_file(const DndLogger*, LongString directory, uint64_t flags, StringView path, LongString text, int *error);

static void vlogit(const DndLogger*logger, int lvl, const char* msg, va_list args);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 2, 3)))
#endif
static void info(const DndLogger* logger, const char* msg, ...);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 2, 3)))
#endif
static void debug(const DndLogger* logger, const char* msg, ...);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 2, 3)))
#endif
static void error(const DndLogger* logger, const char* msg, ...);

static
TextFileResult
read_relative_file_with_suffix_conversion(LongString directory, StringView path, StringView suffix){
    char buff[1024];
    snprintf(buff, sizeof buff, "%s/%.*s%.*s", directory.text, (int)path.length, path.text, (int)suffix.length, suffix.text);
    return read_file(buff, get_mallocator());
}

static
LongString
compile_file(const DndLogger* logger, LongString directory, uint64_t flags, StringView path, LongString text, int *error){
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
        if(n < 0){
            *error = 1;
            return (LongString){0};
        }
        base = (StringView){.length = n, .text = buff};
    }
    StringView filename = {.length=path.length-(p-path.text), .text=p};
    LongString result = {0};
    *error = dndc_compile_dnd_file(flags, base, LS_to_SV(text), filename, &result, NULL, NULL, logger->func, logger->p, NULL, NULL, NULL, LS(""));
    return result;
}

static
int
handle_request(DndLogger*, uint64_t flags, LongString directory, SOCKET accsd, LongString request);

struct DndServer{
    SOCKET sd;
    DndLogger logger;
};

typedef struct DndServer DndServer;

#ifdef _WIN32
//
// winsock is just different enough that I'd rather keep the implementation separate.

DndServer*_Nullable
dnd_server_create(DndcLogFunc* func, void*_Nullable p, int loglevel, int* port){
    DndLogger logger = {.func=func, .p=p, .loglevel=loglevel};
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2,2), &wsadata);
    if(err){
        error(&logger, "some error on startup or something");
        return NULL;
    }
    SOCKET listensocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listensocket == INVALID_SOCKET){
        error(&logger, "Error in socket");
        goto cleanup;
    }
    BOOL opt = 1;
    err = setsockopt(listensocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof opt);
    if(err){
        const char* errmess = wsaerror();
        error(&logger, "setsockopt for SO_REUSEADDR failed: %s (%d)", errmess, WSAGetLastError());
        free_error_mess(errmess);
        goto cleanup;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.S_un.S_addr=htonl(INADDR_LOOPBACK),
        .sin_port = htons(*port),
    };

    err = bind(listensocket, (struct sockaddr*)&addr, sizeof addr);
    if(err){
        const char* errmess = wsaerror();
        error(&logger, "bind error (%s): %d", errmess, WSAGetLastError());
        error(&logger, "port was: %d", *port);
        free_error_mess(errmess);
        goto cleanup;
    }

    int addrlen = sizeof addr;
    err = getsockname(listensocket, (struct sockaddr*)&addr, &addrlen);
    if(err){
        const char* errmess = wsaerror();
        error(&logger, "getsockname error: %s (%d)", errmess, WSAGetLastError());
        free_error_mess(errmess);
        goto cleanup;
    }

    err = listen(listensocket, SOMAXCONN);
    if(err){
        const char* errmess = wsaerror();
        error(&logger, "listen error: %s (%d)", errmess, WSAGetLastError());
        free_error_mess(errmess);
        goto cleanup;
    }
    info(&logger, "Serving at http://localhost:%d", (int)ntohs(addr.sin_port));
    *port = (int)ntohs(addr.sin_port);
    DndServer* server = malloc(sizeof *server);
    server->sd = listensocket;
    server->logger = logger;
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
            const char* errmess = wsaerror();
            error(&server->logger, "accept failed: %s: %d", errmess, (int)accsd);
            free_error_mess(errmess);
            closesocket(sd);
            WSACleanup();
            return 1;
        }
        ssize_t n = recv(accsd, buff, (sizeof buff)-1, 0);
        if(n < 0){
            const char* errmess = wsaerror();
            error(&server->logger, "recv failed: %s: %zd", errmess, n);
            free_error_mess(errmess);
            goto Close;
        }
        if(n == 0){
            info(&server->logger, "close connection");
            goto Close;
        }
        buff[n] = 0;
        shutdown = handle_request(&server->logger, flags, directory, accsd, (LongString){n, buff});
        Close:
        closesocket(accsd);
        if(shutdown) break;
    }
    closesocket(sd);
    WSACleanup();
    return 0;
}

#else



DndServer*_Nullable
dnd_server_create(DndcLogFunc* func, void*_Nullable p, int loglevel, int* port){
    DndLogger logger = {.func=func,.p=p, .loglevel=loglevel};
    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        error(&logger, "Socket failed: %s", strerror(errno));
        return NULL;
    }
    int opt = 1;
    int sso_err = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if(sso_err < 0){
        error(&logger, "setsockopt for SO_REUSEADDR failed: %s", strerror(errno));
        return NULL;
    }
    #if defined(__APPLE__)
    sso_err = setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof opt);
    if(sso_err < 0){
        error(&logger, "setsockopt for SO_NOSIGPIPE failed: %s", strerror(errno));
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
        error(&logger, "Bind failed: %s", strerror(errno));
        close(sd);
        return NULL;
    }
    socklen_t addrlen = sizeof addr;
    err = getsockname(sd, (struct sockaddr*)&addr, &addrlen);
    if(err < 0){
        error(&logger, "getsockname failed: %s", strerror(errno));
        close(sd);
        return NULL;
    }
    err = listen(sd, SOMAXCONN);
    if(err < 0){
        error(&logger, "listen failed: %s", strerror(errno));
        close(sd);
        return NULL;
    }
    info(&logger, "Serving at http://localhost:%d", (int)ntohs(addr.sin_port));
    *port = (int)ntohs(addr.sin_port);
    DndServer* server = malloc(sizeof *server);
    server->sd = sd;
    server->logger = logger;
    return server;
}

int
dnd_server_serve(DndServer* server, uint64_t flags, LongString directory){
    int sd = server->sd;
    char buff[10000];
    struct sockaddr_in clientaddr = {0};
    for(;;){
        int shutdown = 0;
        socklen_t clientlen = sizeof(clientaddr);
        debug(&server->logger, "Waiting for accept...");
        SOCKET accsd = accept(sd, (struct sockaddr*)&clientaddr, &clientlen);
        debug(&server->logger, "Accepted...");
        if(accsd < 0){
            // error(func, p, "accept failed: %s", strerror(errno));
            close(sd);
            return 1;
        }
        ssize_t n = recv(accsd, buff, (sizeof buff)-1, 0);
        if(n < 0){
            error(&server->logger, "recv failed: %s", strerror(errno));
            goto Close;
        }
        if(n == 0){
            info(&server->logger, "close connection");
            goto Close;
        }
        buff[n] = 0;
        shutdown = handle_request(&server->logger, flags, directory, accsd, (LongString){n, buff});
        Close:
        close(accsd);
        if(shutdown){
            info(&server->logger, "got shutdown");
            break;
        }
    }
    debug(&server->logger, "Closing socket");
    close(sd);
    return 0;
}

#endif

static const LongString INDEXTEXT = LS(
    "Index::title\n"
    "::js\n"
    "  let paths = FileSystem.list_dnd_files();\n"
    "  let s = '';\n"
    "  for(let path of paths){\n"
    "     s += `* [${path}]\\n`\n"
    "     ctx.add_link(path, encodeURI(path));\n"
    "   }\n"
    "   ctx.root.parse(s);\n"
    "::css\n"
    "  body > * {\n"
    "    margin: auto;\n"
    "    width: max-content;\n"
    "  }\n"
);

static
int
handle_request(DndLogger* logger, uint64_t flags, LongString directory, SOCKET accsd, LongString request){
    // just assume everything is a GET, lol.
    MStringBuilder urlsb = {.allocator=get_mallocator()};
    StringView path = SV("index.dnd");
    StringView suffix = SV("");
    if(request.length > 6){
        LongString rest = {request.length-5, request.text+5};
        const char* space = strchr(rest.text, ' ');
        if(space && space != rest.text){
            path = (StringView){.text=rest.text, .length=space-rest.text};
            int decoderr = msb_url_percent_decode(&urlsb, path.text, path.length);
            if(decoderr){
                logger->func(logger->p, DNDC_NODELESS_MESSAGE, "", 0, -1, -1, "Bad percent decode", sizeof("Bad percent decode")-1);
                goto LNotFound;
            }
            path = msb_borrow_sv(&urlsb);
            logger->func(logger->p, DNDC_STATISTIC_MESSAGE, path.text-1, path.length+1, -1, -1, "Serving", sizeof("Serving")-1);
        }
        else {
            logger->func(logger->p, DNDC_STATISTIC_MESSAGE, path.text, path.length, -1, -1, "Serving", sizeof("Serving")-1);
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
        msb_destroy(&urlsb);
        goto LShutdown;
    }
    if(memmem(path.text, path.length, "..", 2)){
        error(logger, ".. not allowed: '%.*s'", (int)path.length, path.text);
        goto LNotFound;
    }
    debug(logger, "path: '%.*s' suffix: '%.*s'", (int)path.length, path.text, (int)suffix.length, suffix.text);
    if(endswith(path, SV(".dnd")) || SV_equals(suffix, SV(".dnd"))){
        TextFileResult tfr = read_relative_file_with_suffix_conversion(directory, path, suffix);
        LongString text = LS("");
        if(tfr.errored){
            StringView bn = path_basename(path);
            if(SV_equals(bn, SV("index")) || SV_equals(bn, SV("index.dnd"))){
                text = INDEXTEXT;
            }
            else {
                const char* errmess = os_error_mess(tfr.native_error);
                error(logger, "Error reading '%.*s': %s", (int)path.length, path.text, errmess);
                free_error_mess(errmess);
                goto LNotFound;
            }
        }
        else
            text = tfr.result;
        int err = 0;
        LongString html = compile_file(logger, directory, flags, path, text, &err);
        if(err){
            #define MESS "HTTP/1.1 500 Compiler-Error\r\n\r\n" \
            "<div align=center style=\"margin-top:10%; font-family: sans-serif;\">" \
            "<h1><span style=\"color:red;\">Error</span>Error compiling file.</h1>" \
            "</div>" \
            "\r\n"
            send(accsd, MESS, (sizeof MESS)-1, 0);
            #undef MESS
            goto LOk;
        }
        char buff[1024];
        int n = snprintf(buff, sizeof buff, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", html.length);
        send(accsd, buff, n, 0);
        send(accsd, html.text, html.length, 0);
        dndc_free_string(tfr.result);
        dndc_free_string(html);
        goto LOk;
    }
    else {
        TextFileResult tfr = read_relative_file_with_suffix_conversion(directory, path, suffix);
        if(tfr.errored){
            const char* errmess = os_error_mess(tfr.native_error);
            error(logger, "Error reading '%.*s': %s", (int)path.length, path.text, errmess);
            free_error_mess(errmess);
            goto LNotFound;
        }
        char buff[1024];
        int n = snprintf(buff, sizeof buff, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", tfr.result.length);
        send(accsd, buff, n, 0);
        send(accsd, tfr.result.text, tfr.result.length, 0);
        dndc_free_string(tfr.result);
        goto LOk;
    }
    LNotFound:
    #define MESS "HTTP/1.1 404 Not-Found\r\n\r\n" \
        "<div align=center style=\"margin-top:10%; font-family: sans-serif;\">" \
        "<h1><span style=\"color:red;\">404</span>'ed!<br>Not Found!</h1>" \
        "</div>" \
        "\r\n"
    send(accsd, MESS, (sizeof MESS)-1, 0);
    #undef MESS
    msb_destroy(&urlsb);
    return 0;
    LOk:
    msb_destroy(&urlsb);
    return 0;
    LShutdown:
    msb_destroy(&urlsb);
    return 1;
}

static
void
vlogit(const DndLogger* logger, int lvl, const char* msg, va_list args){
    char buff[4192];
    long len = vsnprintf(buff, sizeof buff, msg, args);
    logger->func(logger->p, lvl, "", 0, -1, -1, buff, len);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 2, 3)))
#endif
static
void
info(const DndLogger* logger, const char* msg, ...){
    if(logger->loglevel < DNDC_STATISTIC_MESSAGE) return;
    va_list args;
    va_start(args, msg);
    vlogit(logger, DNDC_STATISTIC_MESSAGE, msg, args);
    va_end(args);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 2, 3)))
#endif
static
void
debug(const DndLogger* logger, const char* msg, ...){
    if(logger->loglevel < DNDC_DEBUG_MESSAGE) return;
    va_list args;
    va_start(args, msg);
    vlogit(logger, DNDC_DEBUG_MESSAGE, msg, args);
    va_end(args);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 2, 3)))
#endif
static
void
error(const DndLogger* logger, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vlogit(logger, DNDC_NODELESS_MESSAGE, msg, args);
    va_end(args);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif


#include "Allocators/allocator.c"
