#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include "fs.h"


#define CHILDNUM 1
#define INDEXNUM 16
#define FRAMENUM 32
#define OPEN 0
#define READ 1
#define ENTRYNUM 2

FILE* fptr;
struct partition tf;//total file
int root_node;
struct inode* root_ptr;
int user_node;
char buffer[1024];
int block_num=0;
int block_store[0x6];
int user_db_size=0;

int i = 0;
int total_count = 0;
pid_t pid[CHILDNUM];
int pid_index;
int fo_num = 0 ;
int user_fo[ENTRYNUM];

struct msgbuf{
	long int  mtype;
	int pid_index;
	int msg_mode;
	unsigned int  virt_mem[ENTRYNUM];
	char file_name[ENTRYNUM][32];
	int foqueue[ENTRYNUM];
};

typedef struct{
	int valid;
	int pfn;
	int read_only;
}TABLE;

typedef struct{
	char* data;
}PHY_TABLE;

TABLE table[CHILDNUM][INDEXNUM];
PHY_TABLE phy_mem [FRAMENUM];
//int fpl = 0;
int fpl[32];
int fpl_rear, fpl_front =0;

unsigned int pageIndex[ENTRYNUM];
unsigned int virt_mem[ENTRYNUM];
unsigned int offset[ENTRYNUM];
int foQueue[10];

int msgq;
int ret;
int key = 0x12346;
struct msgbuf msg;


void init_partition()
{
	/* ======================read superblock=========================*/
	fread(&tf.s,sizeof(struct super_block),1,fptr);
	printf("super block first_inode : %d\n",tf.s.first_inode);
	root_node = tf.s.first_inode;
	root_ptr = &tf.inode_table[root_node];

	/*======================read inode array =========================*/
	for(int l = 0; l < 224; l ++)
		fread(&tf.inode_table[l],sizeof(struct inode),1,fptr);

	/*=====================read data blocks=============================*/
	for(int l = 0; l < 4088; l ++)
		fread(&tf.data_blocks[l],sizeof(struct blocks),1,fptr);
	printf("Initialization OK \n");
	return;
}

void first_inode() //size of data block, number of data blocks
{
	/*================Access to root node and find data blocks================*/
	printf("Access to first inode (Root node)\n");
	printf("Size of datablocks : %dbytes \n",root_ptr -> size);
	int db_size =root_ptr -> size;
	while(db_size > 0)
	{
		block_store[block_num]=root_ptr->blocks[block_num];
		printf("data block : %d\n", block_store[block_num]);
		block_num ++;
		db_size = db_size - 1000;
	}

}

void root_file() //print root files
{
	/* ============================Read every file in root ================*/
	printf("\n=======================Root dir files==========================\n");
	for(int l = 0; l< block_num; l ++)
	{
		struct blocks* bp = &tf.data_blocks[block_store[l]];
		//printf("\nRead block : %d\n",block_store[l]);
		struct dentry* den;
		char* sp = bp->d;
		int read_size = 0 ;
		while(*sp && read_size < 1024)
		{ //only read 1 data block
			den = (struct dentry *) sp;
			sp += den->dir_length;
			read_size += den->dir_length;
			printf("%s ",*den->n_pad);
		}
	}
	printf("\n=======================Initialization==========================\n"); 
}

int find_user_file(char* file_name)
{
	printf("FInd file name in find_user file function %s\n",file_name);
	for(int l = 0; l< block_num; l ++)
	{
		struct blocks* bp = &tf.data_blocks[block_store[l]];
		struct dentry* den;
		char* sp = bp->d;
		int read_size = 0 ;
		while(*sp && read_size < 1024)
		{ //only read 1 data block
			den = (struct dentry *) sp;
			sp += den->dir_length;
			read_size += den->dir_length;
			printf("file name : %s\n",*den->n_pad);

			if(strcmp(*den->n_pad, file_name) == 0) //to find if file_1 exist
			{
				printf("found %s, enter inode %d\n", *den->n_pad, den->inode);
				user_node = den->inode;
				printf("datablocks size : %dbytes\n", tf.inode_table[user_node].size);
				return user_node;
			}

		}
	}

}


void open_file(char(* file_name)[32],int* fo_num){

	memset(&msg,0,sizeof(msg));
	msg.mtype = IPC_NOWAIT;
	msg.pid_index = i;
	msg.msg_mode = OPEN;
	char* sp ;
	for(int j = 0 ; j < ENTRYNUM ; j++){
		sp = file_name[j];
		int l = 0;
		while(*sp){
			msg.file_name[j][l] = file_name[j][l];
			l++;
			sp++;
		}
		msg.foqueue[j] = fo_num[j];
	}
	ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);
	if(ret == -1)
		perror("msgsnd error");

}

void read_file(int* user_fo,unsigned int* vm){

	memset(&msg,0,sizeof(msg));
	msg.mtype = IPC_NOWAIT;
	msg.pid_index = i;
	msg.msg_mode = READ;
	for(int l = 0; l < ENTRYNUM ; l++){
		
		msg.foqueue[l] = user_fo[l];
		msg.virt_mem[l] = vm[l];
	}
	ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);
	if(ret == -1)
		perror("msgsnd error");
}

void child_function(){
	
	int file_num = 15;
	char file_name[ENTRYNUM][32]={0};
	unsigned int addr;
	unsigned int vm[ENTRYNUM];

	for (int l = 0 ; l < 2; l ++){
		addr = (rand() %0x09)<<12;
		addr |= (rand()%0xfff);
		vm[l] = addr;
		printf("VM : %04x\n",vm[l]);
		sprintf(file_name[l],"file_%d",file_num++);
		printf("Send FIle name : %s\n",file_name[l]);
		user_fo[l] = fo_num++;
	}

	open_file(file_name,user_fo);
	read_file(user_fo,vm);
	exit(0);
}

void initialize_table()
{
	for( int a = 0; a < CHILDNUM ; a++){
		for(int j =0; j< INDEXNUM ; j++)
		{
			table[a][j].valid =0;
			table[a][j].pfn = 0;
			table[a][j].read_only = 0;
		}
	}
	printf("Initialize table\n");
}

void initialize_phymem(){

	for(int l =0 ; l <FRAMENUM ; l++)
		phy_mem[l].data = NULL;
	printf("Initialize phymem\n");
}
char* find_user_data(int user_node)
{
	struct inode* user_ptr;
	user_ptr = &tf.inode_table[user_node];
	user_db_size = user_ptr -> size;
	block_num =0;

	while(user_db_size > 0)
	{
		block_store[block_num]=user_ptr->blocks[block_num];
		printf("data block : %d\n", block_store[block_num]);
		block_num ++;
		user_db_size = user_db_size - 1000;
	}

	for(int l = 0; l< block_num; l++)
	{
		struct blocks* bp = &tf.data_blocks[block_store[l]];
		printf("Read block : %d\n",block_store[l]);
		for(int a=0; a< 1024; a++)
		{
			buffer[a] = bp->d[a];
		}
		printf("data : %s\n", buffer);
	}

	return buffer;

}

int main(int argc,char* argv[])
{


	for(int h=0; h<FRAMENUM; h++)
	{
		fpl[h] = h;
		fpl_rear++;
	}

	msgq = msgget( key, IPC_CREAT | 0666);
	fptr = fopen("disk.img","r");
	initialize_table();
	initialize_phymem();
	/*===================initialization =================*/

	init_partition();
	first_inode();
	root_file();
	/*==================================================*/
	pid[i]= fork();
	if(pid[0] == -1){
		perror("fork error");
		return 0;
	}
	else if( pid[i] == 0){ // child process
		printf("Fork ok \n");
		child_function();
		return 0;
	}
	else{ // parent process

		while(1)
		{
			ret = msgrcv(msgq,&msg,sizeof(msg),IPC_NOWAIT,IPC_NOWAIT); //to receive message
			if(ret != -1)
			{
				printf("get message\n");
				pid_index = msg.pid_index;
				printf("pid index: %d\n", pid_index);
				int mode = msg.msg_mode;
				if( mode == OPEN){
					char file_name[ENTRYNUM][32];
					int foqueue[ENTRYNUM]; 
					for(int j =0 ; j < ENTRYNUM ; j++){
						foqueue[j]= msg.foqueue[j];
						char* sp = msg.file_name[j];
						int l = 0;
						while(*sp){
							file_name[j][l] = msg.file_name[j][l];
							l++;
							sp++;
						}
						printf("file name :%s\n",file_name[j]);
						foQueue[foqueue[j]]=find_user_file(file_name[j]);
						printf("inode:%d\n",foQueue[foqueue[j]]);
					}
				}
				else if (mode == READ)
				{
					for(int j = 0 ; j < ENTRYNUM ; j ++){
						virt_mem[j] = msg.virt_mem[j];
						offset[j] = virt_mem[j] & 0xfff;
						pageIndex[j] = (virt_mem[j] & 0xf000) >> 12 ;
						printf("Read mode\n");
						int foqueue = msg.foqueue[j];
						char* dp;
						if(table[pid_index][pageIndex[j]].valid == 0) //if its invalid
						{
							if(fpl_front != fpl_rear)
							{
								table[pid_index][pageIndex[j]].pfn=fpl[fpl_front%FRAMENUM];
								printf("VA %d -> PA %d\n", pageIndex[j],  table[pid_index][pageIndex[j]].pfn);
								table[pid_index][pageIndex[j]].valid = 1;
								fpl_front++;
								dp= find_user_data(foQueue[foqueue]);
								phy_mem[table[pid_index][pageIndex[j]].pfn].data = dp;
							}
							else
							{
								printf("full");
								exit(0);
							}

						}
						else if(table[msg.pid_index][pageIndex[j]].valid == 1)
						{
							printf("In the disk\n");
						}
					}
					msgctl(msgq, IPC_RMID, NULL);
					 return 0;
						
				}
				
			}
			memset(&msg, 0, sizeof(msg));
			//msgctl(msgq, IPC_RMID, NULL);
		}
		return 0;
	}
}
