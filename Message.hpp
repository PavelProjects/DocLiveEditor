#include <string>
#define CHAR_LEN 200
#define CONNECT 0
#define DISCONNECT -1
#define UPDATE_TEXT 1
#define ADD_CH 10
#define INSERT_CH 11
#define DELETE_CH 12
#define ADD_EMPTY_LINE 13
#define INSERT_EMPTY_LINE 14
#define SPLIT_LINE_AND_INSERT 15
#define DELETE_LINE 16
#define FULL_TEXT 99


class Message{
    public:
        char from[20] = "";
        int task; 
        char change;
        int wch; 
        int where_x;
        int where_y;
        int size;
        char data[CHAR_LEN];
        Message(){};
        ~Message(){};
};

class OnStartMessage{
    public:
        char name[20] = "";
        int port_pub;
        int status;
        int port_push;

};

typedef struct thread_data{
    void* pull;
    void* pub;
}thread_data;