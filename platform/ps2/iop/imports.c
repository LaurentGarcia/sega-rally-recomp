// SDK-generated import stubs; no SDK implementation is copied here.
#include <irx.h>
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <sifrpc.h>
#include <libsd.h>
#include <sysclib.h>

thbase_IMPORTS_start
I_CreateThread
I_DeleteThread
I_StartThread
I_TerminateThread
I_GetThreadId
thbase_IMPORTS_end
thsemap_IMPORTS_start
I_CreateSema
I_DeleteSema
I_iSignalSema
I_WaitSema
thsemap_IMPORTS_end
intrman_IMPORTS_start
I_CpuSuspendIntr
I_CpuResumeIntr
intrman_IMPORTS_end
sifcmd_IMPORTS_start
I_sceSifInitRpc
I_sceSifSetRpcQueue
I_sceSifRegisterRpc
I_sceSifRpcLoop
sifcmd_IMPORTS_end
libsd_IMPORTS_start
I_sceSdInit
I_sceSdSetParam
I_sceSdSetTransCallback
I_sceSdBlockTrans
I_sceSdBlockTransStatus
libsd_IMPORTS_end
sysclib_IMPORTS_start
I_memcpy
sysclib_IMPORTS_end
