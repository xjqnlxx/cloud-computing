/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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

#include "rdt_struct.h"
#include "rdt_receiver.h"

using namespace std;

#define HEADER_SIZE 7
#define WINDOWS_SIZE 10
#define MAX_PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)

static message* cur_msg;
static int cur_msg_byte;

static int expected_pkt_seq;

static packet buffer[WINDOWS_SIZE];

static bool valid_checks[WINDOWS_SIZE];

static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}

static void send_ack(int ack) {
    packet ack_packet;
    memcpy(ack_packet.data + sizeof(short), &ack, sizeof(int));
    short sum = checksum(&ack_packet);
    memcpy(ack_packet.data, &sum, sizeof(short));

    // fprintf(stdout, "At %.2fs: receiver send ack %d \n", GetSimulationTime(), ack);

    Receiver_ToLowerLayer(&ack_packet);
}

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    cur_msg= (message *)malloc(sizeof(message));
    memset(cur_msg, 0, sizeof(message));
    cur_msg_byte = 0;

    expected_pkt_seq = 0;
    memset(buffer, 0, WINDOWS_SIZE * sizeof(packet));
    memset(valid_checks, 0, WINDOWS_SIZE);
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    free(cur_msg);
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

static bool between(int a, int b, int c){
    if((b>=a && b<c) ) return true;
    else return false;
}
/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    // fprintf(stdout, "At %.2fs: receiver From LowerLayer\n", GetSimulationTime());
    short check;
    memcpy(&check, pkt->data, sizeof(short));

    if(check != checksum(pkt)) {
        // fprintf(stdout, "At %.2fs: receiver checksum wrong\n", GetSimulationTime());
        return;
    }

    int packet_seq = 0, payload_size = 0;
    memcpy(&packet_seq, pkt->data + sizeof(short), sizeof(int));

    if(packet_seq == expected_pkt_seq){
        while(true){
        // fprintf(stdout, "At %.2fs: receiver expected_pkt_seq %d\n", GetSimulationTime(), expected_pkt_seq);
        expected_pkt_seq = (expected_pkt_seq+1);
        payload_size = pkt->data[HEADER_SIZE - 1];

        if(cur_msg_byte == 0){
            if(cur_msg->size != 0) free(cur_msg->data);
            memcpy(&cur_msg->size, pkt->data + HEADER_SIZE, sizeof(int));
            cur_msg->data = (char*) malloc(cur_msg->size);
            memcpy(cur_msg->data, pkt->data + HEADER_SIZE + sizeof(int), payload_size - sizeof(int));
            cur_msg_byte += payload_size - sizeof(int);
        } 
        else 
        {
            memcpy(cur_msg->data + cur_msg_byte, pkt->data + HEADER_SIZE, payload_size);
            cur_msg_byte += payload_size;
        }

        if(cur_msg->size == cur_msg_byte) {
            Receiver_ToUpperLayer(cur_msg);
            cur_msg_byte = 0;
            cur_msg->size = 0;
            free(cur_msg->data);
        }

        // fprintf(stdout, "At %.2fs: receiver valid_checks[expected_pkt_seq WINDOWS_SIZE] %d\n", GetSimulationTime(), valid_checks[expected_pkt_seq % WINDOWS_SIZE]);
        if(valid_checks[expected_pkt_seq % WINDOWS_SIZE]){
            pkt = &buffer[expected_pkt_seq % WINDOWS_SIZE];
            memcpy(&packet_seq, pkt->data + sizeof(short), sizeof(int));
            valid_checks[expected_pkt_seq % WINDOWS_SIZE] = false;
        } 
        else {
            break;
        }
        }
        // fprintf(stdout, "At %.2fs: receiver From LowerLayer send ack %d\n", GetSimulationTime(), packet_seq);
        send_ack(packet_seq);
        // fprintf(stdout, "At %.2fs: receiver FromLowerLayer end\n", GetSimulationTime());
        return;
    }
    
    if(between(expected_pkt_seq, packet_seq, expected_pkt_seq+WINDOWS_SIZE)) {
        if(!valid_checks[packet_seq % WINDOWS_SIZE]) {
            memcpy(&(buffer[packet_seq % WINDOWS_SIZE].data), pkt->data, RDT_PKTSIZE);
            valid_checks[packet_seq % WINDOWS_SIZE] = 1;
        }
    }
    send_ack(expected_pkt_seq - 1);
    return;
}