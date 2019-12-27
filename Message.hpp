#include <string>
#define CHAR_LEN 5

class Message{
    public:
        int task;
        int status;
        char data[CHAR_LEN];
        int length;
        Message(){};
};

class OnStartMessage{
    public:
        char addres[20];
        int port_pub;
        int port_push;

};