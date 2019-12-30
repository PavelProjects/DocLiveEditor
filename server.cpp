#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>
#include "zmq.h"
#include "Message.hpp"
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
vector<string> full_text;

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
        m.size = full_text.at(i).size();
        memset(m.data, 0, CHAR_LEN);
        send_message(socket_push, m);
        for(int j=1; j <= full_text.at(i).size() / CHAR_LEN + 1; j++){
            memcpy(m.data, full_text.at(i).substr(prev, j*CHAR_LEN).c_str(),  full_text.at(i).substr(prev, j*CHAR_LEN).size());
            send_message(socket_push, m);
            prev = i*CHAR_LEN;
        }
    }
    mute_text.unlock();
    zmq_close(socket_push);
    return NULL;
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

void load_from_file(){
    mute_text.lock();
    ifstream file(file_path, ios::in|ios::binary|ios::ate);
    if(file.is_open()){
        string n;
        full_text.push_back(n);
        int size = file.tellg();
        char* lod = new char [size];
        file.seekg (0, ios::beg);
        file.read (lod, size);
        file.close();
        cout<<"Loaded"<<endl;
        for(int i=0; i < size; i++){
            full_text.back().push_back(lod[i]);
            if((lod[i] == '\n' || lod[i] == '.') && i+1 < size){ 
                full_text.push_back(n);
            }
        }
        delete[] lod;
    }else{
        cout<<"Can't open file!"<<endl;
    }
    mute_text.unlock();
}

bool write_to_file(){
    ofstream file;
    file.open(file_path);
    if(file.is_open()){
        mute_text.lock();
        for(int i = 0; i< full_text.size(); i++){
            file<<full_text.at(i);
        }
        mute_text.unlock();
        file<<'\0';
        file.close();
        return true;
    }else{
        cout<<"Can't open file for write!"<<endl;
        return false;
    }
    return false;
}

int main(int argc, char* argv[]){
    if(argc != 4){
        cout<<"Wrong using!"<<endl;
        cout<<"./server /path/to/file port_publicher port_pull"<<endl;
        return 0;
    }
    file_path = argv[1];
    load_from_file();
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
            write_to_file();
            Message m;
            m.task = DISCONNECT;
            send_message(main_pub_socket, m);
            flag = false;
        }
        if(text == "show"){
            for(int i = 0; i< full_text.size(); i++){
                cout<<full_text.at(i);
            }
            cout<<endl;
        }
        if(text == "save"){
            write_to_file();
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
        a.port_pub = port_publisher;
        a.port_push = port_pull
    ;
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
    write_to_file();
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