#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#define BLOCK_SIZE 1024

//Struct holding information each thread needs
typedef struct threadWorker{
	char* ipaddress;
	char* port;
	int sockfd;
	int base_index;
	int current_index;
	pthread_t thread;
}threadWorker;

void writeToFile(char* buff, int block_index,int bytes_recieved);
int getChunk(char* buff, int sockfd, int block_index);
int getSocket(char* ipaddress, char* port);
void *runThread(threadWorker* worker);

FILE* outfile;			//Output file
pthread_mutex_t filelock;

int main(int argc, char**argv){
	outfile = fopen("output.txt", "w");		//Ouput file is always the same
	if(outfile==NULL){
		printf("Error creating output file\n");
		return -1;
	}if(argc<3 || (argc%2)!=1){			//Must have a correct number of arguements
		printf("Format is ./client <ipaddr1> <port1> <ipaddr2> <port2>...\n");
		return -1;
	} 
	pthread_mutex_init(&filelock,NULL);
	struct threadWorker tw[(argc-1)/2];		//array of threadWorkers each representing a thread
	int i;
	int index=0;		//Index of threadWorker Array
	int base_counter = 1;	//Indec to start each struct's base_index on
	for(i=1;i<argc;i=i+2){
		tw[index].ipaddress=argv[i];
		tw[index].port=argv[i+1];
		tw[index].base_index = base_counter;
		tw[index].current_index = tw[index].base_index;
		pthread_create(&(tw[index].thread), NULL,(void *)runThread,&tw[index]); //Start running the thread on the runThread method
		index++;	
		base_counter++;
	}
	for(i=0;i<(argc-1)/2;i++)			//Checking if all the threads are done
		pthread_join(tw[i].thread,NULL);
	fclose(outfile);
	return 1;
} 

//This method is the heart of the program and where the 
//threads spend most of their time. Is a loop od getting a 
//chunk and writing it to file
void *runThread(threadWorker* worker){	
	if(worker->base_index==1) worker->current_index=0; //Special case to download the first block
	int bytes_recieved = 1024;			   //Conditional of the loop	
	while(bytes_recieved==1024){			   //Keep looping if the last block was full	
		char buffer[BLOCK_SIZE]={0};						//Buffer to store the blocks in	
		worker->sockfd = getSocket(worker->ipaddress,worker->port);		//Get a socket
		bytes_recieved=getChunk(buffer,worker->sockfd,worker->current_index);   //Go get a chunk
		pthread_mutex_lock(&filelock);						//Mutex protecting writing to the file
		writeToFile(buffer,worker->current_index,bytes_recieved);		//Write to the file
		pthread_mutex_unlock(&filelock);
		worker->current_index+=worker->base_index;				//Update index of block wanted
		close(worker->sockfd);	
	}
	pthread_exit(NULL);					//Finished downloading it's blocks and returns
}

//Requests a block from the server and puts the block recieved
//in the arguement buffer
int getChunk(char* buff, int sockfd,int block_index){
	char temp[10]={0};
	snprintf(temp,10,"%d", block_index);	//block_index is converted to a string before being sent
	send(sockfd,temp,strnlen(temp,10),0);
	return recv(sockfd, buff,BLOCK_SIZE,0);	
}

//Writes the the buffer to the output file. Blcok index specifies
//where in the file to write. bytes_recieved is how many bytes are
//in the buffer and need to be written out. This method is mutex locked
//from the runThread method.	
void writeToFile(char*buff,int block_index,int bytes_recieved){
	fseek(outfile,BLOCK_SIZE*block_index,SEEK_SET);
	fwrite(buff,bytes_recieved,1,outfile);
}

//This method creates a socket from the given ipaddress and port and returns
//the socket number
int getSocket(char* ipaddress, char* port){
	int sockfd;
	struct sockaddr_in serv_addr;
	if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
		printf("Error creating socket\n");
		return -1;
	}
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(port));
	if(inet_pton(AF_INET, ipaddress, &serv_addr.sin_addr)<=0){
		printf("Invalid IP Address\n");
		return -1; 
	}
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0){
		printf("Connection failure\n");
		return -1;
	}		
	return sockfd;
}
