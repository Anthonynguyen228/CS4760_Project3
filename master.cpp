/*
CS_4760
Anthony Nguyen
03/14/2022
*/

#include <unistd.h>
#include <fstream> 
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>

using namespace std;

void spawnChild(int count); //spawns child 
void spawn(int count); //helper function
void sigHandler(int SIG_CODE); 
void timerSignalHandler(int); 
void releaseMemory(); 

const int MAX_NUM_OF_PROCESSES_IN_SYSTEM = 20;
int currentNumOfProcessesInSystem = 0; 


int sharedIntKey = ftok("Makefile", 1);
int sharedIntSegmentID;
int *sharedInt;


int flagsKey = ftok("Makefile", 2); //
int flagsSegmentID;
int *flags;


int turnKey = ftok("Makefile", 3);
int turnSegmentID;
int *turn;



int slaveProcessGroupKey = ftok("Makefile", 4);
int slaveProcessGroupSegmentID;
pid_t * slaveProcessGroup;


//number of slaves to spawn
int slaveKey = ftok("Makefile", 5);
int slaveSegmentID;
int *slaveNum;

//output file name
int fileNameKey = ftok("Makefile", 6);
int fileNameSegmentID;
char *fileName;


//number of times slave should enter crit
int maxWritesKey = ftok("Makefile", 7);
int maxWritesSegmentID;
int *maxWrites;

int status = 0; 

int startTime; 
int durationBeforeTermination; 


int main(int argc, char** argv){

	
	signal(SIGINT, sigHandler);
	signal(SIGUSR2, timerSignalHandler);

	int numOfSlaves = 5; 
	int numOfSlaveExecutions = 3; 
	durationBeforeTermination = 20; 
	const char *fName = "output.out"; //output file

	extern char *optarg; 
	int c; 

	int n, y, z; 

	while((c = getopt(argc, argv, "hs:l:i:t:")) != -1){
		switch(c){
			case 'h': //help option
				cout << "This program accepts the following command-line arguments:" << endl;
				cout << "\t-h: Get help option." << endl;
				cout << "\t-s n: Maximum number of slave processes to spawn." << endl;
				cout << "\t-l filename: the output file for the log (default 'output.out')." << endl;
				cout << "\t-i y: Number of times each slave should execute critical section." << endl;
				cout << "\t-t ss: Time (seconds) at which master will terminate itself." << endl;

				exit(0);
			break;

			case 's': //# of slaves option
				n = atoi(optarg);
				if(n < 0){
					cerr << "Cannot spawn a negative number of slaves." << endl;
					exit(1);
				}
				else{
					numOfSlaves = n;
				}
			break;

			case 'l': //filename option
				fName = optarg;
			break;

			case 'i': //Number of critical section executions by slaves
				y = atoi(optarg);
				if(y < 0){
					cerr << "Negative arguments are not valid" << endl;
					exit(1);
				}
				else{
					numOfSlaveExecutions = y;
				}
			break;

			case 't': //time at which master will terminate
				z = atoi(optarg);

				if(z < 0){
					cerr << "Master cannot have a run duration of negative time." << endl;
					exit(1);
				}
				else{
					durationBeforeTermination = z;
				}
			break;

			default:
				cerr << "Default getopt statement" << endl; //FIXME: Use better message.
				exit(1);
			break;
		}

	}

	//Configure shared memory for sharedNum
	if((sharedIntSegmentID = shmget(sharedIntKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for shared int");
	 	exit(1);
	 }
	 else{
	 	sharedInt = (int *) shmat(sharedIntSegmentID, NULL, 0);
	 	(*sharedInt) = 0;
	 }

	 
	if((flagsSegmentID = shmget(flagsKey, numOfSlaves * sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for flags array");
	 	exit(1);
	 }
	 else{
	 	flags = (int *)shmat(flagsSegmentID, NULL, 0); //Should this be cast to something else
	 }

	if((turnSegmentID = shmget(turnKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for turn array");
	 	exit(1);
	 }
	 else{
	 	turn = (int *)shmat(sharedIntSegmentID, NULL, 0);
	 }

	
	
	if((slaveProcessGroupSegmentID = shmget(slaveProcessGroupKey, sizeof(pid_t), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for group PID");
	 	exit(1);
	 }
	 else{
	 	slaveProcessGroup = (pid_t *) shmat(slaveProcessGroupSegmentID, NULL, 0);
	 }


	//Congifure shared memory to hold num of slaves to spawn
	if((slaveSegmentID = shmget(slaveKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for slaveNum");
	 	exit(1);
	 }
	 else{
	 	slaveNum = (int *)shmat(slaveSegmentID, NULL, 0);
	 	*slaveNum = numOfSlaves;
	 }

	 
	 //Congifure shared memory to hold fileName
	 
	 
	if((fileNameSegmentID = shmget(fileNameKey, sizeof(char) * 26, IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for filename");
	 	exit(1);
	 }
	 else{
	 	fileName = (char *)shmat(fileNameSegmentID, NULL, 0);
	 	
	 	strcpy(fileName, fName);
	 }

	 //Congifure shared memory to hold num of times slaves should execute critical section
	if((maxWritesSegmentID = shmget(maxWritesKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for maxWrites");
	 	exit(1);
	 }
	 else{
	 	maxWrites = (int *)shmat(maxWritesSegmentID, NULL, 0);
	 	*maxWrites = numOfSlaveExecutions;
	 }

	 //start timer, then spawn slaves
	 startTime = time(0);

	 int count = 1;

	 while(count <= numOfSlaves){
	 	spawnChild(count);
	 	++count;
	 }

	 //wait for all child processes to finish or time to run out, then free up memory and close

	while((time(0) - startTime < durationBeforeTermination) && currentNumOfProcessesInSystem > 0){
	 	wait(NULL);
	 	--currentNumOfProcessesInSystem;
	 	cout << currentNumOfProcessesInSystem << " processes in system.\n";
	 }

	releaseMemory();

	return 0;
}

void spawnChild(int count){
	if(currentNumOfProcessesInSystem < MAX_NUM_OF_PROCESSES_IN_SYSTEM){
		spawn(count);
	}
	else{
		waitpid(-(*slaveProcessGroup), &status, 0);
		--currentNumOfProcessesInSystem;
		cout << currentNumOfProcessesInSystem << " processes in system.\n";
		spawn(count);
	}
}

void spawn(int count){
	++currentNumOfProcessesInSystem;
	if(fork() == 0){
		cout << currentNumOfProcessesInSystem << " processes in system.\n";
	 	if(count == 1){ 
	 		(*slaveProcessGroup) = getpid();
	 	}
	 	setpgid(0, (*slaveProcessGroup));
	 	execl("./slave", "slave", to_string(count).c_str(), (char *)NULL); 
	 	exit(0);
	 }
}

void sigHandler(int signal){
	killpg((*slaveProcessGroup), SIGTERM);
	
	for(int i = 0; i < currentNumOfProcessesInSystem; i++){
	 	wait(NULL);
	 }
	releaseMemory();
	// close a file
	cout << "Exiting master process" << endl;
	exit(0);
}

void timerSignalHandler(int signal){
	if(time(0) - startTime >= durationBeforeTermination){
	 	cout << "Master: Time's up!\n";
	 	killpg((*slaveProcessGroup), SIGUSR1);
		//CHECK TO SEE IF PROCESSES HAVE EXITED with waitpid and sigkill
		
		for(int i = 0; i < currentNumOfProcessesInSystem; i++){
		 	wait(NULL);
		 }
		releaseMemory();
		cout << "Exiting master process" << endl;
		exit(0);
	 }
}

void releaseMemory(){
	shmdt(sharedInt);
	shmctl(sharedIntSegmentID, IPC_RMID, NULL);

	shmdt(flags);
	shmctl(flagsSegmentID, IPC_RMID, NULL);

	shmdt(turn);
	shmctl(turnSegmentID, IPC_RMID, NULL);

	shmdt(slaveProcessGroup);
	shmctl(slaveProcessGroupSegmentID, IPC_RMID, NULL);

	shmdt(slaveNum);
	shmctl(slaveSegmentID, IPC_RMID, NULL);

	// delete file
	shmdt(fileName);
	shmctl(fileNameSegmentID, IPC_RMID, NULL);

	shmdt(maxWrites);
	shmctl(maxWritesSegmentID, IPC_RMID, NULL);

}