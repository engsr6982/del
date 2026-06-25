#pragma once

#ifdef _MSC_VER
#define DEL_EXTERN __declspec(dllexport)
#else
#define DEL_EXTERN __attribute__((visibility("default")))
#endif

#define DEL_DISABLE_COPY(C)                                                                                            \
  C(C const&)            = delete;                                                                                     \
  C& operator=(C const&) = delete

#define DEL_DISABLE_MOVE(C)                                                                                            \
  C(C&&)            = delete;                                                                                          \
  C& operator=(C&&) = delete

#define DEL_DISABLE_COPY_MOVE(C)                                                                                       \
  DEL_DISABLE_COPY(C);                                                                                                 \
  DEL_DISABLE_MOVE(C)
