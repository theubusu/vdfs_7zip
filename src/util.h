#include <cstdio>
#include <string>

#ifdef DEBUG

extern FILE* g_logF;

#  define DBG_LOG(...) \
      do { \
          g_logF = fopen("C:\\Users\\tomek\\dev2nd\\Projects\\v7z\\bin\\run.log", "a"); \
          fprintf(g_logF, __VA_ARGS__); \
          fclose(g_logF); \
      } while (false)

#else

#  define DBG_LOG(...)

#endif

std::wstring getDLLlocation();