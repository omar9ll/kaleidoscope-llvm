// Built-in functions that Kaleidoscope programs can call with `extern`.
#include <cstdio>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT double putchard(double x) { std::fputc(static_cast<char>(x), stdout); return 0; }
extern "C" DLLEXPORT double printd(double x) { std::printf("%g\n", x); return 0; }
