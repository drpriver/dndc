//
// Copyright © 2021-2023, David Priver
//
#ifndef DNDC_LOCAL_SERVER_H
#define DNDC_LOCAL_SERVER_H
#include "dndc_long_string.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif
typedef struct DndServer DndServer;

DndServer*_Nullable dnd_server_create(DndcLogFunc*logfunc, void*_Nullable logdata, int loglevel, int* port, _Bool bind_all);

int dnd_server_serve(DndServer* server, uint64_t flags, LongString directory);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
