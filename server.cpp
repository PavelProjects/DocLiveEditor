#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include "zmq.h"
#include "Message.hpp"
using namespace std;

string addr_first_connect = "tcp://127.0.0.1:16776";
int port_send_pull = 7523;
int port_subscribe = 4040;

vector<string> users;

void* context = zmq_ctx_new();
void* main_socket = zmq_socket(context, ZMQ_PUB);
void* main_pull_socket = zmq_socket(context, ZMQ_PULL);


void* connecting_port_listening(void * args);
void* inf_listen_connect(void * args);

void send_message(void* socket, Message mes){
    zmq_msg_t req;
    zmq_msg_init_size(&req, sizeof(Message));
    memcpy(zmq_msg_data(&req), &mes, sizeof(Message));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}

void send_text_to_sub(void* socket, string* text){
    Message a;
    zmq_msg_t req;
    a.length = text->length();
    a.task = 1;
    send_message(socket, a);
    int prev=0;
    for(int i=1; i <= text->length() / CHAR_LEN + 1; i++){
        memset(a.data, 0, CHAR_LEN);
        memcpy(a.data, text->substr(prev, i*CHAR_LEN).c_str(), text->length());
        send_message(socket, a);
        prev = i*CHAR_LEN;
    }
}

int main(){
    pthread_t inf_thread;
    if(0>zmq_bind(main_socket, ("tcp://*:"+to_string(port_subscribe)).c_str())){
        cout<<"Main socket :: "<<strerror(errno)<<endl;
        exit(1);
    }
    if(0>zmq_bind(main_pull_socket, ("tcp://*:"+to_string(port_send_pull)).c_str())){
        cout<<"Main pull socket :: "<<strerror(errno)<<endl;
        exit(1);
    }

    pthread_create(&inf_thread, NULL, inf_listen_connect, context);
    pthread_detach(inf_thread);

    string text;
    for(;;){
        getline(cin, text);
        send_text_to_sub(main_socket, &text);
    }

    zmq_close(main_socket);
    zmq_close(main_pull_socket);
    zmq_ctx_destroy(context);
}

void* inf_listen_connect(void * args){
    void* con =(void *) args;
    pthread_t conn_thread;
        cout<<"Start listening"<<endl;
    while(true){
        pthread_create(&conn_thread, NULL, connecting_port_listening, con);
        pthread_join(conn_thread,NULL);
    }
    return NULL;
}

void* connecting_port_listening(void* args){
    void* con = (void *) args;
    void* first_connect = zmq_socket(con, ZMQ_REP);
    string user_ip;
    if(0>zmq_bind(first_connect, addr_first_connect.c_str())){
        cout<<"First connect :: "<<strerror(errno)<<endl;
        exit(1);
    }

    zmq_msg_t req, ans;
    OnStartMessage *r, a;
    zmq_msg_init(&req);
    zmq_msg_recv(&req, first_connect, 0);
    r = (OnStartMessage *) zmq_msg_data(&req);
    zmq_msg_close(&req);
    if(r->addres != 0){
        user_ip = r->addres;
        a.port_pub = port_subscribe;
        a.port_push = port_send_pull;
        zmq_msg_init_size(&ans, sizeof(OnStartMessage));
        memcpy(zmq_msg_data(&ans), &a, sizeof(OnStartMessage));
        zmq_msg_send(&ans, first_connect, 0);
        zmq_msg_close(&ans);

        users.push_back(user_ip);
        cout<<"New user connected"<<endl;
        for(int i=0; i<users.size(); i++){
            cout<<i<<"::"<<users.at(i)<<endl;
        }
    }
    zmq_unbind(first_connect, addr_first_connect.c_str());
    zmq_close(first_connect);
    return NULL;
}
