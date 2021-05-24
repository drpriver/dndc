#ifndef DNDC_API_DEF_H
#define DNDC_API_DEF_H
#ifdef _WIN32
#define DNDC_API __declspec(dllexport)
#else
#define DNDC_API extern
#endif
#endif
