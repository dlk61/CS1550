#ifndef _CONDVAR_H_
#define _CONDVAR_H_
#ifndef __NR_cs1550_create
#define __NR_cs1550_create  325
#endif

#ifndef __NR_cs1550_open
#define __NR_cs1550_open  326
#endif

#ifndef __NR_cs1550_down
#define __NR_cs1550_down  327
#endif

#ifndef __NR_cs1550_up
#define __NR_cs1550_up  328
#endif

#ifndef __NR_cs1550_close
#define __NR_cs1550_close  329
#endif

struct cs1550_lock {
  int mutex; //mutex semaphore
  int next;  //next semaphore
  int nextCount;
};

struct cs1550_condition {
  struct cs1550_lock *l;
  int condSem; //condsem semaphore
  int semCount;
};



#endif
