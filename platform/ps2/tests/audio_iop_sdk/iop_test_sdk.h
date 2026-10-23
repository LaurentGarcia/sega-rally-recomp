// Original minimal declarations for testing our IOP module without a console.
#pragma once
#include <stdint.h>
typedef unsigned char u8;
typedef struct { int attr, option, initial, maximum; } iop_sema_t;
typedef struct { int attr, option; void (*entry)(void *); int stack, priority; } iop_thread_t;
typedef struct { int unused; } SifRpcDataQueue_t;
typedef struct { int unused; } SifRpcServerData_t;
#define IRX_ID(name, major, minor)
#define TH_C 1
#define MODULE_NO_RESIDENT_END 1
#define MODULE_RESIDENT_END 0
#define SD_PARAM_MVOLL 0x100
#define SD_PARAM_MVOLR 0x200
#define SD_PARAM_BVOLL 0x300
#define SD_PARAM_BVOLR 0x400
#define SD_PARAM_AVOLL 0x500
#define SD_PARAM_AVOLR 0x600
#define SD_TRANS_WRITE 0
#define SD_TRANS_LOOP 0x10
int iSignalSema(int);
int WaitSema(int);
int CreateSema(iop_sema_t *);
int DeleteSema(int);
int CreateThread(iop_thread_t *);
int StartThread(int, void *);
int TerminateThread(int);
int DeleteThread(int);
int GetThreadId(void);
int CpuSuspendIntr(int *);
int CpuResumeIntr(int);
int sceSdInit(int);
void sceSdSetParam(unsigned, unsigned);
void sceSdSetTransCallback(int, int (*)(void *));
int sceSdBlockTrans(int, unsigned, u8 *, unsigned, ...);
unsigned sceSdBlockTransStatus(int, int);
void SifInitRpc(int);
void SifSetRpcQueue(SifRpcDataQueue_t *, int);
void SifRegisterRpc(SifRpcServerData_t *, unsigned, void *(*)(int, void *, int),
                    void *, void *, void *, SifRpcDataQueue_t *);
void SifRpcLoop(SifRpcDataQueue_t *);
