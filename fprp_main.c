#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>

#define MAX_NEIGHBOR 10
#define MY_PORT 7000
#define MY_PORT2 8000
#define MAX_BUF 200
#define MAX_SLOT 5

#define RR_PAK 1
#define CR_PAK 2
#define RC_PAK 3
#define RA_PAK 4

int reserved_slot_info[MAX_SLOT];
void *client_thread(void *);
void *general_thread();
void *recv_function(void *);
void send_rr();
char* packet_handler(char *,int);
char* conditional_send_cr(char*);
char* send_cr(char *);
void send_rc();
char* send_ra(char *,char *);
void print_neighbor(void);
void ra_handler(char *,char *);
int slot;

struct neighbor_s
{
	char *name;
	char *time_slot;
	struct neighbor_s *next;
	struct neighbor_s *its_neighbor;
};
struct neighbor_s *neighbor_t;


void *server_thread()
{
	int sockfd,clientfd,addrlen,recv_data_len;
	struct sockaddr_in serverAddr,clientAddr;
	char *buf,*new_buf;
	if((sockfd = socket(AF_INET, SOCK_STREAM,0)) < 0)
	{
		perror("Error opening port !");
		exit(errno);
	}
	
	bzero (&serverAddr,sizeof(serverAddr));
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(MY_PORT);

	if(bind(sockfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr)) < 0)
	{
		perror("Error in Bind !");
		exit(errno);
	}

	if(listen(sockfd,10) != 0)
	{
		perror("Error in Listen !");
		exit(errno);
	}
	
	addrlen = sizeof(clientAddr);

	while(clientfd = accept(sockfd,(struct sockaddr*)&clientAddr,&addrlen))
	{
		buf = calloc(1,1);
	        if((recv_data_len = recv(clientfd,(void*)buf,255,0))<0)
		{
			perror("Error in receive !");
			exit(errno);
		}

		printf("MSG1: %s\n",buf);
		new_buf = calloc(1,1);
		new_buf = packet_handler(buf,recv_data_len);
		int n = send(clientfd,new_buf,strlen(new_buf),0);
		printf("n=%d\n",n);
		printf("MSG: %s\n",buf);
		
		close(clientfd);
	}
}
/*
void *recv_function(void *clientfd1)
{
	char buf[MAX_BUF];
	int sock = *(int*)clientfd1;
	printf("I am here recv_function\n");
	if((recv(sock,buf,255,0)))
	{
		printf("MSG: %s",buf);
	}			
}
*/
char* packet_handler(char *buf, int recv_data_len)
{
	char *requested_slot,request_type;
	int data_length,i;
	char *pname;

	pname = malloc(1);
	requested_slot = malloc(1);
	data_length = strlen(buf);
	printf("DATA_LEN %d RECV_DATA %d\n",data_length,recv_data_len);
	
	if(data_length != recv_data_len)
	{
		perror("Data length mismatch !");
		exit(errno);
	}

	for(i=0;i<data_length;i++)
	{
		if (i<data_length-2)
			pname[i] = buf[i];
		if (i == data_length-2)
			request_type = buf[i];
		if (i == data_length-1)
			*requested_slot = buf[i];
	}
	printf("NAME: %s REQUEST TYPE: %c REQUESTED SLOT: %s\n",pname,request_type,requested_slot);

	switch(request_type)
	{
		case '1': return (conditional_send_cr(requested_slot));	
			break;
		case '2': printf("Collision ! Try to book different slot...\n");
			break;
		case '3': return (send_ra(pname,requested_slot));
			break;
		case '4':
			if (*requested_slot == '9')
			{
			printf("RC discarded ! Start from begining...\n");
			break;
			}
			printf("Woohoo ! Received RA\n");
			ra_handler(pname,requested_slot);
			break;
	}
}

void ra_handler(char *pname,char *requested_slot)
{
	struct neighbor_s *new_neighbor;
	new_neighbor = (struct neighbor_s*)malloc(sizeof(struct neighbor_s));
       
	printf("I am here ra_handler\n");
	printf("PID: %d SLOT: %s\n",getpid(),requested_slot);

	new_neighbor->name = pname;
	new_neighbor->time_slot = requested_slot;
	new_neighbor->next = neighbor_t;
	new_neighbor->its_neighbor = NULL;
	neighbor_t = new_neighbor;

	printf("<client>New Neighbor added ! NAME: %s GIVEN SLOT: %s\n",neighbor_t->name,neighbor_t->time_slot);

}

char* send_ra(char *pname,char *requested_slot)
{	
        char *buf,*pak_type;
	int s;
	pthread_t client;
        buf = calloc(1,1);
        pak_type = calloc(1,1);
        printf("I am here send_ra\n");

	s = atoi((const char *)requested_slot);
        sprintf(buf,"%d",getpid());
        sprintf(pak_type,"%d",RA_PAK);
	
	if(reserved_slot_info[s]==0)
	{
		reserved_slot_info[s] = 1;
	
		struct neighbor_s *new_neighbor;
		new_neighbor = (struct neighbor_s*)malloc(sizeof(struct neighbor_s));
        
		printf("PID: %d SLOT: %s\n",getpid(),requested_slot);

		new_neighbor->name = pname;
		new_neighbor->time_slot = requested_slot;
		new_neighbor->next = neighbor_t;
		new_neighbor->its_neighbor = NULL;
		neighbor_t = new_neighbor;

		printf("New Neighbor added ! NAME: %s GIVEN SLOT: %s\n",neighbor_t->name,neighbor_t->time_slot);
	}
	else
		*requested_slot = '9';

        strcat(buf,pak_type);
        strcat(buf,requested_slot);

	return buf;
}

char* conditional_send_cr(char *requested_slot)
{
	int i,flag=0,s;
	char *buf;
	buf = malloc(1);
	s = atoi((const char*)requested_slot);
	printf("I am here conditional_send_cr REQUESTED_SLOT: %s\n",requested_slot);
	for(i=0;i<5;i++)
	{
		if(reserved_slot_info[s]==1)
		{
			flag = 1;
			break;
		}
	}
	
	if(flag)
	{
		printf("Collision detected !\n");
		buf = send_cr(requested_slot);
	}
	else
		printf("Hurrah ! No collision !\n");
	
	return buf;
}

char* send_cr(char *requested_slot)
{
        char *buf,*pak_type;
        buf = malloc(1);
        pak_type = malloc(1);
        printf("I am here send_cr\n");
        printf("PID: %d SLOT: %s\n",getpid(),requested_slot);
        sprintf(buf,"%d",getpid());
        sprintf(pak_type,"%d",CR_PAK);
        strcat(buf,pak_type);
        strcat(buf,requested_slot);
	printf("CR PAK: %s\n",buf);
	return buf;
}
void *client_thread(void *data)
{
	int sockfd,n;
	struct sockaddr_in server_addr;
	char *new_data;
	new_data = malloc(1);
	//char buf[MAX_BUF];
	printf ("I am in client_thread !\n");

	if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
	{
		perror("Failed creating socket !");
		exit(errno);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("142.133.168.33");
	server_addr.sin_port = htons(MY_PORT);

	if((connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)))<0)
	{
		perror("Failed to connect !");
		exit(errno);
	}

	//printf("INPUT: ");
	//bzero(buf,MAX_BUF);
	//fgets(buf,MAX_BUF,stdin);
	if((send(sockfd,data,strlen(data),0))<0)
		perror("Failed to send !");
	if((n = (recv(sockfd,new_data,255,0)))<0)
                perror("Failed to receive !");
	printf("REPLY: %s\n",new_data);
	close(sockfd);
	
	if(!(strlen(new_data)))
	{
		printf("No Collision ! Path is clear to send RC\n");
	}
	else
	{
		packet_handler(new_data,n);
	}
}

void send_rc()
{
        char *buf,*pak_type,*requested_slot;
	pthread_t client;
        buf = malloc(1);
        pak_type = malloc(1);
	requested_slot = malloc(1);
        printf("I am here send_rc\n");
        printf("PID: %d SLOT: %d\n",getpid(),slot);
        sprintf(buf,"%d",getpid());
        sprintf(pak_type,"%d",RC_PAK);
	sprintf(requested_slot,"%d",slot);
        strcat(buf,pak_type);
        strcat(buf,requested_slot);
	pthread_create(&client,NULL,client_thread,(void*)buf);
	pthread_join(client,NULL);
	
}

void *general_thread()
{
	int choice;
	while(1)
	{
		printf("Press 1 for send RR\nPress 2 for send RC\nPress 3 for print neighbor list\nCHOICE: ");
		scanf("%d",&choice);
		switch(choice)
		{
			case 1: send_rr();
				break;
			case 2: send_rc();
				break;
			case 3: print_neighbor();
				break;
		}
	}
}

void print_neighbor()
{
	struct neighbor_s *temp,*temp2;
	temp = (struct neighbor_s *)malloc(sizeof(struct neighbor_s));
	temp2 = (struct neighbor_s *)malloc(sizeof(struct neighbor_s));
	temp = neighbor_t;
	printf("NEIGHBOR LIST:\n");
	while(temp!=NULL)
	{
		printf("%s(slot:%s)",temp->name,temp->time_slot);
		temp2 = temp->its_neighbor;
		while(temp2!=NULL)
		{
			printf("-->%s(slot:%s)",temp2->name,temp2->time_slot);
			temp2 = temp2->next;
		}
		printf("\n");
		temp = temp->next;
	}
}

void send_rr()
{
	pthread_t client;
	char *buf,*pak_type,*slotc;
	slot = rand()%5;
	buf = malloc(1);
	pak_type = malloc(1);
	slotc = malloc(1);
	printf("I am here send_rr\n");
	printf("PID: %d SLOT: %d\n",getpid(),slot);
	sprintf(buf,"%d",getpid());
	sprintf(pak_type,"%d",RR_PAK);
	sprintf(slotc,"%d",slot);
	strcat(buf,pak_type);
	strcat(buf,slotc);
	pthread_create(&client,NULL,client_thread,(void*)buf);
	pthread_join(client,NULL);
}

int main()
{
pthread_t server,general;
pthread_create(&general,NULL,general_thread,NULL);
pthread_create(&server,NULL,server_thread,NULL);
pthread_exit(NULL);
return 0;
}

