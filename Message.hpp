#include <string>
#define CHAR_LEN 200
#define CONNECT 0
#define DISCONNECT -1
#define UPDATE_TEXT 1
#define FULL_TEXT 10
#define INSERT_CH 11
#define DELETE_CH 12


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