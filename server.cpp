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
void* inf_listen_pull(void* args);
void* pull_port_listener(void* args);

void send_message(void* socket, Message mes){
    zmq_msg_t req;
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
string update_text(int length, void* socket){
    int c = length / CHAR_LEN + 1;
    string res;
    Message a;
    for(int i=0; i < c; i++){
        a = recv_message(socket);
        res.append(a.data);
    }
    return res;
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
    pthread_t inf_connect_listener, inf_pull_listener;
    if(0>zmq_bind(main_socket, ("tcp://*:"+to_string(port_subscribe)).c_str())){
        cout<<"Main socket :: "<<strerror(errno)<<endl;
        exit(1);
    }
    if(0>zmq_bind(main_pull_socket, ("tcp://*:"+to_string(port_send_pull)).c_str())){
        cout<<"Main pull socket :: "<<strerror(errno)<<endl;
        exit(1);
    }

    pthread_create(&inf_connect_listener, NULL, inf_listen_connect, context);
    pthread_detach(inf_connect_listener);
    pthread_create(&inf_pull_listener, NULL, inf_listen_pull, main_pull_socket);
    pthread_detach(inf_pull_listener);

    string text;
    bool flag = true;
    while(flag){
        getline(cin, text);
        if(text == "!/exit"){
            flag = false;
        }else{
            send_text_to_sub(main_socket, &text);
        }
    }

    zmq_close(main_socket);
    zmq_close(main_pull_socket);
    zmq_ctx_destroy(context);
}

void* inf_listen_connect(void * args){
    pthread_t conn_thread;
    cout<<"Connecting port start listening"<<endl;
    while(true){
        pthread_create(&conn_thread, NULL, connecting_port_listening, args);
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
    if(r->name != 0){
        user_ip = r->name;
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

void* inf_listen_pull(void* args){
    pthread_t pthread;
    void* socket = (void*) args;
    cout<<endl<<"Pull port start listening"<<endl;
    zmq_bind(socket, ("tcp://*:"+to_string(port_send_pull)).c_str());
    while(true){
        pthread_create(&pthread, NULL, pull_port_listener, socket);
        pthread_join(pthread, NULL);
    }
    zmq_unbind(socket, ("tcp://*:"+to_string(port_send_pull)).c_str());
}
void* pull_port_listener(void* args){
    void* socket = (void*) args;
    Message m = recv_message(socket);
    cout<<"From :: "<<m.from<<endl;
    switch(m.task){
        case -1:
            cout<<"Disconnect"<<endl;
            for(int i = 0; i<users.size(); i++){
                if(users.at(i) == m.from){
                    users.erase(users.begin() + i);
                }
            }
        case 2:{
            string text = update_text(m.length, socket);
            cout<<"text for update :: "<<text<<endl;
            send_text_to_sub(main_socket, &text);
            break;
        }
    }
    return NULL;
}