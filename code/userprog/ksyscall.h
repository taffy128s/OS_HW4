/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"

#include "synchconsole.h"

typedef int OpenFileId;	

void SysHalt()
{
  kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

int SysCreate(char *filename, int size)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename, size);
}

OpenFileId SysOpen(char *name) {
    return kernel->interrupt->myOpen(name);
}

int SysRead(char *buffer, int size, OpenFileId id) {
    return kernel->interrupt->Read(buffer, size, id);
}

int SysWrite(char *buffer, int size, OpenFileId id) {
    return kernel->interrupt->Write(buffer, size, id);
}

int SysClose(OpenFileId id) {
    return kernel->interrupt->Close(id);
}

#endif /* ! __USERPROG_KSYSCALL_H__ */
