#include "TCPServer.h"
#include "Consts.h" // server settings
#include "../utils/Log.h"
#include "../utils/Settings.h"
#include "ParseRequest.h"
#include "HTTPResponse.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>


using namespace std;

TCPServer::TCPServer(int port) : server_port(port), master_socket() {
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(server_port);
    s_addr.sin_addr.s_addr = INADDR_ANY;

    if (startServer() != 0) {
        Log::log("Server start failed");
    }
}

TCPServer::~TCPServer() {
    endServer();
}


int TCPServer::startServer() {
    // Clear log file
    fstream file;
    file.open("log.txt", ios::out | ios::trunc);
    file.close();
    /* /////////// */

    Log::log("*** Starting the server... ***");
    master_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_socket == -1) {
        Log::log("socket() failed");
        return 1;
    }

    socketSettingUp();

    if (bind(master_socket, (struct sockaddr *) &s_addr, sizeof(s_addr)) == -1) {
        Log::log("bind() failed");
        close(master_socket);
        return 1;
    }
    return 0;
}

void TCPServer::socketSettingUp() {
    const int enabled = 1;
    if (setsockopt(master_socket, SOL_SOCKET,  SO_REUSEADDR, &enabled, sizeof(int)) == -1)
        Log::log("setsockopt() failed");

    if (ioctl(master_socket, FIONBIO, &enabled) == -1)
        Log::log("ioctl() failed");
}

int TCPServer::startListen() {
    Log::log("Listening to the master socket...");
    if (listen(master_socket, 5) == -1) {
        Log::log("listen() failed");
        return 1;
    }

    FD_ZERO(&master_set);
    FD_ZERO(&working_set);

    FD_SET(master_socket, &master_set);
    max_sd = master_socket;
   
    do {
        memcpy(&working_set, &master_set, sizeof(master_set));

        Log::log("Waiting on select()...");
        if (select(max_sd + 1, &working_set, nullptr, nullptr, nullptr) == -1) {
            Log::log("select() failed");
            break;
        }

        for (int i = 0; i <= max_sd; ++i) {
            if (FD_ISSET(i, &working_set)) {

                if (i == master_socket) {
                    Log::log("Master socket is readable");
                    if (acceptConnection() == 1) {
                        endServer();
                    }
                } else {
                    Log::log("Descriptor " + to_string(i) + " is readable");
                    while (true) {
                        if (getRequest(i) != 0) {
                            closeConnection(i);
                            break;
                        }

                        if (sendResponse(i) != 0) {
                            closeConnection(i);
                            break;
                        }
                    }
                } 
            }    
        }         

    } while (true);
    return 0;
}

int TCPServer::acceptConnection() {
    do {
        client_socket = accept(master_socket, nullptr, nullptr);
        if (client_socket < 0) {
            if (errno != EWOULDBLOCK) {
                Log::log("accept() failed");
                return 1;
            }
            break;
        }

        Log::log("New incoming connection: " + to_string(client_socket));
        FD_SET(client_socket, &master_set);
        if (client_socket > max_sd)
            max_sd = client_socket;
    } while (client_socket != -1);
    return 0;
}

void TCPServer::closeConnection(int c_socket) {
    FD_CLR(c_socket, &master_set);
    if (c_socket == max_sd) {
        while (FD_ISSET(max_sd, &master_set) == false)
            max_sd -= 1;
    }
    shutdown(c_socket, 2);
    close(c_socket);
}

int TCPServer::getRequest(int c_socket) {  
    int ret = recv(c_socket, buffer, sizeof(buffer), 0);
    if (ret < 0) {
        Log::log("recv() failed");
        return 1;
    }

    if (ret == 0) {
        Log::log("Connection closed");
        return 1;
    }

    bytes_recv = ret;
    Log::log(to_string(bytes_recv) + " bytes received");
    Log::log(buffer);

    string HTTP_method = rp::getHTTPMethod(string(buffer));
    string URI = rp::getURI(string(buffer));
    return 0;
}

int TCPServer::sendNotFound(int c_socket) {
    HTTPResponse NotFound = HTTPResponse(404, "Closed");
    bytes_sent = send(c_socket, NotFound.getFinalResponse(), strlen(NotFound.getFinalResponse()), 0);
    Log::log(to_string(bytes_sent) + " bytes sent");
    if (bytes_sent != -1) {
        Log::log("Connection closed [404 NOT FOUND]");
        bzero(buffer, sizeof(buffer));
        return 1;
    }
    return 0;
}

int TCPServer::sendNotImplemented(int c_socket) {
    HTTPResponse NotImplemented = HTTPResponse(501, "Closed");
    bytes_sent = send(c_socket, NotImplemented.getFinalResponse(), strlen(NotImplemented.getFinalResponse()), 0);
    Log::log(to_string(bytes_sent) + " bytes sent");
    if (bytes_sent != -1) {
        Log::log("Connection closed [501 NOT IMPLEMENTED]");
        bzero(buffer, sizeof(buffer));
        return 1;
    }
    return 0;
}

int TCPServer::sendTextFile(int c_socket, string URI, string ct) {
    HTTPResponse response = HTTPResponse(200, "Closed", ct);
        
    ifstream in(URI, ifstream::in);

    if (in.is_open()) {
        ostringstream index;
        index << in.rdbuf();
        response.addData(index.str());
    } else {
        if (sendNotFound(c_socket) == 1)
            return 1;
    }
    
    bytes_sent = send(c_socket, response.getFinalResponse(), strlen(response.getFinalResponse()), 0);
    Log::log(to_string(bytes_sent) + " bytes sent");
    if (bytes_sent != -1) {
        Log::log("Connection closed (" + URI + ")");
        bzero(buffer, sizeof(buffer));
        return 1;
    }
    return 0;
}

int TCPServer::sendBinaryFile(int c_socket, string URI, string ct) {
    ifstream in(URI, ios::in | ios::binary | ios::ate);
    
    if (in.is_open()) {
        HTTPResponse response = HTTPResponse(200, "Closed", ct);
        bytes_sent = send(c_socket, response.getFinalResponse(), strlen(response.getFinalResponse()), 0);
        Log::log(to_string(bytes_sent) + " bytes sent");

        streampos size = in.tellg();
        char* file = new char[size];
        in.seekg (0, ios::beg);
        in.read (file, size);
        in.close();

        bytes_sent = send (c_socket, file, size, 0);
        Log::log(to_string(bytes_sent) + " bytes sent");
        if (bytes_sent != -1) {
            Log::log("Connection closed (" + URI + ")");
            bzero(buffer, sizeof(buffer));
            delete file;
            return 1;
        }
    } else {
        if (sendNotFound(c_socket))
            return 1;
    }
    return 0;
}

int TCPServer::sendResponse(int c_socket) {
    string http_method = rp::getHTTPMethod(string(buffer));
    string URI = rp::getURI(string(buffer));
    string con_type = rp::getFileExtension(URI);

    if (http_method.compare("GET") != 0 && http_method.compare("HEAD") != 0) {
        if (sendNotImplemented(c_socket))
            return 1;
    }
    if (rp::isCGI(URI)) {
        string qstring = rp::getQueryString(URI);
        string filename = rp::rmQueryString(URI);
        string path = Consts::CGI_BIN + filename;

        string env = "QUERY_STRING=" + qstring;
        putenv(const_cast<char*> (env.c_str()));

        pid_t pid = fork();
        if (pid == -1) {
            Log::log("fork() failed: CGI Execution failed");
            closeConnection(c_socket);
            return 1;
        }
        if (pid == 0) {
            if (execlp(path.c_str(), path.c_str(), to_string(c_socket).c_str(), nullptr) == -1) {
                Log::log("exec() failed");
            }
        }
        wait(nullptr);
        if (sendTextFile(c_socket, "tmp/cgi_response.html", "html"));
            return 1;
    }

    if (con_type.empty() && URI == "") {
        if (sendTextFile(c_socket, Consts::SERVER_DATA_PATH + Consts::HOME_PAGE, "html"))
            return 1;
    } else if (con_type.compare("html") == 0 ||
        con_type.compare("css") == 0 ||
        con_type.compare("js") == 0 ||
        con_type.compare("txt") == 0) {

        if (sendTextFile(c_socket, Consts::SERVER_DATA_PATH + URI, con_type))
            return 1;
    } else if (con_type.compare("jpeg") == 0 ||
        con_type.compare("jpg") == 0 ||
        con_type.compare("png") == 0 ||
        con_type.compare("tiff") == 0 ||
        con_type.compare("gif") == 0 || 
        con_type.compare("ico") == 0 ||
        con_type.compare("webp") == 0 ||
        con_type.compare("svg") == 0 ||
        con_type.compare("pdf") == 0 ||
        con_type.compare("djvu") == 0 ||
        con_type.compare("json") == 0 ||
        con_type.compare("js") == 0 ||
        con_type.compare("zip") == 0 ||
        con_type.compare("ogg") == 0 ||
        con_type.compare("wav") == 0 || 
        con_type.compare("webm") == 0 ||
        con_type.compare("mp4") == 0 ||
        con_type.compare("mpeg") == 0) {
        
        if (URI == Consts::FAVICON) {
            if (sendBinaryFile(c_socket, Consts::SERVER_DATA_PATH + URI, con_type))
                return 1;
        }

        if (sendBinaryFile(c_socket, Consts::SERVER_DATA_PATH + URI, con_type))
            return 1;
    } else {
        if (sendNotImplemented(c_socket))
            return 1;
    }
    
    bzero(buffer, sizeof(buffer));
    return 0;
}

void TCPServer::endServer() {
    for (int sd = 0; sd <= max_sd; sd++) {
        if (FD_ISSET(sd, &master_set))
            close(sd);
    }
    shutdown(master_socket, 2);
    close(master_socket);
    Log::log("Server shutdown");
    exit(0);
}
