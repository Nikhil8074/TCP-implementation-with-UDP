#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/time.h>
#include<sys/select.h>

#define PORT 8080
#define size_of_chunk 10
int end=0;
typedef struct packet
{
    int seq_num;
    int total_chunks;
    char data[size_of_chunk];
}packet;

typedef struct ack
{
    int seq_num;
}ack;

void send_data_chunks(int sockfd,struct sockaddr_in *client,char *send_data,int data_len)
{
    socklen_t addr_len=sizeof(*client);
    int total_no_of_chunks=(data_len+size_of_chunk-1)/size_of_chunk;
    packet data_packet;
    ack ack_packet;
    int acked_chunks[total_no_of_chunks];
    memset(acked_chunks,0,sizeof(acked_chunks));
    int no_of_acks=0;
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec=0;
    timeout.tv_usec=100000;
    for(int i=0;i<total_no_of_chunks;i++)
    {
        data_packet.seq_num=i;
        data_packet.total_chunks=total_no_of_chunks;
        memcpy(data_packet.data,send_data+(i*size_of_chunk),size_of_chunk);

        sendto(sockfd,&data_packet,sizeof(data_packet),0,(struct sockaddr *)client,addr_len);
    }

    while(no_of_acks<total_no_of_chunks)
    {
        FD_ZERO(&read_fds);
        FD_SET(sockfd,&read_fds);

        if(select(sockfd+1,&read_fds,NULL,NULL,&timeout)>0)
        {
            if(recvfrom(sockfd,&ack_packet,sizeof(ack_packet),0,(struct sockaddr *)client,&addr_len)>0 && ack_packet.seq_num<total_no_of_chunks)
            {
                if(acked_chunks[ack_packet.seq_num]==0)
                {
                    acked_chunks[ack_packet.seq_num]=1;
                    printf("ACK recieved for %d chunk\n",ack_packet.seq_num+1);
                    no_of_acks++;
                }
            }
        }
        else
        {
            for(int i=0;i<total_no_of_chunks;i++)
            {
                if(acked_chunks[i]==0)
                {
                    data_packet.seq_num=i;
                    data_packet.total_chunks=total_no_of_chunks;
                    memcpy(data_packet.data,send_data+(i*size_of_chunk),size_of_chunk);
                    printf("Retransmitting....\n");
                    sendto(sockfd,&data_packet,sizeof(data_packet),0,(struct sockaddr *)client,addr_len);
                }
            }
        }
    }
    printf("All chunks Sent\n");
}

void receive_data_chunks(int sockfd,struct sockaddr_in *client)
{
    packet data_packet;
    ack ack_packet;
    char received_data[10000]={0};
    int recv_chunks=0;
    int total_chunks=-1;
    socklen_t addr_len=sizeof(*client);
    while(1)
    {
        if(recvfrom(sockfd,&data_packet,sizeof(data_packet),0,(struct sockaddr *)client,&addr_len)>0)
        {
            total_chunks=(total_chunks==-1) ? data_packet.total_chunks : total_chunks;
            if(total_chunks==0)
                break;
            if(data_packet.seq_num<total_chunks)
            {
                printf("Received chunk %d \n",data_packet.seq_num+1);

                memcpy(received_data+(data_packet.seq_num*size_of_chunk),data_packet.data,size_of_chunk);
                recv_chunks++;

                ack_packet.seq_num=data_packet.seq_num;
                sendto(sockfd,&ack_packet,sizeof(ack_packet),0,(struct sockaddr *)client,addr_len);
            }

            if(recv_chunks==total_chunks)
            {
                printf("All chunks Received\n");
                printf("Assembeld Message : %s",received_data);
                if(strncmp(received_data,"exit",3)==0)
                    end=1;
                break;
            }
        }
        
    }
}

int main()
{
    int sockfd=socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd<0)
    {
        printf("Error in intializing the socket\n");
        exit(1);
    }
    struct sockaddr_in server,client;
    socklen_t addr_len=sizeof(server);
    
    memset(&server,0,addr_len);
    server.sin_family=AF_INET;
    server.sin_port=htons(PORT);
    server.sin_addr.s_addr=inet_addr("127.0.0.1");

    int all_flags=fcntl(sockfd,F_GETFL,0) | O_NONBLOCK;
    fcntl(sockfd,F_SETFL,all_flags);

    if(bind(sockfd,(struct sockaddr *)&server,addr_len)<0)
    {
        printf("Error while binding\n");
        close(sockfd);
        exit(1);
    }
    printf("SERVER STARTED\n");
    while(1)
    {
        printf("Waiting for client to send Message\n");
        receive_data_chunks(sockfd,&client);
        if(end==1)
            break;
        printf("Enter The Message: ");
        char input[100000];
        fgets(input,sizeof(input),stdin);

        send_data_chunks(sockfd,&client,input,strlen(input));
        if(strncmp(input,"exit",4)==0)
            break;
    }
    close(sockfd);
    return 0;
}