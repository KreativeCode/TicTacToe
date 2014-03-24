//////////////////////////////////////////////////////////
// C++ class for UNIX semaphores

// Filename:     Semaphore.H
// Author:       Geoffrey C. Speicher
// Written:      6/9/96
// Last updated: 12/9/96
//////////////////////////////////////////////////////////
// Copyright (c) 1996 Geoff Speicher
// All Rights Reserved.

// You may freely use this->software in any form,
// for any legal purpose.

// Distribution of source permissible provided that:
//      - no modifications are made
//      - source is available in its original form

// This software contains no warranty, implicit or
// expressed.  Use at your own risk.
//////////////////////////////////////////////////////////
// Defining SEMAPHORE_DEBUG will result in information
// about the Semaphore being printed before/after operations.

#ifndef _ooipc_Semaphore_H
#define _ooipc_Semaphore_H

#ifdef SEMAPHORE_DEBUG
#include <stdio.h>
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

extern "C" {
  int semget(key_t key, int nsems, int semflg);
//  int semctl(int semid, int semnum, int cmd, union semun arg);
  int semop(int semid, struct sembuf *sops, size_t nsops);
}

class Semaphore {
  int id;
public:
  Semaphore(int value, int key);
  void remove();
  inline void wait();
  inline void signal();
  inline void P();
  inline void V();
  inline int value();
};

Semaphore::Semaphore(int value, int key) {
  union semun arg;
  arg.val=value;
  if ((id = semget(key,1,0)) == -1) {
    if (errno==ENOENT) {
      id = semget(key,1,0777|IPC_CREAT);
      semctl(id,0,SETVAL,arg);
#ifdef SEMAPHORE_DEBUG
      printf("Semaphore created: id = %d; value = %d\n",id,this->value());
#endif
    } else {
      perror("Semaphore");
      exit(1);
    } 
  } else {
#ifdef SEMAPHORE_DEBUG
    printf("Semaphore attached: id = %d; value = %d\n",id,this->value());
#endif
  }
}

void Semaphore::wait() {
  struct sembuf	p_buf;
#ifdef SEMAPHORE_DEBUG
  printf("Semaphore wait:\n");
  printf("\tValue before wait(): %d (id=%d)\n",this->value(),id);
#endif
  p_buf.sem_num = 0;
  p_buf.sem_op = -1;	     // subtract 1 indicates P
  p_buf.sem_flg = 0;
  semop(id,&p_buf,1);
#ifdef SEMAPHORE_DEBUG
  printf("\tValue after wait(): %d (id=%d)\n",this->value(),id);
#endif
}

void Semaphore::signal() {
  struct sembuf	v_buf;
#ifdef SEMAPHORE_DEBUG
  printf("Semaphore signal:\n");
  printf("\tValue before signal(): %d (id=%d)\n",this->value(),id);
#endif
  v_buf.sem_num = 0;
  v_buf.sem_op = 1;          // add 1 indicates V
  v_buf.sem_flg = 0;
  semop(id,&v_buf,1);
#ifdef SEMAPHORE_DEBUG
  printf("\tValue after signal(): %d (id=%d)\n",this->value(),id);
#endif
}

void Semaphore::P() {
  wait();
}

void Semaphore::V() {
  signal();
}

int Semaphore::value() {
  union semun arg;
  return semctl(id,0,GETVAL,arg);
}

void Semaphore::remove() {
  if (id != -1) {
    union semun arg;
    semctl(id,0,IPC_RMID,arg);
#ifdef SEMAPHORE_DEBUG
    printf("Semaphore removed: id = %d\n",id);
#endif
    id = -1;
  }
}
#endif

