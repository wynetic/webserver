#include "ParseRequest.h"

#include <iostream>
#include <string>

using namespace std;

namespace rp {
    string getHTTPMethod(string request) {
        return request.substr(0, request.find(" "));
    }

    string getURI(string request) {
        string method = getHTTPMethod(request);
        char URI[256];
        int j = 0;
        for (int i = method.length() + 2; request[i] != ' '; i++) {
            URI[j] = request[i];
            j++;
        }
        URI[j] = '\0';
        return string(URI);
    }

    string getFileExtension(string& file_path) {
        int pos = file_path.find(".");
        if (pos == string::npos) // npos is a max value of std::size_t (returned when function fails)
            return "";
        return file_path.substr(pos + 1, file_path.length());
    }

    bool isCGI(string& path) {
        if (path.find("cgi-bin/") == string::npos)
            return false;
        return true;
    }
    
    string getQueryString(string& URI) {
        int pos = URI.find("?");
        if (pos == string::npos)
            return "";
        return URI.substr(pos + 1, URI.length());
    }

    string rmQueryString(string& URI) { // remove query string from request path
        // char path[256];
        int pos_start = URI.find("/"), pos_end = URI.find("?");
        if (pos_start == string::npos || pos_end == string::npos)
            return "";
        return URI.substr(pos_start + 1, pos_end - pos_start - 1);
    }

    char** execList() {

    }

} // namespace rp
