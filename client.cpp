#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cerrno>
#include <string>
#include <sys/ioctl.h>
#include <net/if.h> 
#include <netinet/in.h>
#include "Message.hpp"
#include "zmq.h"
using namespace std;

string user_name;
string first_addr_conn = "tcp://localhost:16776";
string base_addr = "tcp://localhost:";
int time_wait = 10000;
bool i_send = false;

void* context = zmq_ctx_new();
void* publisher = zmq_socket(context, ZMQ_SUB);
void* socket_push = zmq_socket(context, ZMQ_PUSH);

bool connect(void*);
void send_message(void*, Message);
void send_changes(void*, string*);
Message recv_message(void* );
string update_text(int , void*);

void* inf_pub_listener(void*);
void* publisher_listener(void*);

int main(int argc, char const *argv[]){
    cout<<"You name :: ";
    cin>>user_name;
    if(!connect(context)){
        cout<<"Can't connect to server"<<endl;
        return 0;
    }
    pthread_t pub_thr;
    pthread_create(&pub_thr, NULL, inf_pub_listener, publisher);
    pthread_detach(pub_thr);

    bool flag = true;
    string text;
    while(flag){
        getline(cin, text);
        if(text == "!/exit"){
            flag = false;
        }else{
            i_send = true;
            send_changes(socket_push, &text);
        }
    }

    zmq_close(publisher);
    zmq_close(socket_push);
    zmq_ctx_destroy(context);
    return 0;
}

bool connect(void* context){
    void* first_connect = zmq_socket(context, ZMQ_REQ);
    if(0>zmq_connect(first_connect, first_addr_conn.c_str())){
        cout<<"First connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_setsockopt(first_connect, ZMQ_RCVTIMEO, &time_wait, sizeof(int));

    OnStartMessage r, *a;
    zmq_msg_t ans, req;
    memcpy(r.name, user_name.c_str(), user_name.length());
    zmq_msg_init_size(&req, sizeof(OnStartMessage));
    memcpy(zmq_msg_data(&req), &r, sizeof(OnStartMessage));
    zmq_msg_send(&req, first_connect, 0);
    zmq_msg_close(&req);

    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, first_connect, 0);
    a = (OnStartMessage*) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    zmq_close(first_connect);
    if(a->port_pub < 0 || a->port_push < 0){
        return false;
    }
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
    return true;
}
void send_message(void* socket, Message mes){
    zmq_msg_t req;
    memcpy(mes.from, user_name.c_str(), user_name.length());
    zmq_msg_init_size(&req, sizeof(Message));
    memcpy(zmq_msg_data(&req), &mes, sizeof(Message));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
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

void* inf_pub_listener(void* args){
    pthread_t th;
    cout<<"Start publisher listener"<<endl;
    while(true){
        pthread_create(&th, NULL, publisher_listener, args);
        pthread_join(th, NULL);
    }
    return NULL;
}
void* publisher_listener(void* args){
    void* socket = (void*) args;
    Message m = recv_message(socket);
    if(m.task == 1){ 
        string text = update_text(m.length, socket);
        if(!i_send){
            cout<<"New text from server:"<<endl<<text<<endl;
        }else{
            cout<<"Commited"<<endl;
            i_send = false;
        }
    }
    return NULL;
}
