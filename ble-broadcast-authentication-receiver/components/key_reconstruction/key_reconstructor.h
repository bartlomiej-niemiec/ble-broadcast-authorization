#ifndef KEY_RECONSTRUCTOR_H
#define KEY_RECONSTRUCTOR_H

#define KEY_RECONSTRUCTION_TASK_SIZE 4096
#define RECONSTRUCTOR_TASK_CORE 1
#define RECONSTRUCTOR_TASK_PRIORITY 10

void ReconstructorTaskMain(void *arg);

void SpawnRecontructionKeyTask(void);


#endif