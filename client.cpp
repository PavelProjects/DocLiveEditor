#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cerrno>
#include <string>
#include <sys/ioctl.h>
#include <ncurses.h>
#include "Message.hpp"
#include "zmq.h"
using namespace std;

string user_name;
string first_addr_conn = "tcp://localhost:16776";
string base_addr = "tcp://localhost:";
int time_wait = 10000;
bool i_send = false;
WINDOW *main_wind;

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
    main_wind = initscr(); 
    
    pthread_t pub_thr;
    pthread_create(&pub_thr, NULL, inf_pub_listener, publisher);
    pthread_detach(pub_thr);
    zmq_setsockopt(socket_push, ZMQ_SNDTIMEO, &time_wait, sizeof(int));

    bool flag = true;
    noecho();  
    vector<string> full_text;
    string line;
    full_text.push_back(line);
    int x = 0, y = 0, max_x;
    auto it = full_text.begin();
    int key = 0;
    keypad(main_wind, TRUE);
    while(flag){
        move(y, x);
        key = wgetch(main_wind);
        if(key == KEY_RIGHT || key == KEY_LEFT || key == KEY_UP || key == KEY_DOWN || key == KEY_BACKSPACE){
		    switch(key){
            	case KEY_UP:
		    		if(y-1 >= 0){
                        y--;
                        if(x>=full_text.at(y).length()){
                            x = full_text.at(y).length()-1;
                        }
                    }
		    		break;
		    	case KEY_DOWN:
		    		if(y+1 < full_text.size()){
                        y++;
                        if(x>=full_text.at(y).length() || full_text.at(y).length()-1 >= 0){
                            x = full_text.at(y).length()-1;
                        }
                    }
		    		break;
                case KEY_LEFT:
		    		if(x-1 >= 0) x--;
		    		break;
		    	case KEY_RIGHT:
                    if(full_text.at(y).back() == 10){
		    		    if(x+1 <= max_x && full_text.at(y).length()-1 >= x+1) x++;
                    }else{
                        if(x+1 <= max_x && full_text.at(y).length() >= x+1) x++;
                    }
		    		break;
                case KEY_BACKSPACE:
                    if(x-1 >= 0){
                        x--;
                        full_text.at(y).erase(full_text.at(y).begin() + x);
                    }else if(x == 0){
                        if(y-1 >= 0){
                            full_text.erase(full_text.begin()+y);
                            y--;
                            x = full_text.at(y).length()-1;
                        }
                    }
                    break;                 
            }
        }else{
            if(key == '\n'){
                string n;
                if(x == full_text.at(y).length()-1){
                    x = 0;
                    if(y == full_text.size()-1){
                        full_text.push_back(n);
                    }else{
                        full_text.insert(full_text.begin()+y, n);
                    }
                }else{
                    n = full_text.at(y).substr(0, x);
                    full_text.at(y) = full_text.at(y).substr(x, full_text.at(y).length());
                    n.push_back(key);
                    full_text.insert(full_text.begin() + y, n);
                    x = 0;
                }
                y++;
            }else{
                if(x >= full_text.at(y).length()-1){
                    if(full_text.at(y).back() != 10){
                        full_text.at(y).push_back(key);
                    }else{
                        full_text.at(y).insert(full_text.at(y).end() - 1, key);
                    }
                }else{
                    full_text.at(y).insert(full_text.at(y).begin() + x, key);
                }
                x++;
            }            
        }
        wclear(main_wind);
        for(int i=0; i < full_text.size(); i++){
            addstr(full_text.at(i).c_str());
        }
        mvprintw(10, 10, "[%d,%d], [%d %d]",x , y, full_text.at(y).length(), full_text.at(y).back());
        
    }
    getch();
    endwin();  

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
    memset(mes.from , 0, CHAR_LEN);
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
    //cout<<"Start publisher listener"<<endl;
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
        wclear(main_wind);
        waddstr(main_wind, text.c_str());
    }
    return NULL;
}
