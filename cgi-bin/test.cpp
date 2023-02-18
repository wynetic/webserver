#include <cstring>
#include <map>
#include <fstream>

using namespace std;


map<string, string> parseQueryString(string qstring) {
    map<string, string> dict;
    char key[256], value[256];
    int k = 1, v = 0, j = 0, len = qstring.length();
    for (int i = 0; i <= len; i++) {
        if (qstring[i] == '&' || i == len) {
            value[j] = '\0';
            dict.insert(pair<string, string>(string(key), string(value)));
            bzero(key, sizeof(key));
            bzero(value, sizeof(value));
            k = 1, v = 0, j = 0;
            continue;
        }

        if (qstring[i] == '=') {
            key[j] = '\0';
            k = 0, v = 1, j = 0;
            continue;
        }

        if (k == 1)
            key[j++] = qstring[i];
        else if (v == 1)
            value[j++] = qstring[i];
    }

    return dict;
}

int main(int argc, char* argv[]) {
    char* qs = getenv("QUERY_STRING");
    
    map<string, string> dict = parseQueryString(qs);
    ofstream file("tmp/cgi_response.html");
    if (file.is_open()) {
        file << "<!DOCTYPE html>\r\n" << "<html lang=\"en\">\r\n" << "<head>\r\n";
        file << "<meta charset=\"UTF-8\">\r\n" << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n";
        file << "</head>\r\n" << "<body>\r\n";

        /* Here is a result of CGI */
        file << "Hello! My name is " << dict.find("name")->second << " and I'm " << dict.find("age")->second << "\r\n";
        /* ********************** */

        file << "</body>\r\n" << "</html>\r\n";
    }

    return 0;
}