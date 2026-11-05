/* avp360: crtdbg.h do MSVC (heap de debug). Nada disso existe no console. */
#ifndef AVP360_CRTDBG_H
#define AVP360_CRTDBG_H
#define _ASSERT(x) ((void)0)
#define _ASSERTE(x) ((void)0)
#define _RPT0(t, m) ((void)0)
#define _RPT1(t, m, a) ((void)0)
#define _RPT2(t, m, a, b) ((void)0)
#define _CrtSetDbgFlag(f) 0
#define _CrtCheckMemory() 1
#define _CrtDumpMemoryLeaks() 0
#define _CRTDBG_ALLOC_MEM_DF 1
#define _CRTDBG_LEAK_CHECK_DF 0x20
#define _CRT_WARN 0
#define _CRT_ERROR 1
#define _CRT_ASSERT 2
typedef int (*_CRT_REPORT_HOOK)(int, char *, int *);
#define _CrtSetReportHook(f) ((_CRT_REPORT_HOOK)0)
#define _CrtSetReportMode(t, m) 0
#define _CRTDBG_MODE_DEBUG 2
#define _CRTDBG_MODE_WNDW 4
/* malloc.h: inspecao do heap do MSVC (MemoryUtils.cpp). Sem isso no newlib: o heap e sempre "OK" e vazio. */
#define _HEAPOK (-2)
#define _HEAPEMPTY (-1)
#define _HEAPBADBEGIN (-3)
#define _HEAPBADNODE (-4)
#define _HEAPEND (-5)
#define _HEAPBADPTR (-6)
#define _USEDENTRY 1
#define _FREEENTRY 0
typedef struct _heapinfo { int *_pentry; size_t _size; int _useflag; } _HEAPINFO;
static inline int _heapchk(void) { return _HEAPOK; }
static inline int _heapwalk(_HEAPINFO *h) { (void)h; return _HEAPEND; }
static inline int _heapmin(void) { return 0; }
#endif
