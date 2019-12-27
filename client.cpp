#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cerrno>
#include <string>
#include "Message.hpp"
#include "zmq.h"
using namespace std;

string first_connection = "tcp://localhost:16776";
string base_addr = "tcp://localhost:";

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

int main(int argc, char const *argv[]){
    void* context = zmq_ctx_new();
    void* connect_socket = zmq_socket(context, ZMQ_REQ);
	void* server = zmq_socket(context, ZMQ_SUB);
    void* socket_send = zmq_socket(context, ZMQ_PUSH);
    string all_text;

    if(0>zmq_connect(connect_socket, first_connection.c_str())){
        cout<<strerror(errno)<<endl;
        exit(1);
    }

    Message m , a;
    m.task = 0;
    m.status = 0;
    send_message(connect_socket, m);
    a = recv_message(connect_socket);

    if(a.status != 0){
        cout<<"Refused"<<endl;
        exit(1);
    }
    zmq_close(connect_socket);

    if(0>zmq_connect(socket_send, (base_addr+to_string(a.port)).c_str())){
        cout<<(base_addr+to_string(a.task)).c_str()<<endl;
        cout<<strerror(errno)<<endl;
        exit(1);
    }
    m.status = 0;
    m.task = 0;
    send_message(socket_send, m);

    if(0>zmq_connect(server, (base_addr+to_string(a.task)).c_str())){
        cout<<(base_addr+to_string(a.task)).c_str()<<endl;
        cout<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_setsockopt(server, ZMQ_SUBSCRIBE, 0, 0);
    
    cout<<"Subscribed on "<<(base_addr+to_string(a.task)).c_str()<<endl;
    cout<<"Push socket on "<<(base_addr+to_string(a.port)).c_str()<<endl;

    for(;;){
        a = recv_message(server);
        if(a.task == 1){ //recive text
            all_text.clear();
            all_text = update_text(a, server);
        }
    }

    zmq_ctx_destroy(context);

}