#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cerrno>
#include <string>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <mutex>
#include "Message.hpp"
#include "zmq.h"
#include "MyText.hpp"
using namespace std;
#define ctrl(x)           ((x) & 0x1f)
#define QUIT_CTRL "ctrl+x :: quit"
#define SAVE_CTRL "ctrl+o :: save copy"
#define BLACK_ON_WHITE 1
#define RED_ON_WHITE 2
#define offset 2

string user_ip;
char file_name[20];
string first_port_conn = ":16776";
string base_addr = "tcp://";
string server_addr;
int time_wait = 5000;
int push_port = 0;
double sent_time_wait = 1;
WINDOW *main_wind;
mutex queue_mute;
vector<Message> queue;
mutex mute_send_flag;
bool send_working = false;
mutex mute_text;
bool i_send = false, saved = false;

void* context = zmq_ctx_new();
void* publisher = zmq_socket(context, ZMQ_SUB);
void* socket_push = zmq_socket(context, ZMQ_PUSH);

bool connect(void*);
void send_changes(void*, string*);
void send_msg(void*, Message);
void send_start_msg(void* socket, OnStartMessage r);
Message recv_message(void* );
OnStartMessage recv_start_message(void*);
int size, length;
bool first_start = true, main_loop = true, safe_mode = false;
Text full_text;

void* inf_pub_listener(void*);
void* publisher_listener(void*);
void* send(void*);
void* send_from_queue(void*);
void add_to_queue(Message);
void add_changes(Message m);

int main(int argc, char const *argv[]){
    // while(main_loop){
        // cout<<"Server address :: ";
        // getline(cin, server_addr);
        // cout<<"You ip :: ";
        // getline(cin, user_ip);
        // cout<<"Turn safe mode?(low speed)y/n :: ";
        // char c;
        // cin>>c;
        // if(server_addr.length() > 0 && user_ip.length() >0){
            // main_loop = false;
        // }else{
            // cout<<"Wrong input!"<<endl;
        // }
        // if(c == 'y'){
            // safe_mode = true;
            // cout<<"Safe mode on"<<endl;
        // }else{
            // safe_mode = false;
            // cout<<"Safe mode off"<<endl;
        // }
    // }
    server_addr = user_ip = "localhost"; safe_mode = false;
    if(!connect(context)){
        cout<<"Can't connect to server"<<endl;
        return 0;
    }

    pthread_t pub_thr;
    pthread_create(&pub_thr, NULL, inf_pub_listener, publisher);
    pthread_detach(pub_thr);
    
    main_wind = initscr();
    start_color();
    init_pair(BLACK_ON_WHITE, COLOR_BLACK, COLOR_WHITE); 
    init_pair(RED_ON_WHITE, COLOR_RED, COLOR_WHITE);
    noecho();  
    Message m;
    string nl;
    m.wch = 0;
    int x = 0, y = 0, max_x = 0, row, column;
    getmaxyx(main_wind, row, column);
    int key = 0;
    keypad(main_wind, TRUE);
    main_loop = true; 
    while(main_loop){
        wclear(main_wind);
        for(int i=0; i<full_text.size(); i++){
            mvwprintw(main_wind, i, offset, "%s", full_text.at_st(i).c_str());
        }
        for(int i=0; i<full_text.size(); i++) mvwprintw(main_wind, i, 0, "%d|", i+1);
        wrefresh(main_wind);
        attron(COLOR_PAIR(BLACK_ON_WHITE));
        mvwprintw(main_wind, row-1, column/2-strlen(QUIT_CTRL), QUIT_CTRL);
        mvwprintw(main_wind, row-1, column-strlen(SAVE_CTRL), SAVE_CTRL);
        mvwprintw(main_wind, row-1, 0, "%s", file_name);
        if(saved){
            mvwprintw(main_wind, row-1, strlen(file_name)+1, "saved");
        }else{
            mvwprintw(main_wind, row-1, strlen(file_name)+1, "not saved");
        }
        attroff(COLOR_PAIR(BLACK_ON_WHITE));
        //mvwprintw(main_wind, row-1, 0, "[%d %d] %d %d", x, y, full_text.at_st(y).length(), full_text.size());
        move(y, x + offset);
        key = wgetch(main_wind);
        if(key == KEY_RIGHT || key == KEY_LEFT || key == KEY_UP || key == KEY_DOWN || key == KEY_BACKSPACE){
		    switch(key){
            	case KEY_UP:
		    		if(y-1 >= 0){
                        y--;
                        if(x>=full_text.at_st(y).length()){
                            x = full_text.at_st(y).length()-1;
                        }
                    }
		    		break;
		    	case KEY_DOWN:
		    		if(y+1 < full_text.size()){
                        y++;
                        if(x>=full_text.at_st(y).length()){
                            if(full_text.at_st(y).back() == '\n'){
                                x = full_text.at_st(y).length()-1;
                            }else{
                                x = full_text.at_st(y).length();
                            }
                        }
                    }
		    		break;
                case KEY_LEFT:
		    		if(x-1 >= 0) x--;
		    		break;
		    	case KEY_RIGHT:
                    if(full_text.at_st(y).back() == '\n'){
                        if(x+1 <= full_text.at_st(y).length()-1) x++;
                    }else{
                        if(x+1 <= full_text.at_st(y).length()) x++;
                    }
		    		break;
                case KEY_BACKSPACE:
                    saved = false;
                    mute_text.lock();
                    m.where_x = x;
                    m.where_y = y;
                    if(full_text.at_st(y).back() == '\n'){
                        if(full_text.at_st(y).length() > 1){
                            if(x == 0){
                                if(y != 0){
                                    full_text.append_with_prev(y);
                                    y--;
                                    x=full_text.at_st(y).length()-1;
                                }
                            }else{
                                full_text.erase(y, x);
                                if(x>0) x--;
                            }
                        }else{
                            full_text.erase(y, x);
                            if(y != 0){
                                full_text.erase(y);
                                y--;
                                if(full_text.at_st(y).back() == '\n'){
                                    x = full_text.at_st(y).length()-1;
                                }else{
                                    x = full_text.at_st(y).length();
                                }
                            }else{
                                if(x > 0)x--;
                            }
                        }
                    }else{
                        if(x == full_text.at_st(y).length()){
                            if(full_text.at_st(y).length() >= 1){
                                full_text.pop_back_c(y);
                                x--;
                            }else if(full_text.at_st(y).length() == 0){
                                if(y != 0){
                                    full_text.erase(y);
                                    y--;
                                    if(full_text.at_st(y).back() == '\n'){
                                        x = full_text.at_st(y).length()-1;
                                    }else{
                                        x = full_text.at_st(y).length();
                                    }
                                }else{
                                    x = 0;
                                }
                            }
                        }else if(x >= 0){
                            if(x == 0){
                                if(y != 0){
                                    full_text.append_with_prev(y);
                                    y--;
                                    x=full_text.at_st(y).length();
                                }
                            }else{
                                full_text.erase(y, x-1);
                                x--;
                            }
                        }
                    }
                    mute_text.unlock();
                    m.task = UPDATE_TEXT;
                    m.wch = DELETE_CH;
                    add_to_queue( m);
                    i_send = true;
                    break;                 
            }
        }else if(key == ctrl('x')){
            m.task = DISCONNECT;
            void* socket_push = zmq_socket(context, ZMQ_PUSH);
            if(0>zmq_connect(socket_push, (base_addr+server_addr+":"+to_string(push_port)).c_str())){
                cout<<strerror(errno)<<endl;
            }
            send_msg(socket_push, m);
            zmq_close(socket_push);
            wclear(main_wind);
            mvwprintw(main_wind,row/2-1, column/2-strlen("Press any button"),"Press any button");
            main_loop = false;
        }else if(key == ctrl('o')){
            string fn(file_name);
            full_text.writeToFile(fn.append("#copy"));
            saved = true;
        }else{
            mute_text.lock();
            m.where_x = x;
            m.where_y = y;
            m.change = key;
            m.wch = INSERT_CH;
            m.task = UPDATE_TEXT;
            add_to_queue(m);
            saved = false;
            full_text.insert(y, x, key);
            if(key == '\n'){
                y++;
                x = 0;
            } else{
                x++;
            }
            mute_text.unlock();
            i_send = true;  
        }
    }
    getch();
    endwin();
    cout<<"Exit"<<endl;
    exit(1);
    zmq_close(publisher);
    zmq_ctx_destroy(context);
    return 0;
}

bool connect(void* context){
    void* first_connect = zmq_socket(context, ZMQ_REQ);
    if(0>zmq_connect(first_connect, (base_addr+server_addr+first_port_conn).c_str())){
        cout<<"First connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_setsockopt(first_connect, ZMQ_RCVTIMEO, &time_wait, sizeof(int));

    OnStartMessage r, a;
    memcpy(r.name, user_ip.c_str(), user_ip.size());
    send_start_msg(first_connect, r);
    a = recv_start_message(first_connect);

    if(a.port_pub <= 0 || a.port_push <= 0){
        return false;
    }
    cout<<"pub="<<a.port_pub<<"::"<<"push="<<a.port_push<<endl;
    if(0>zmq_connect(publisher, (base_addr+server_addr+":"+to_string(a.port_pub)).c_str())){
        cout<<"Publisher connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_setsockopt(publisher, ZMQ_SUBSCRIBE, "", 0);
    push_port = a.port_push;
    if(0>zmq_connect(socket_push, (base_addr+server_addr+":"+to_string(push_port)).c_str())){
        cout<<"Push connect :: "<<strerror(errno)<<endl;
        exit(1);
    }
    zmq_close(socket_push);
    zmq_close(first_connect);
    cout<<"Connected"<<endl;

    void* socket_pill = zmq_socket(context, ZMQ_PULL);
    if(0>zmq_bind(socket_pill, "tcp://*:2020")){
        cout<<"Pull connect :: "<<strerror(errno)<<endl;
        exit(1);
    }else{
        Message m = recv_message(socket_pill);
        if(m.task == FULL_TEXT){
            memcpy(file_name, m.data, strlen(m.data));
            cout<<"Loading file "<<file_name<<endl;
            int size = m.size, length;
            for(int i=0; i<size ; i++){
                m = recv_message(socket_pill);
                length = m.size / CHAR_LEN + 1;
                string n;
                for(int j=0; j<length; j++){
                    m = recv_message(socket_pill);
                    n.append(m.data);
                    if(m.change == 'e'){
                        full_text.push_back(n);
                        n.clear();
                    }
                }
                if(full_text.size() > size) full_text.pop_back_string();
            }
        }else{
            cout<<"Loading text refused by server"<<endl;
        }
        cout<<"Done"<<endl;
    }
    zmq_close(socket_pill);

    sleep(1);
    return true;
}


Message recv_message(void* socket){
    zmq_msg_t ans;
    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, socket, 0);
    Message * res = (Message *) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    return (*res);
}
void send_msg(void* socket, Message r){
    zmq_msg_t req;
    memcpy(r.from, user_ip.c_str(), user_ip.size());
    zmq_msg_init_size(&req, sizeof(Message));
    memcpy(zmq_msg_data(&req), &r, sizeof(Message));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}
OnStartMessage recv_start_message(void* socket){
    zmq_msg_t ans;
    zmq_msg_init(&ans);
    zmq_msg_recv(&ans, socket, 0);
    OnStartMessage * res = (OnStartMessage *) zmq_msg_data(&ans);
    zmq_msg_close(&ans);
    return (*res);
}
void send_start_msg(void* socket, OnStartMessage r){
     zmq_msg_t req;
    memcpy(r.name, user_ip.c_str(), user_ip.size());
    zmq_msg_init_size(&req, sizeof(OnStartMessage));
    memcpy(zmq_msg_data(&req), &r, sizeof(OnStartMessage));
    zmq_msg_send(&req, socket, 0);
    zmq_msg_close(&req);
}

void add_changes(Message m){
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
    wclear(main_wind);
    for(int i=0; i<full_text.size(); i++){
        mvwprintw(main_wind, i, offset, "%s", full_text.at_st(i).c_str());
    }
    for(int i=0; i<full_text.size(); i++) mvwprintw(main_wind, i, 0, "%d|", i+1);
    wrefresh(main_wind);
    mute_text.unlock();
}

void add_to_queue(Message mes){
    queue_mute.lock();
    queue.push_back(mes);
    queue_mute.unlock();
    mute_send_flag.lock();
    if(!send_working){
        pthread_t pth;
        pthread_create(&pth, NULL, send_from_queue, NULL);
        pthread_detach(pth);
        send_working = true;
    }
    mute_send_flag.unlock();
}
void* send_from_queue(void* args){
    bool flag = true;
    int status = -1, retr = 20;
    while(flag){
        queue_mute.lock();
        Message m = queue.front();
        queue_mute.unlock();
        pid_t pid = fork();
        if(pid == 0){
            void* context = zmq_ctx_new();
            void* socket_push = zmq_socket(context, ZMQ_PUSH);
            if(0>zmq_connect(socket_push, (base_addr+server_addr+":"+to_string(push_port)).c_str())){
                cout<<strerror(errno)<<endl;
            }
            zmq_msg_t req;
            memcpy(m.from, user_ip.c_str(), user_ip.size());
            zmq_msg_init_size(&req, sizeof(Message));
            memcpy(zmq_msg_data(&req), &m, sizeof(Message));
            zmq_msg_send(&req, socket_push, 0);
            zmq_msg_close(&req);
            zmq_close(socket_push);
            zmq_ctx_destroy(context);
            exit(0);
        }else{
            if(safe_mode){
                status = waitpid(pid, 0, WNOHANG);
                if(status == 0){
                    sleep(sent_time_wait);
                    status = waitpid(pid, 0, WNOHANG);
                    if(status == 0){
                        kill(pid, SIGKILL);
                    }
                }
            }else{
                status = 1;
            }
        }

        if(status != 0 ){
            queue_mute.lock();
            queue.erase(queue.begin());
            queue_mute.unlock();
            mute_send_flag.lock();
            i_send = true;
            mute_send_flag.unlock();
            retr = 20;
        }else{
            retr--;
        }
        queue_mute.lock();
        if(queue.size() == 0){
            flag = false;
        }
        queue_mute.unlock();
        if(retr == 0){
            flag = false;
            kill(pid, SIGKILL);
            cout<<"Lost connection with server"<<endl;
            exit(1);
        }
    }
    send_working = false;
    return NULL;
}

void* inf_pub_listener(void* args){
    pthread_t th;
    while(true){
        pthread_create(&th, NULL, publisher_listener, args);
        pthread_join(th, NULL);
    }
    return NULL;
}

void* publisher_listener(void* args){
    void* socket = (void*) args;
    Message m = recv_message(socket);

    switch(m.task){
        case DISCONNECT:
            wclear(main_wind);
            attron(COLOR_PAIR(RED_ON_WHITE));
            mvwprintw(main_wind, getmaxy(main_wind)/2-2, getmaxx(main_wind)/2-1-strlen("Server turned off\n"), "Server turned off\n");
            attroff(COLOR_PAIR(RED_ON_WHITE));
            attron(COLOR_PAIR(BLACK_ON_WHITE));
            mvwprintw(main_wind, getmaxy(main_wind)/2, getmaxx(main_wind)/2-1-strlen("Press any button\n"), "Press any button\n");
            attroff(COLOR_PAIR(BLACK_ON_WHITE));
            wrefresh(main_wind);
            main_loop = false;
            break;
        case UPDATE_TEXT:
            if(i_send){
                mute_send_flag.lock();
                i_send = false;
                mute_send_flag.unlock();
            }else{
                add_changes(m);
            }
            break;
    }
    return NULL;
}

