#ifndef DISPATCHER_H
#define DISPATCHER_H


#define DISPATCHER_TASK_SIZE 4096
#define DISPATCHER_TASK_PRIORITY 8
#define DISPATCHER_TASK_CORE 1


void DispatcherTaskMain(void *arg);

void SpawnDispatcherrTask(void);

void PassPduToDispatcher(void);


#endif