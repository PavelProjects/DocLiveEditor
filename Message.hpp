#include <string>
#define CHAR_LEN 100

class Message{
    public:
        char from[20];
        int task;
        int status; //-1-disconnect//0-connect //1-update text //2-client changes
        char data[CHAR_LEN];
        int length;
        Message(){};
        ~Message(){};
};

class OnStartMessage{
    public:
        char name[20];
        int port_pub;
        int port_push;

};