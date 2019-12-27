#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cerrno>
#include <string>
#include "Message.hpp"
#include "zmq.h"
using namespace std;

string first_addr_conn = "tcp://localhost:16776";
string base_addr = "tcp://localhost:";
int time_wait = 4000;

void* context = zmq_ctx_new();
void* publisher = zmq_socket(context, ZMQ_SUB);
void* socket_push = zmq_socket(context, ZMQ_PUSH);

void send_message(void* socket, Message mes){
    zmq_msg_t req;
    zmq_msg_init_size(&req, sizeof(Message));
    memcpy(zmq_msg_data(&req), &mes, sizeof(Message));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}

Message recv_message(void* socket){
    zmq_msg_t ans;
    Message *res;
    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, socket, 0);
    res = (Message *) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    return (*res);
}

string update_text(Message a, void* socket){
    int c = a.length / CHAR_LEN + 1;
    string res;
    cout<<a.length<<endl;
    for(int i=0; i<c; i++){
        cout<<a.data<<endl;  //какая то фигня с обработкой текста
        res.append(a.data);
        a = recv_message(socket);
    }
    return res;
}

void connect(void*);

int main(int argc, char const *argv[]){
    connect(context);

    zmq_close(publisher);
    zmq_close(socket_push);
    zmq_ctx_destroy(context);
}

void connect(void* context){
    void* first_connect = zmq_socket(context, ZMQ_REQ);
    if(0>zmq_connect(first_connect, first_addr_conn.c_str())){
        cout<<"First connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_setsockopt(first_connect, ZMQ_RCVTIMEO, &time_wait, sizeof(int));

    OnStartMessage r, *a;
    zmq_msg_t ans, req;
    memcpy(r.addres,"192.168", 7);
    zmq_msg_init_size(&req, sizeof(OnStartMessage));
    memcpy(zmq_msg_data(&req), &r, sizeof(OnStartMessage));
    zmq_msg_send(&req, first_connect, 0);
    zmq_msg_close(&req);

    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, first_connect, 0);
    a = (OnStartMessage*) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    zmq_close(first_connect);

    cout<<"pub="<<a->port_pub<<"::"<<"push="<<a->port_push<<endl;
    if(0>zmq_connect(publisher, (base_addr+to_string(a->port_pub)).c_str())){
        cout<<"Publisher connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_setsockopt(publisher, ZMQ_SUBSCRIBE, "", 0);
    if(0>zmq_connect(socket_push, (base_addr+to_string(a->port_push)).c_str())){
        cout<<"Push connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    cout<<"Connected"<<endl;
}