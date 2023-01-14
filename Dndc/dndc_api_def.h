//
// Copyright © 2021-2023, David Priver
//
#ifndef DNDC_API_DEF_H
#define DNDC_API_DEF_H
#ifndef DNDC_API
#ifdef _WIN32
#define DNDC_API __declspec(dllexport)
#else
#define DNDC_API extern
#endif
#endif
#endif
