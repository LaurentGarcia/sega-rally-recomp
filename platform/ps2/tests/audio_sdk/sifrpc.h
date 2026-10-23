#pragma once
struct SifRpcClientData_t { void *server; };
void SifInitRpc(int);
int SifBindRpc(SifRpcClientData_t *, unsigned, int);
int SifCallRpc(SifRpcClientData_t *, unsigned, int, void *, int, void *, int,
               void (*)(void *), void *);
