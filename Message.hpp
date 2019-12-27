#include <string>
#define CHAR_LEN 5

class Message{
    public:
        int port;
        int task;
        int status;
        char data[CHAR_LEN];
        int length;
        Message(){};
};


