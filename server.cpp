#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include "zmq.h"
#include "Message.hpp"
#include <sys/wait.h>
#include <sys/types.h>
using namespace std;

string addr_pull = "tcp://127.0.0.1:16776";
int port_send = 7523;
int port_subscribe = 4040;

vector<void *> sockets_pull;
void* context = zmq_ctx_new();
void* create_connect = zmq_socket(context, ZMQ_REP);

void* connecting_port_listening(void * args);
void* inf_listen_connect(void * args);
void* connect_new(void* args);

void send_text_to_sub(void* socket, string text){
    Message a;
    zmq_msg_t req;
    a.length = text.length();
    a.task = 1;
    int prev = 0;
    for(int i = 0; i <= a.length / CHAR_LEN + 1; i++){
        memcpy(a.data, text.substr(prev, i*CHAR_LEN).c_str(), CHAR_LEN);
        zmq_msg_init_size(&req, sizeof(Message));
        memcpy(zmq_msg_data(&req), &a, sizeof(Message));
        zmq_msg_send(&req, socket, 0);
        zmq_msg_close(&req);
        prev = i*CHAR_LEN;
    }
}

int main(){
    pthread_t inf_thread;
	void* main_socket = zmq_socket(context, ZMQ_PUB);
    zmq_bind(main_socket, ("tcp://*:"+to_string(port_subscribe)).c_str());

    pthread_create(&inf_thread, NULL, inf_listen_connect, context);
    pthread_detach(inf_thread);

    string text;
    for(;;){
        getline(cin, text);
        send_text_to_sub(main_socket, text);
    }

    zmq_close(main_socket);
    zmq_ctx_destroy(context);
}

void* inf_listen_connect(void * args){
    void* con =(void *) args;
    pthread_t conn_thread;
    while(true){
        cout<<"Start listening"<<endl;
        pthread_create(&conn_thread, NULL, connecting_port_listening, con);
        pthread_join(conn_thread,NULL);
    }
    return NULL;
}

void* connecting_port_listening(void* args){
    if(0>zmq_bind(create_connect, addr_pull.c_str())){
        cout<<"create connect::"<<strerror(errno)<<endl;
        exit(1);
    }
    void* con =(void *) args;
    pthread_t create_thr;

    zmq_msg_t req, ans;
    zmq_msg_init(&req);
    zmq_msg_recv(&req, create_connect, 0);
    Message a, *m = (Message *) zmq_msg_data(&req);
    zmq_msg_close(&req);

    if(m->task == 0){
        void* new_socket = zmq_socket(con, ZMQ_PULL); 
        if(0>zmq_bind(new_socket, ("tcp://127.0.0.1:"+to_string(port_send)).c_str())){
            cout<<"new socket::"<<strerror(errno)<<endl;
            pthread_exit(NULL);
            zmq_unbind(create_connect, addr_pull.c_str());
        }
        a.port = port_send;
        a.task = port_subscribe;
        a.status = 0;

        zmq_msg_init_size(&ans,sizeof(Message));
        memcpy(zmq_msg_data(&ans), &a, sizeof(Message));
        zmq_msg_send(&ans, create_connect, 0);
        zmq_msg_close(&ans);
        zmq_msg_init(&ans);
        zmq_msg_recv(&ans, new_socket, 0);
        m = (Message *) zmq_msg_data(&ans);
        zmq_msg_close(&ans);
        if(m->task == 0 && m->status == 0){
            cout<<"New connection"<<endl;
            sockets_pull.push_back(new_socket);
        }else{
            zmq_unbind(new_socket, ("tcp://127.0.0.1:"+to_string(port_send)).c_str());
            zmq_close(new_socket);
        }
        cout<<"Somebody connected"<<endl;
        port_send++;
    }
    zmq_unbind(create_connect, addr_pull.c_str());
    return NULL;
}
