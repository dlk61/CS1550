#include <sys/mman.h>
#include <stdio.h>
#include "condvar.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

struct shared_mem {
    struct cs1550_lock l;
    struct cs1550_condition visitorGo;
    struct cs1550_condition outsideGuideGo;
    struct cs1550_condition insideGuideGo;
    struct timeval start, end;
    bool open;                                  //is museum open or closed?
    int waitingVisitors;                        //number of visitors waiting outside
    int waitingOutsideGuides;                   //number of tour guides waiting outside to enter
    int waitingInsideGuides;                    //number of tour guides waiting inside to leave
    int visitorsToured;                         //number of visitors that have toured the museum
    int activeVisitors;                         //number of visitors inside the museum
    int activeGuides;                           //number of tour guides inside the museum
};

//global pointer to shared_mem struct
struct shared_mem* sm;

//declare functions
void visitorArrives();
static bool visitorShouldWait();
void tourMuseum();
void visitorLeaves();
void tourguideArrives();
static bool outsideGuidesShouldWait();
void openMuseum();
void tourguideLeaves();
static bool insideGuidesShouldWait();

//argc is number of arguments passed, argv[] is array of arguments
int main(int argc, char *argv[])
{
    //if amount of arguments passed is not 17, stop program
    if (argc != 17) {
        fprintf(stderr, "\n17 arguments are needed.");
        return 1;
    }

    //To have multiple processes share same memory region, ask for the size of shared_mem bytes of RAM from OS directly by using mmap()
    sm = (struct shared_mem*) mmap(NULL, sizeof(struct shared_mem),PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    
    //get time at start of program
    gettimeofday(&sm->start, NULL);

    //initialize arguments to 0
    int m, k, pv, dv, sv, pg, dg, sg = 0;

    //convert arguments to integers
    m = atoi(argv[2]);     // m is number of visitors 
    k = atoi(argv[4]);     // k is number of tour guides
    pv = atoi(argv[6]);    // pv is probability of a visitor immediately following another visitor
    dv = atoi(argv[8]);    // dv is delay in seconds when a visitor does not immediately follow another visitor
    sv = atoi(argv[10]);   // sv is random seed for the visitor arrival process
    pg = atoi(argv[12]);   // pg is probability of a tour guide immediately following another tour guide
    dg = atoi(argv[14]);   // dg is delay in seconds when a tour guide does not immediately follow another tour guide
    sg = atoi(argv[16]);   // sg is random seed for the tour guide arrival process

    //initialize lock and conition variables
    cs1550_init_lock(&sm->l, "TestKey1");
    cs1550_init_condition(&sm->visitorGo, &sm->l, "TestKey2");
    cs1550_init_condition(&sm->outsideGuideGo, &sm->l, "TestKey3");
    cs1550_init_condition(&sm->insideGuideGo, &sm->l, "TestKey4");

    //initialize integers to 0
    int i, j, l, n, x, value = 0;

    /*Main process creates the visitor generator process */
    int pid = fork();

    //if pid is less than 0, print an error message
    if (pid < 0) 
        fprintf(stderr, "Error forking visitor generator process.\n"); 
    
    else if (pid == 0) {
        /*Visitor generator process */
        srand(sv); //set seed for generator
        //fork m times to create m visitors
        for (i = 0; i < m; i++) {
            pid = fork();

            //if pid is less than 0, print an error message
            if (pid < 0) {
                fprintf(stderr, "Error forking visitor process.\n"); 
            }
            else if (pid == 0) {
                //Visitor process: tour logic
                visitorArrives();
                tourMuseum();
                visitorLeaves();
                //explicitly call exit so the visitor doesn't go back up to for loop and fork a process
                exit(0);
            }
            else {
                /* Generator visitor: proceed to generate next visitor */
                value = rand() % 100 + 1;  //random number between 1 and 100
                if (value > pv)            //pv% chance delay
                    sleep(dv);             //sleep for dv seconds
            }
        }
        //wait m times so the parent (visitor generator process) can wait for its children (visitors)
        for (j = 0; j < m; j++) 
            wait(NULL);
    }
    else {
        /* Main process (reused as guide generator process) */
        srand(sg);  //set seed for generator
        //fork k times to create k guides
        for ( l = 0; l < k; l++) {
            pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Error forking tour guide process.\n"); 
            }
            else if (pid == 0) {
                //guide process: guide logic
                tourguideArrives();
                openMuseum();
                tourguideLeaves();
                //explicitly call exit so the tour guide doesn't go back up to for loop and fork a process
                exit(0);
            }
            else {
                /* Generator guide: proceed to generate next guide */
                int value = rand() % 100 + 1;  //random number between 1 and 100
                if (value > pg)                //pg% chance delay
                    sleep(dg);                 //sleep for dg seconds
            }
        }
        //wait k times so the parent (main/guide generator process) can wait for the tour guides so the tour guides don't become orphan
        for ( n = 0; n < k; n++) 
            wait(NULL);
        //wait once so the parent (main/guide generator process) can wait for the visitor generator process so the visititor generator process doesn't become orphan
        wait(NULL);
       
        //close condiditions variables and locks
        cs1550_close_condition(&sm->visitorGo);
        cs1550_close_condition(&sm->outsideGuideGo);
        cs1550_close_condition(&sm->insideGuideGo);
        cs1550_close_lock(&sm->l);
    }

    return 0;
}

void visitorArrives() {

    cs1550_acquire(&sm->l);               //acquire lock
    int i, fixed = 0;                     //initalize integers to 0
    gettimeofday(&sm->end, NULL);         //get time                   
    //calculate the time since the start of the program                              
    long elapsed = ((sm->end.tv_sec * 1000000 + sm->end.tv_usec ) - (sm->start.tv_sec * 1000000+ sm->start.tv_usec));
    fprintf(stderr, "Visitor %d arrives at time %ld \n", getpid(), elapsed / 1000000);   //print the time since start of program
    sm->waitingVisitors++;               //increment waitingVisitors
    fixed = sm->waitingOutsideGuides;    //set the number of waiting outside guides equal to the integer fixed
    for (i = 0; i < fixed; i++)          //wake up the tour guides waiting outside
        cs1550_signal(&sm->outsideGuideGo);                                             
    while (visitorShouldWait())          //wait until visitor can enter        
        cs1550_wait(&sm->visitorGo);                                               
    sm->waitingVisitors--;               //decrement waitingVisitors
    sm->activeVisitors++;                //increment waitingVisitors since it got passed the while loop                                   
    sm->visitorsToured++;                //increment the visitors toured
    cs1550_release(&sm->l);              //release lock                                              
    
}

static bool visitorShouldWait() {
    //visitor waits if capacity is reached or museum is not open
    return (sm->visitorsToured >= (sm->activeGuides * 10)) || (!sm->open);

}

void tourMuseum() {
    cs1550_acquire(&sm->l);             //acquire the lock
    gettimeofday(&sm->end, NULL);       //grab the time
    //calculate the time since the start of the program
    long elapsed = ((sm->end.tv_sec * 1000000 + sm->end.tv_usec ) - (sm->start.tv_sec * 1000000+ sm->start.tv_usec));
    fprintf(stderr, "Visitor %d tours the museum at time %ld \n", getpid(), elapsed / 1000000);     //print the time since start of program                                                                                                                 
    cs1550_release(&sm->l);            //release lock
    sleep(2);                          //let visitor tour for 2 seconds without blocking another visitor                                             
        
}

void visitorLeaves() {
    cs1550_acquire(&sm->l);            //acquire lock
    int i, fixed = 0;                  //initalize integers to 0
    gettimeofday(&sm->end, NULL);      //get time   
    //calculate the time since the start of the program
    long elapsed = ((sm->end.tv_sec * 1000000 + sm->end.tv_usec ) - (sm->start.tv_sec * 1000000+ sm->start.tv_usec));                               
    fprintf(stderr, "Visitor %d leaves the museum at time %ld \n", getpid(), elapsed / 1000000);     //print the time since start of program
    sm->activeVisitors--;             //decrement active visitors                                               
    fixed = sm->activeGuides;         //set the number of active guides equal to the integer fixed
    for (i = 0; i < fixed; i++)       //wake up guides waiting inside in case they wanted to leave
        cs1550_signal(&sm->insideGuideGo);                                                  
    cs1550_release(&sm->l);           //release lock                                               
       
}

void tourguideArrives() {
    cs1550_acquire(&sm->l);           //acquire lock                                             
    int i, fixed = 0;                 //initalize integers to 0
    gettimeofday(&sm->end, NULL);     //get time       
    //calculate the time since the start of the program                                                  
    long elapsed = ((sm->end.tv_sec * 1000000 + sm->end.tv_usec ) - (sm->start.tv_sec * 1000000+ sm->start.tv_usec));
    fprintf(stderr, "Tour guide %d arrives at time %ld \n", getpid(), elapsed / 1000000);          //print the time since start of program
    sm->waitingOutsideGuides++;       //increase guides waiting outside
    fixed = sm->waitingVisitors;      //set the number of waiting visitors equal to the integer fixed                                                    
    while (outsideGuidesShouldWait()) //wait until outisde guide can enter
        cs1550_wait(&sm->outsideGuideGo);                                               
    sm->waitingOutsideGuides--;       //decrement guides waiting outside
    cs1550_release(&sm->l);           //release lock
}

static bool outsideGuidesShouldWait() {
    //tour guides outside should wait when the museum is closed and no visitor is waiting ouside OR 
    //the museum is open and 2 tour guides are inside the museum
    return (sm->activeGuides == 2 && sm->open) || (sm->waitingVisitors == 0 && !sm->open);
}

void openMuseum() {
    cs1550_acquire(&sm->l);           //acquire lock 
    int i, fixed = 0;                 //initalize integers to 0
    sm->activeGuides++;               //increment active guides
    gettimeofday(&sm->end, NULL);     //get time
    //calculate the time since the start of the program
    long elapsed = ((sm->end.tv_sec * 1000000 + sm->end.tv_usec ) - (sm->start.tv_sec * 1000000+ sm->start.tv_usec));
    fprintf(stderr, "Tour guide %d opens the museum at time %ld \n", getpid(), elapsed / 1000000);    //print the time since start of program
    sm->open = true;                  //set the museum to be open
    fixed = sm->waitingVisitors;      //set the number of waiting visitors equal to the integer fixed  
    for (i = 0; i < fixed; i++) {     //wake up visitors waiting outside
        cs1550_signal(&sm->visitorGo);                                                     
    }
    cs1550_release(&sm->l);           //release lock
}

void tourguideLeaves() {
    cs1550_acquire(&sm->l);                //acquire lock
    int i, fixed = 0;                      //initalize integers to 0
    while (insideGuidesShouldWait())       //wait until it can leave
        cs1550_wait(&sm->insideGuideGo);                                                        
    gettimeofday(&sm->end, NULL);          //get time
    //calculate the time since the start of the program
    long elapsed = ((sm->end.tv_sec * 1000000 + sm->end.tv_usec ) - (sm->start.tv_sec * 1000000+ sm->start.tv_usec));
    fprintf(stderr, "Tour guide %d leaves the museum at time %ld \n", getpid(), elapsed / 1000000);    //print the time since start of program
    sm->activeGuides--;                    //decrement number of active guides
    //if there are no more guides inside, set open to false and reset visitors toured to 0
    if (sm->activeGuides == 0) {
        sm->open = false;
        sm->visitorsToured = 0;
        fprintf(stderr, "The museum is now empty\n");
    }                                            
    fixed = sm->waitingOutsideGuides;     //set the number of waiting outside guides equal to the integer fixed 
    for (i = 0; i < fixed; i++)           //wake up the outside guides waiting to get in
        cs1550_signal(&sm->outsideGuideGo);
    cs1550_release(&sm->l);              //release lock

}

static bool insideGuidesShouldWait() {
    //tour guides wait for everybody inside the museum to leave
    return (sm->activeVisitors > 0);
}

