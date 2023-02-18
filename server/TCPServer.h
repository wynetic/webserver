#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <netinet/in.h>
#include <string>

class TCPServer {
    public:
        TCPServer(int port); // constructor
        ~TCPServer(); // destructor
        int startListen();

    private:
        char buffer[1024]; // request buffer
        fd_set master_set, working_set;
        int max_sd; // max socket descriptor
        int server_port;
        int master_socket;
        int client_socket;
        int bytes_recv; // bytes recieved from client
        int bytes_sent; // bytes sent to client
        struct sockaddr_in s_addr; // server address
        
        int startServer();
        void socketSettingUp();
        int acceptConnection();
        int getRequest(int c_socket);

        int sendNotFound(int c_socket);
        int sendNotImplemented(int c_socket);

        int sendTextFile(int c_socket, std::string URI, std::string ct);
        int sendBinaryFile(int c_socket, std::string URI, std::string ct);
        int sendResponse(int c_socket);

        void closeConnection(int c_socket);
        void endServer();
};

#endif