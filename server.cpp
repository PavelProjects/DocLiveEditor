#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <mutex>
#include <fstream>
#include <sys/wait.h>
#include "zmq.h"
#include "Message.hpp"
#include "MyText.hpp"
using namespace std;
#define SAVE_TIME_WAIT 5

string addr_first_connect = "tcp://*:16776";
string file_path = "./united.txt";
int port_pull = 7523;
int port_publisher = 4040;
int time_wait = 10000;
mutex mute_pr_state;
bool pr_state = false;
mutex mute_save_state;
bool save_state = false;

mutex mute_users;
vector<string> users;
mutex mute_pull;
vector<Message> pull_messages;
mutex mute_text;
Text full_text;

void* context = zmq_ctx_new();
void* main_pub_socket = zmq_socket(context, ZMQ_PUB);
void* main_pull_socket = zmq_socket(context, ZMQ_PULL);

void* connecting_port_listening(void * args);
void* inf_listen_connect(void * args);
void* inf_listen_pull(void* args);
void* pull_port_listener(void* args);
void* process_message(void* args);
void* wait_and_save(void* args);

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

void* send_text(void* args){
    void* socket_push = zmq_socket(context, ZMQ_PUSH);
    mute_users.lock();
    zmq_connect(socket_push, ("tcp://"+users.back()+":2020").c_str());
    mute_users.unlock();
    zmq_setsockopt(socket_push, ZMQ_RCVTIMEO, &time_wait, sizeof(int));
    mute_text.lock();
    Message m;
    m.task = FULL_TEXT;
    m.size = full_text.size();
    memset(m.data, 0, CHAR_LEN);
    memcpy(m.data, file_path.c_str(), file_path.length());
    send_message(socket_push, m);
    int prev = 0;
    for(int i = 0; i < full_text.size(); i++){
        m.size = full_text.at_st(i).size();
        memset(m.data, 0, CHAR_LEN);
        send_message(socket_push, m);
        for(int j=1; j <= full_text.at_st(i).size() / CHAR_LEN + 1; j++){
            prev = 0;
            if(j*CHAR_LEN < full_text.at_st(i).size()){
                memcpy(m.data, full_text.at_st(i).substr(prev, j*CHAR_LEN).c_str(),  full_text.at_st(i).substr(prev, j*CHAR_LEN).size());
                m.change = 'a';
                prev = j*CHAR_LEN;
            }else{
                memcpy(m.data, full_text.at_st(i).substr(prev, full_text.at_st(i).size()).c_str(),  full_text.at_st(i).size() - prev);
                m.change = 'e';
            }
            send_message(socket_push, m);
        }
    }
    mute_text.unlock();
    zmq_close(socket_push);
    return NULL;
}

void update_text(Message m){
    mute_text.lock();
    switch(m.wch){
        case DELETE_CH:
            if(full_text.at_st(m.where_y).back() == '\n'){
                if(full_text.at_st(m.where_y).length() > 1){
                    if(m.where_x == 0){
                        if(m.where_y != 0){
                            full_text.append_with_prev(m.where_y);
                        }
                    }else{
                        full_text.erase(m.where_y, m.where_x);
                    }
                }else{
                    full_text.erase(m.where_y, m.where_x);
                    if(m.where_y != 0){
                        full_text.erase(m.where_y);
                    }
                }
            }else{
                if(m.where_x == full_text.at_st(m.where_y).length()){
                    if(full_text.at_st(m.where_y).length() >= 1){
                        full_text.pop_back_c(m.where_y);
                    }else if(full_text.at_st(m.where_y).length() == 0){
                        if(m.where_y != 0){
                            full_text.erase(m.where_y);
                        }
                    }
                }else if(m.where_x >= 0){
                    if(m.where_x == 0){
                        if(m.where_y != 0){
                            full_text.append_with_prev(m.where_y);
                        }
                    }else{
                        full_text.erase(m.where_y, m.where_x-1);
                    }
                }
            }
            break;
        case INSERT_CH:
            full_text.insert(m.where_y, m.where_x, m.change);
            break;
    }
    cout<<"Text Update"<<endl;
    for(int i=0; i< full_text.size(); i++){
        cout<<full_text.at_st(i);
    }
    cout<<endl<<"--------------------"<<endl;
    mute_text.unlock();
}


int main(int argc, char* argv[]){
    if(argc != 4){
        cout<<"Wrong using!"<<endl;
        cout<<"./server /path/to/file port_publicher port_pull"<<endl;
        return 0;
    }
    file_path = argv[1];
    if(!full_text.loadFromFile(file_path)){
        return 0;
    }
    port_publisher = atoi(argv[2]);
    port_pull = atoi(argv[3]);
    cout<<"Loaded file :: "<<file_path<<endl;
    cout<<"Port publisher :: "<<port_publisher<<endl;
    cout<<"Port pull :: "<<port_pull<<endl;

    pthread_t inf_connect_listener, inf_pull_listener;
    if(0>zmq_bind(main_pub_socket, ("tcp://*:"+to_string(port_publisher)).c_str())){
        cout<<"Main socket :: "<<strerror(errno)<<endl;
        exit(1);
    }
    if(0>zmq_bind(main_pull_socket, ("tcp://*:"+to_string(port_pull)).c_str())){
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
            full_text.writeToFile(file_path);
            Message m;
            m.task = DISCONNECT;
            send_message(main_pub_socket, m);
            flag = false;
        }
        if(text == "show"){
            for(int i = 0; i< full_text.size(); i++){
                cout<<full_text.at_st(i);
            }
            cout<<endl;
        }
        if(text == "save"){
            full_text.writeToFile(file_path);
        }
    }

    zmq_close(main_pub_socket);
    zmq_close(main_pull_socket);
    zmq_ctx_destroy(context);
}

void* inf_listen_connect(void * args){
    pthread_t conn_thread, thsnd;
    cout<<"Connecting port start listening"<<endl;
    while(true){
        pthread_create(&conn_thread, NULL, connecting_port_listening, args);
        pthread_join(conn_thread,NULL);
        pthread_create(&thsnd, NULL, send_text, NULL);
        pthread_detach(thsnd);
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
        mute_users.lock();
        users.push_back(user_ip);
        mute_users.unlock();
        a.port_pub = port_publisher;
        a.port_push = port_pull;
        a.users = users.size();
        send_message(first_connect, a);

        cout<<"New user connected"<<endl;
        for(int i=0; i<users.size(); i++){
            cout<<i<<"::"<<users.at(i)<<endl;
        }
        Message m;
        m.task = USER_CONNECTED;
        send_message(main_pub_socket, m);
    }
    zmq_unbind(first_connect, addr_first_connect.c_str());
    zmq_close(first_connect);
    return NULL;
}

void* inf_listen_pull(void* args){
    pthread_t pthread;
    thread_data* sockets = (thread_data*) args;
    cout<<endl<<"Pull port start listening"<<endl;
    zmq_bind(sockets->pull, ("tcp://*:"+to_string(port_pull
)).c_str());
    while(true){
        pthread_create(&pthread, NULL, pull_port_listener, sockets);
        pthread_join(pthread, NULL);
    }
    zmq_unbind(sockets->pull, ("tcp://*:"+to_string(port_pull
)).c_str());
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

void* wait_and_save(void* args){
    sleep(SAVE_TIME_WAIT);
    cout<<"---saved---"<<endl;
    full_text.writeToFile(file_path);
    mute_save_state.lock();
    save_state = false;
    mute_save_state.unlock();
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
                m.task = USER_DISCONNECTED;
                send_message(main_pub_socket, m);
                break;
            case UPDATE_TEXT:{
                update_text(m);
                if(!save_state){
                    pthread_t pth;
                    pthread_create(&pth, NULL, wait_and_save, NULL);
                    pthread_detach(pth);
                    mute_save_state.lock();
                    save_state = true;
                    mute_save_state.unlock();
                }
                send_message(socket, m);
                break;
            }
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