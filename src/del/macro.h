#pragma once

#ifdef _MSC_VER
#define DEL_EXTERN __declspec(dllexport)
#else
#define DEL_EXTERN __attribute__((visibility("default")))
#endif