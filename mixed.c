/*
 * File: mixed.c
 * Author: Anne Gatchell
 * Code based off of code by Andy Saylor
 * Project: CSCI 3753 Programming Assignment 3
 * Modify Date: 2013/03/31
 * Description: A program with a mixture of i/o and cpu bound portions
                program to statistically calculate pi and write
                to an output file. A user determined number of process
                all do this.

 run with ./mixed <#Bytes to Write to Output File> <Block Size> <Num Processes> 
                            <Scheduling Policy> <Number of Iterations>
 */

/* Include Flags */
#define _GNU_SOURCE

/* System Includes */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sched.h>
#include <math.h>

/* Local Defines */
#define MAXFILENAMELENGTH 80
#define DEFAULT_INPUTFILENAME "rwinput"
#define DEFAULT_OUTPUTFILENAMEBASE "rwoutput"
#define DEFAULT_BLOCKSIZE 1024
#define DEFAULT_TRANSFERSIZE 1024*100
#define RADIUS (RAND_MAX / 2)
#define DEFAULT_ITERATIONS 1000000

// #define MAX_ORDER_OF_PROCESSES 3
#define MAX_NUM_PROCESSES 999
#define DEFAULT_NUM_PROCESSES 1

inline double dist(double x0, double y0, double x1, double y1){
    return sqrt(pow((x1-x0),2) + pow((y1-y0),2));
}

inline double zeroDist(double x, double y){
    return dist(0, 0, x, y);
}


int main(int argc, char* argv[]){

    int rv;
    int outputFD;
    char outputFilename[MAXFILENAMELENGTH];
    char outputFilenameBase[MAXFILENAMELENGTH];

    ssize_t transfersize = 0;
    ssize_t blocksize = 0; 
    char* transferBuffer = NULL;
    ssize_t buffersize;

    ssize_t bytesWritten = 0;
    ssize_t totalBytesWritten = 0;
    int totalWrites = 0;
    int num_processes;

    int policy;
    struct sched_param param;

    double x, y;
    double inCircle = 0.0;
    double inSquare = 0.0;
    long iterations;
    
    //./mixed <transfer size><block size><num processes><policy><num iterantions><outputfilename>

    /* Process program arguments to select run-time parameters */
    /* Set supplied transfer size or default if not supplied */
    if(argc < 2){
		transfersize = DEFAULT_TRANSFERSIZE;
    }
    else{
		transfersize = atol(argv[1]);
		if(transfersize < 1){
		    fprintf(stderr, "Bad transfersize value\n");
		    exit(EXIT_FAILURE);
		}
    }
    /* Set supplied block size or default if not supplied */
    if(argc < 3){
		blocksize = DEFAULT_BLOCKSIZE;
    }
    else{
		blocksize = atol(argv[2]);
		if(blocksize < 1){
		    fprintf(stderr, "Bad blocksize value\n");
		    exit(EXIT_FAILURE);
		}
    }
    /*Set the supplied number of processes*/
    if(argc < 4){
    	if(DEFAULT_NUM_PROCESSES < MAX_NUM_PROCESSES){
    		num_processes = DEFAULT_NUM_PROCESSES;
    	}else{
    		fprintf(stderr, "Invalid default number processes\n");
    		exit(EXIT_FAILURE);
    	}
    }else{
    	num_processes = atoi(argv[3]);
    	if(num_processes > MAX_NUM_PROCESSES){
    		fprintf(stderr, "Invalid number of processes %d\n", MAX_NUM_PROCESSES);
    		exit(EXIT_FAILURE);
    	}else if(num_processes < 1){
    		fprintf(stderr, "Invalid number of processes\n");
    		exit(EXIT_FAILURE);
    	}
    }

    /* Set policy if supplied */
    if(argc < 5){
    	fprintf(stderr, "No scheduling policy provided\n");
    	//exit(EXIT_FAILURE);
    }else{
    	if(!strcmp(argv[4], "SCHED_OTHER")){
    	    policy = SCHED_OTHER;
    	}
    	else if(!strcmp(argv[4], "SCHED_FIFO")){
    	    policy = SCHED_FIFO;
    	}
    	else if(!strcmp(argv[4], "SCHED_RR")){
    	    policy = SCHED_RR;
    	}
    	else{
    	    fprintf(stderr, "Unhandeled scheduling policy\n");
    	    exit(EXIT_FAILURE);
    	}
    }

    /* Set number of iterations or default if not supplied */
    if(argc < 6){
    	iterations = DEFAULT_ITERATIONS;
    }else{
    	iterations = atol(argv[5]);
    	if(iterations < 1){
    	    fprintf(stderr, "Bad iterations value\n");
    	    exit(EXIT_FAILURE);
    	}
	}
    
    /* Set supplied output filename base or default if not supplied */
    if(argc < 7){
		if(strnlen(DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH) >= MAXFILENAMELENGTH){
		    fprintf(stderr, "Default output filename base too long\n");
		    exit(EXIT_FAILURE);
		}
		strncpy(outputFilenameBase, DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH);
    }else{
		if(strnlen(argv[6], MAXFILENAMELENGTH) >= MAXFILENAMELENGTH){
		    fprintf(stderr, "Output filename base is too long\n");
		    exit(EXIT_FAILURE);
		}
		strncpy(outputFilenameBase, argv[6], MAXFILENAMELENGTH);
    }

    /* Confirm blocksize is multiple of and less than transfersize*/
    if(blocksize > transfersize){
		fprintf(stderr, "blocksize can not exceed transfersize\n");
		exit(EXIT_FAILURE);
    }
    if(transfersize % blocksize){
		fprintf(stderr, "blocksize must be multiple of transfersize\n");
		exit(EXIT_FAILURE);
    }

     /* Set process to max prioty for given scheduler */
    param.sched_priority = sched_get_priority_max(policy);
    
    /* Set new scheduler policy */
    fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);
    if(sched_setscheduler(0, policy, &param)){
    	perror("Error setting scheduler policy");
    	exit(EXIT_FAILURE);
    }
    fprintf(stdout, "New Scheduling Policy: %d\n", sched_getscheduler(0));

	/* Fork the correct number of child processes */
	pid_t wait_pid, child_pid;
	int status = 0;
	char* text = "OH HEYY THERE\n";

	int j;
	for(j = 0; j < num_processes; j++){
		if((child_pid = fork()) < 0){
			fprintf(stderr, "Fork failed\n");
			exit(EXIT_FAILURE);
		} else if(child_pid == 0){

			/* Allocate buffer space */
		    buffersize = blocksize;
		    if(!(transferBuffer = malloc(buffersize*sizeof(*transferBuffer)))){
				perror("Failed to allocate transfer buffer");
				exit(EXIT_FAILURE);
		   	}
		   	strcpy(transferBuffer, text);

		    /* Open Output File Descriptor in Write Only mode with standard permissions*/
		    rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s-%d",
				  outputFilenameBase, getpid());    
		    if(rv > MAXFILENAMELENGTH){
				fprintf(stderr, "Output filenmae length exceeds limit of %d characters.\n",
					MAXFILENAMELENGTH);
				exit(EXIT_FAILURE);
		    }else if(rv < 0){
				perror("Failed to generate output filename");
				exit(EXIT_FAILURE);
		    }
		    if((outputFD =
					open(outputFilename,
					     O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
					     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0){
				perror("Failed to open output file");
				exit(EXIT_FAILURE);
		    }

		    /* Print Status */
		    fprintf(stdout, "Writing to %s\n", outputFilename);

		    /* Do an iteration of pi calc, and then write some 
		    bogus stuff to file*/
		    int i;
		    for(i=0; i<iterations; i++){
				x = (random() % (RADIUS * 2)) - RADIUS;
                y = (random() % (RADIUS * 2)) - RADIUS;
                if(zeroDist(x,y) < RADIUS){
                    inCircle++;
                }
                inSquare++;

			/* If all bytes were read, write to output file*/
				bytesWritten = write(outputFD, transferBuffer, strlen(text));
			    if(bytesWritten < 0){
					perror("Error writing output file");
					exit(EXIT_FAILURE);
			    }
			    else{
					totalBytesWritten += bytesWritten;
					totalWrites++;
			    }
			
			}

		    /* Output some possibly helpfull info to make it seem like we were doing stuff */
		    fprintf(stdout, "Written: %zd bytes in %d writes\n",
			    totalBytesWritten, totalWrites);
		    fprintf(stdout, "Processed %zd bytes in blocks of %zd bytes\n",
			    transfersize, blocksize);
		    break;

		    /* Free Buffer */
		    free(transferBuffer);

		    /* Close Output File Descriptor */
		    if(close(outputFD)){
				perror("Failed to close output file");
				exit(EXIT_FAILURE);
		    }
		}

	}
	/*Wait for children to finish*/
	while ((wait_pid = wait(&status)) > 0){
		printf("Exit status of %d was %d \n", (int)wait_pid, status);
	}

    

    return EXIT_SUCCESS;
}
