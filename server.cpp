#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <mutex>
#include "zmq.h"
#include "Message.hpp"
using namespace std;

string addr_first_connect = "tcp://*:16776";
int port_send_pull = 7523;
int port_subscribe = 4040;
mutex mute_pr_state;
bool pr_state = false;

mutex mute_users;
vector<string> users;
mutex mute_pull;
vector<Message> pull_messages;
mutex mute_text;
vector<string> full_text;

void* context = zmq_ctx_new();
void* main_pub_socket = zmq_socket(context, ZMQ_PUB);
void* main_pull_socket = zmq_socket(context, ZMQ_PULL);

void* connecting_port_listening(void * args);
void* inf_listen_connect(void * args);
void* inf_listen_pull(void* args);
void* pull_port_listener(void* args);
void* process_message(void* args);

void send_message(void* socket, Message mes){
    zmq_msg_t req;
    zmq_msg_init_size(&req, sizeof(Message));
    memcpy(zmq_msg_data(&req), &mes, sizeof(Message));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}
void send_message(void* socket, OnStartMessage mes){
    zmq_msg_t req;
    zmq_msg_init_size(&req, sizeof(OnStartMessage));
    memcpy(zmq_msg_data(&req), &mes, sizeof(OnStartMessage));
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
OnStartMessage on_start_recv(void* socket){
    zmq_msg_t ans;
    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, socket, 0);
    OnStartMessage * res = (OnStartMessage *) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    return (*res);
}

void send_text(){
    void* socket_push = zmq_socket(context, ZMQ_PUSH);
    zmq_connect(socket_push, "tcp://localhost:2020");
    mute_text.lock();
    Message m;
    m.task = FULL_TEXT;
    m.size = full_text.size();
    send_message(socket_push, m);
    int prev = 0;
    for(int i = 0; i < full_text.size(); i++){
        m.size = full_text.at(i).size();
        send_message(socket_push, m);
        for(int j=1; j <= full_text.at(i).size() / CHAR_LEN + 1; j++){
            memcpy(m.data, full_text.at(i).substr(prev, j*CHAR_LEN).c_str(),  full_text.at(i).substr(prev, j*CHAR_LEN).size());
            send_message(socket_push, m);
            cout<<m.data<<endl;
            prev = i*CHAR_LEN;
        }
    }
    mute_text.unlock();
    zmq_close(socket_push);
}

void update_text(Message m){
    mute_text.lock();
    int key = m.change;
    int x = m.where_x;
    int y = m.where_y;
    if(full_text.size() == 0){
        string line;
        full_text.push_back(line);
        m.wch = ADD_EMPTY_LINE;
    }
    if(m.wch == DELETE_CH){
        if(full_text.at(y).size()>0){
            full_text.at(y).erase(full_text.at(y).begin() + x);
        }
    }else if(m.wch == DELETE_LINE){
        full_text.erase(full_text.begin()+y);
    }else{
        if(key == '\n'){
        string n;
        if(x == full_text.at(y).length()-1){
            x = 0;
            if(y == full_text.size()-1){
                full_text.push_back(n);
                m.wch = ADD_EMPTY_LINE;
            }else{
                m.where_y = y;
                full_text.insert(full_text.begin()+y, n);
                m.wch = INSERT_EMPTY_LINE;
            }
        }else{
            m.where_x = x;
            m.where_y = y;
            m.change = key;
            n = full_text.at(y).substr(0, x);
            full_text.at(y) = full_text.at(y).substr(x, full_text.at(y).length());
            n.push_back(key);
            full_text.insert(full_text.begin() + y, n);
            m.wch = SPLIT_LINE_AND_INSERT;
            x = 0;
        }
        }else{
        if(x >= full_text.at(y).length()-1){
            if(full_text.at(y).back() != 10){
                m.where_y = y;
                m.change = key;
                full_text.at(y).push_back(key);
                m.wch = ADD_CH;
            }else{ 
                m.where_y = y;
                m.where_x = 0;
                m.change = key;
                full_text.at(y).insert(full_text.at(y).end() - 1, key);
                m.wch = INSERT_CH;
            }
        }else{
            m.where_y = y;
            m.where_x = x;
            m.change = key;
            full_text.at(y).insert(full_text.at(y).begin() + x, key);
            m.wch = INSERT_CH;
        }
        }
    }
    cout<<"Text Update"<<endl;
    for(int i=0; i< full_text.size(); i++){
        cout<<full_text.at(i);
    }
    cout<<endl<<"--------------------"<<endl;
    mute_text.unlock();
}

int main(){
    pthread_t inf_connect_listener, inf_pull_listener;
    if(0>zmq_bind(main_pub_socket, ("tcp://*:"+to_string(port_subscribe)).c_str())){
        cout<<"Main socket :: "<<strerror(errno)<<endl;
        exit(1);
    }
    if(0>zmq_bind(main_pull_socket, ("tcp://*:"+to_string(port_send_pull)).c_str())){
        cout<<"Main pull socket :: "<<strerror(errno)<<endl;
        exit(1);
    }

    pthread_create(&inf_connect_listener, NULL, inf_listen_connect, context);
    pthread_detach(inf_connect_listener);
    thread_data data;
    data.pull = main_pull_socket;
    data.pub = main_pub_socket;
    pthread_create(&inf_pull_listener, NULL, inf_listen_pull, &data);
    pthread_detach(inf_pull_listener);

    string text;
    bool flag = true;
    while(flag){
        getline(cin, text);
        if(text == "!/exit"){
            flag = false;
        }
        Message m;
        m.task = UPDATE_TEXT;
        send_message(main_pub_socket, m);
    }

    zmq_close(main_pub_socket);
    zmq_close(main_pull_socket);
    zmq_ctx_destroy(context);
}

void* inf_listen_connect(void * args){
    pthread_t conn_thread;
    cout<<"Connecting port start listening"<<endl;
    while(true){
        pthread_create(&conn_thread, NULL, connecting_port_listening, args);
        pthread_join(conn_thread,NULL);
        send_text();
    }
    return NULL;
}

void* connecting_port_listening(void* args){
    void* con = (void *) args;
    void* first_connect = zmq_socket(con, ZMQ_REP);
    if(0>zmq_bind(first_connect, addr_first_connect.c_str())){
        cout<<"First connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    string user_ip;

    zmq_msg_t req, ans;
    OnStartMessage a, r;
    r = on_start_recv(first_connect);
    if(r.name != 0){
        user_ip = r.name;
        a.port_pub = port_subscribe;
        a.port_push = port_send_pull;
        send_message(first_connect, a);

        mute_users.lock();
        users.push_back(user_ip);
        mute_users.unlock();
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
    thread_data* sockets = (thread_data*) args;
    cout<<endl<<"Pull port start listening"<<endl;
    zmq_bind(sockets->pull, ("tcp://*:"+to_string(port_send_pull)).c_str());
    while(true){
        pthread_create(&pthread, NULL, pull_port_listener, sockets);
        pthread_join(pthread, NULL);
    }
    zmq_unbind(sockets->pull, ("tcp://*:"+to_string(port_send_pull)).c_str());
    return NULL;
}
void* pull_port_listener(void* args){
    thread_data* sockets = (thread_data *) args;
    Message m = recv_message(sockets->pull);
    mute_pull.lock();
    pull_messages.push_back(m);
    mute_pull.unlock();
    cout<<m.from<<"::"<<m.task<<endl;
    mute_pr_state.lock();
    if(!pr_state){
        pthread_t pth;
        pthread_create(&pth, NULL, process_message, sockets->pub);
        pthread_detach(pth);
        pr_state = true;
    }
    mute_pr_state.unlock();
    return NULL;
}

void* process_message(void* args){
    void* socket = (void*) args;
    bool flag = true;
    while(flag){ 
        mute_pull.lock();
        Message m = pull_messages.front();
        mute_pull.unlock();
        switch(m.task){
            case DISCONNECT:
                mute_users.lock();
                for(int i=0; i < users.size(); i++){
                    if(users.at(i) == m.from) users.erase(users.begin() + i);
                }
                mute_users.unlock();
                break;
            case UPDATE_TEXT:
                update_text(m);
                send_message(socket, m);
                break;
        }

        mute_pull.lock();
        pull_messages.erase(pull_messages.begin());
        if(pull_messages.size() == 0){
            flag = false;
        }
        mute_pull.unlock();
    }
    mute_pr_state.lock();
    pr_state = false;
    mute_pr_state.unlock();
    return NULL;
}