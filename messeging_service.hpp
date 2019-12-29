#include "Message.hpp"
#include "zmq.h"
#include <string>

void send_message(void* socket, Message mes){
    zmq_msg_t req;
    // memset(mes.from , 0, CHAR_LEN);
    // memcpy(mes.from, user_name.c_str(), user_name.length());
    zmq_msg_init_size(&req, sizeof(Message));
    memcpy(zmq_msg_data(&req), &mes, sizeof(Message));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}

Message recv_message(void* socket){
    zmq_msg_t ans;
    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, socket, 0);
    Message * res = (Message *) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    return (*res);
}
void send_message(void* socket, OnStartMessage mes){
    zmq_msg_t req;
    zmq_msg_init_size(&req, sizeof(OnStartMessage));
    memcpy(zmq_msg_data(&req), &mes, sizeof(OnStartMessage));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}

OnStartMessage recv_message_start(void* socket){
    zmq_msg_t ans;
    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, socket, 0);
    OnStartMessage * res = (OnStartMessage *) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    return (*res);
}
void send_changes(void* socket, string* text){
    Message a;
    zmq_msg_t req;
    a.length = text->length();
    a.task = 2;
    send_message(socket, a);
    int prev=0;
    for(int i=1; i <= text->length() / CHAR_LEN + 1; i++){
        memset(a.data, 0, CHAR_LEN);
        memcpy(a.data, text->substr(prev, i*CHAR_LEN).c_str(), text->length());
        send_message(socket, a);
        prev = i*CHAR_LEN;
    }
}