/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queue>

using namespace std;

#include "rdt_struct.h"
#include "rdt_sender.h"

#define TIMEOUT 0.3
#define MAX_WINDOW_SIZE 10
#define MAX_BUFFER_SIZE 15000

#define HEADER_SIZE 7
#define MAX_PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)

static queue<message *> msg_buffer;
static int msg_num;
static int cur_msg_byte;

static packet windows[MAX_WINDOW_SIZE];
static int next_seq;
static int next_packet_seq;
static int next_packet_to_send;
static int expected_ack;
static int packet_num;


static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}

static int inc_seq(){
    int origin = next_seq;
    next_seq = next_seq + 1;
    return origin;
}

static bool between(int a, int b, int c){
    // int a = ra % MAX_SEQ;
    // int c = rc % MAX_SEQ;
    // int b = rb % MAX_SEQ;
    if((b>=a && b<c)) return true;
    else return false;
}

static void from_buffer_to_windows() {
    // fprintf(stdout, "At %.2fs: sender from_buffer_to_windows ...\n", GetSimulationTime());
    while(packet_num < MAX_WINDOW_SIZE && ! msg_buffer.empty()){
        message msg =  *msg_buffer.front();
        packet pkt;
        short check;

        if(msg.size - cur_msg_byte > MAX_PAYLOAD_SIZE){
            int seq = inc_seq();
            memcpy(pkt.data + sizeof(short), &seq, sizeof(int));

            pkt.data[HEADER_SIZE - 1] = MAX_PAYLOAD_SIZE;

            memcpy(pkt.data + HEADER_SIZE, msg.data + cur_msg_byte, MAX_PAYLOAD_SIZE);

            check = checksum(&pkt);
            memcpy(pkt.data, &check, sizeof(short));

            memcpy(&(windows[next_packet_seq % MAX_WINDOW_SIZE]), &pkt, sizeof(packet));

            cur_msg_byte += MAX_PAYLOAD_SIZE;
            next_packet_seq++;
            packet_num++;
        }
        else
        {
            if(msg.size > cur_msg_byte){
                int seq = inc_seq();
                memcpy(pkt.data + sizeof(short), &seq, sizeof(int));

                pkt.data[HEADER_SIZE - 1] = msg.size - cur_msg_byte;

                memcpy(pkt.data + HEADER_SIZE, msg.data + cur_msg_byte, msg.size - cur_msg_byte);

                check = checksum(&pkt);
                memcpy(pkt.data, &check, sizeof(short));

                memcpy(&(windows[next_packet_seq % MAX_WINDOW_SIZE]), &pkt, sizeof(packet));

                msg_num--;
                cur_msg_byte = 0;
                next_packet_seq++;
                message* buf = msg_buffer.front();
                msg_buffer.pop();
                free(buf);
                
                packet_num++;
            }
        }

    }
    // fprintf(stdout, "At %.2fs: sender from_buffer_to_windows end...\n", GetSimulationTime());
}

static void send_packets() {
    // fprintf(stdout, "At %.2fs: sender send_packets ...\n", GetSimulationTime());
    packet pkt;
    while(between(next_packet_seq - MAX_WINDOW_SIZE , next_packet_to_send, next_packet_seq)) {
        memcpy(&pkt, &(windows[next_packet_to_send % MAX_WINDOW_SIZE]), sizeof(packet));
        int seq = 0;
        memcpy(&seq ,pkt.data + sizeof(short),sizeof(int));
        // fprintf(stdout, "At %.2fs: sender send_packets seq %d...\n", GetSimulationTime(), seq);
        Sender_ToLowerLayer(&pkt);
        next_packet_to_send = (next_packet_to_send + 1);
    }
    // fprintf(stdout, "At %.2fs: sender send_packets end...\n", GetSimulationTime());
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    msg_num = 0;
    cur_msg_byte = 0;

    memset(windows, 0, (MAX_WINDOW_SIZE) * sizeof(packet));
    next_seq = 0;
    next_packet_seq = 0;
    next_packet_to_send = 0;
    expected_ack = 0;
    packet_num = 0;

    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    // fprintf(stdout, "At %.2fs: sender from upper layer ...\n", GetSimulationTime());
    if(msg_num >= MAX_BUFFER_SIZE) {
        ASSERT(0);
    }

    message *new_msg = new message();
    int new_size = msg->size + sizeof(int);
    new_msg->size = new_size;
    new_msg->data = (char *) malloc(new_size);
    memcpy(new_msg->data, &msg->size, sizeof(int));
    memcpy(new_msg->data + sizeof(int), msg->data, msg->size);
    msg_num++;

    msg_buffer.push(new_msg);
    // fprintf(stdout, "At %.2fs: sender from upper layer msg_buffer size %d ...\n", GetSimulationTime(), (int)msg_buffer.size());

    if(Sender_isTimerSet()){
        // fprintf(stdout, "At %.2fs: sender from upper layer Sender_isTimerSet ...\n", GetSimulationTime());
        return;
    }

    // fprintf(stdout, "At %.2fs: sender from upper layer start timer ...\n", GetSimulationTime());
    Sender_StartTimer(TIMEOUT);

    from_buffer_to_windows();
    send_packets();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    short check;
    memcpy(&check, pkt->data, sizeof(short));
    if(check != checksum(pkt)) return;

    int ack;
    memcpy(&ack, pkt->data + sizeof(short), sizeof(int));
    // fprintf(stdout, "At %.2fs: sender from lower layer ack %d\n", GetSimulationTime(), ack);
    if(between(expected_ack, ack , next_packet_seq)){
        Sender_StartTimer(TIMEOUT);
        packet_num -= (ack - expected_ack + 1);
        expected_ack = (ack+1);
        from_buffer_to_windows();
        send_packets();
    }
    if(ack == (next_packet_seq - 1)) Sender_StopTimer();
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    // fprintf(stdout, "At %.2fs: sender time out ...\n", GetSimulationTime());
    Sender_StartTimer(TIMEOUT);
    next_packet_to_send = expected_ack;
    from_buffer_to_windows();
    send_packets();
}
