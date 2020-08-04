#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <sys/resource.h>
#include "condvar.h"

int create(int value, char key[32]) {
  return syscall(__NR_cs1550_create, value, key);
}

int open(char key[32]) {
  return syscall(__NR_cs1550_open, key);
}

int down(int sem_id) {
  return syscall(__NR_cs1550_down, sem_id);
}

int up(int sem_id) {
  return syscall(__NR_cs1550_up, sem_id);
}

int close(int sem_id) {
  return syscall(__NR_cs1550_close, sem_id);
}

void cs1550_init_lock(struct cs1550_lock *l,
                      char key[32]) {
  l->nextCount = 0;
  l->mutex = create(1, key);
  if(l->mutex < 0){
    printf("Error initializing lock.\n");
    return;
  }
  l->next = create(0, key);
  if(l->next < 0){
    printf("Error initializing lock.\n");
    return;
  }
}

void cs1550_acquire(struct cs1550_lock *l){
  int result = down(l->mutex);
  if(result < 0){
    printf("Error acquiring lock.\n");
    return;
  }
  // printf("%d: acquiring lock\n", getpid());
}

void cs1550_release(struct cs1550_lock *l) {
  int result;
  if (l->nextCount > 0)
    result = up(l->next);
  else
    result = up(l->mutex);
  if(result < 0){
    printf("Error releasing lock.\n");
    return;
  }
  // printf("%d: releasing lock\n", getpid());
}

void cs1550_close_lock(struct cs1550_lock *l) {
  int result = close(l->mutex);
  if(result < 0){
    printf("Error closing lock.\n");
    return;
  }
  result = close(l->next);
  if(result < 0){
    printf("Error closing lock.\n");
    return;
  }
}


void cs1550_init_condition(struct cs1550_condition *cond,
                           struct cs1550_lock *l, char key[32]){
  cond->semCount = 0;
  cond->l = l;
  cond->condSem = create(0, key);
  if(cond->condSem < 0){
    printf("Error initializing condition variable.\n");
    return;
  }
}

void cs1550_wait(struct cs1550_condition *cond){
  int result;
  cond->semCount += 1;
  if (cond->l->nextCount > 0)
    result = up(cond->l->next);
  else
    result = up(cond->l->mutex);
  if(result < 0){
    printf("Error waiting on condition variable.\n");
    return;
  }
  result = down(cond->condSem);
  if(result < 0){
    printf("Error waiting on condition variable.\n");
    return;
  }
  // printf("%d: waking up\n", getpid());
  cond->semCount -= 1;
}

void cs1550_signal(struct cs1550_condition *cond){
  int result;
  if (cond->semCount > 0) {
    cond->l->nextCount += 1;
    result = up(cond->condSem);
    if(result < 0){
      printf("Error signaling on condition variable.\n");
      return;
    }
    result = down(cond->l->next);
    if(result < 0){
      printf("Error signaling on condition variable.\n");
      return;
    }
    cond->l->nextCount -= 1;
    // printf("%d: resuming\n", getpid());
  }
}

void cs1550_close_condition(struct cs1550_condition *cond) {
  int result = close(cond->condSem);
  if(result < 0){
    printf("Error closing condition variable.\n");
    return;
  }
}
