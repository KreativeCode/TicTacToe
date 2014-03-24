//////////////////////////////////////////////////////////
// C++ class for shared memory in UNIX

// Filename:     Shared.H
// Author:       Geoffrey C. Speicher
// Written:      6/9/96
// Last updated: 10/28/96
//////////////////////////////////////////////////////////
// Copyright (c) 1996 Geoff Speicher
// All Rights Reserved.

// You may freely use this software in any form,
// for any legal purpose.

// Distribution of source permissible provided that:
//      - no modifications are made
//      - source is available in its original form

// This software contains no warranty, implicit or
// expressed.  Use at your own risk.
//////////////////////////////////////////////////////////
// Defining SHARED_DEBUG will print out information
// about the shared memory as it is attached and detached.

#ifndef _ooipc_Shared_H
#define _ooipc_Shared_H

#ifdef SHARED_DEBUG
#include <stdio.h>
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

extern "C" {
  int shmget(key_t key, size_t size, int shmflg);
  void *shmat(int shmid, const void *shmaddr, int shmflg);
  int shmdt(const void *shmaddr);
  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
  void exit(int);
}

template<class T> class Shared;
template<class T> T* operator +(Shared<T>& sh, int offset);
template<class T> T& operator *(Shared<T>& sh);
template<class T> class Shared {
  T *addr;
  int sz;
  int id;
public:
  Shared(int size, int key, int shmaddr =0);
  Shared(): addr(0), sz(0), id(0) { };
  ~Shared();
  void alloc(int size, int key, int shmaddr =0);
  int size();
  inline operator T*();
  inline T& operator [](int index);
  friend T* operator +<>(Shared<T>& sh, int offset);
  friend T& operator *<>(Shared<T>& sh);
  	T* operator ->();
  void remove();
};

template<class T> Shared<T>::Shared(int size, int key, int shmaddr) {
  sz = size;
  id = shmget(key,sz*sizeof(T),0777|IPC_CREAT);
  if (id == -1) {
    perror("Shared->shmget():");
    exit(1);
  }
  addr = (T*)shmat(id,(char*)shmaddr,0);
#ifdef SHARED_DEBUG
  printf("Shared memory attached: id = %d; Size = %d*%d; Addr = %u\n",
	 id,sz,sizeof(T),addr);
#endif
  if ((long)addr == -1) {
    perror("Shared->shmat():");
    exit(1);
  }
}

template<class T> Shared<T>::~Shared() {
  if (addr != NULL) {
    shmdt((char*)addr);
#ifdef SHARED_DEBUG
    printf("Shared memory detached: id = %d\n",id);
#endif
    addr = NULL;
    sz = 0;
    id = 0;
  }
}

template<class T> void Shared<T>::alloc(int size, int key, int shmaddr) {
  if (addr != NULL) {
    shmdt((char*)addr);
#ifdef SHARED_DEBUG
    printf("Shared memory detached: id = %d\n",id);
#endif
  }
  sz = size;
  id = shmget(key,sz*sizeof(T),0777|IPC_CREAT);
  if (id == -1) {
    perror("Shared.alloc()->shmget():");
    exit(1);
  }
  addr = (T*)shmat(id,(char*)shmaddr,0);
#ifdef SHARED_DEBUG
  printf("Shared memory alloc'd: id = %d; Size = %d*%d; Addr = %u\n",
	 id,sz,sizeof(T),addr);
#endif
  if ((int)addr == -1) {
    perror("Shared.alloc()->shmat():");
    exit(1);
  }
}

template<class T> int Shared<T>::size() {
  return sz*sizeof(T);
}

template<class T> Shared<T>::operator T*() {
  return addr;
}

template<class T> T& Shared<T>::operator [](int index) {
  return *(addr + index);
  //return *(addr + sizeof(T)*index);
}

template<class T> T* operator +(Shared<T>& sh, int offset) {
  return (sh.addr + offset);
  //return (sh.addr + sizeof(T)*offset);
}

template<class T> T& operator *(Shared<T>& sh) {
  return *sh.addr;
}

template<class T> void Shared<T>::remove() {
  shmctl(id,IPC_RMID,0);
#ifdef SHARED_DEBUG
  printf("Shared memory removed: id = %d\n",id);
#endif
  addr = NULL;
  sz = 0;
  id = 0;
}
template<class T> T* Shared<T>::operator ->() {
 return (addr);
}
#endif

