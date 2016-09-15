#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <zconf.h>
#include <curl/curl.h>
#include <map>
#include <algorithm>

using namespace std;

string siteData;

map<string, string> headers;

size_t writeCallback(char * buf, size_t size, size_t nmemb, void * up){

    for (int c = 0; c<size*nmemb; c++)
    {
        siteData.push_back(buf[c]);
    }
    return size*nmemb; //tell curl how many bytes we handled

}

size_t headerCallback(char * buffer, size_t size, size_t nitems, void * userdata){

    string headerData;

    for (int c = 0; c<size*nitems; c++)
    {
        headerData.push_back(buffer[c]);
    }

    int sepIndex = headerData.find(':');

    cout << "Header Being PRocessed: " << headerData << endl;
    cout << "Index of Colon: " << sepIndex << endl;

    string headerSub = headerData.substr(0, sepIndex);
    string valueSub = headerData.substr(sepIndex + 1, headerData.length());

    string cleanValue = "";
    //trim the value up
    for(int i = 0; i < valueSub.length(); ++i){

        char c = valueSub[i];


        if(valueSub[i] == ' ' || valueSub[i] == '\n' || valueSub[i] == '\r'){
            continue;
        }else{
            cleanValue += valueSub[i];
        }
    }

    cout << "Header SubString: >" << headerSub << "<" << endl;
    cout << "Value SubString: >" << valueSub << "<" << endl;
    cout << "Clean SubString: >" << cleanValue << "<" << endl;

    headers.insert(pair<string, string>(headerSub, cleanValue));

    return size*nitems; //tell curl how many bytes we handled
}

int main(int argc, char *argv[]){


    cout << "Creating Socket" << endl;

    int socketDescriptor;
    if((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Failed To Create Socket");
        exit(1);
    }

    cout << "Setting Socket Options" << endl;

    int arg = 1;
    if (setsockopt (socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1){
        perror("Setting Of Socket Option Failed");
        exit(1);
    }

    cout << "Main - Binding Socket" << endl;

    struct	sockaddr_in server;
    //bind the socket
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketDescriptor, (struct sockaddr *)&(server), sizeof(server)) == -1)
    {
        perror("Can't bind name to socket");
        exit(1);
    }else{
        cout << "Main - Port Binding Complete" << endl;
    }


    //set socket to start listening
    listen(socketDescriptor, 10);


    while(1){

        struct  sockaddr_in client;
        int socketSessionDescriptor;
        socklen_t client_len= sizeof(client);
        if((socketSessionDescriptor = accept(socketDescriptor, (struct sockaddr *)&client,&client_len)) == -1){
            cout << " ERROR - Can't Accept Client Connection Request" << endl;
            exit(1);
        }


        const int BUFFERSIZE = 4096;
        char inbuf[BUFFERSIZE];

        long bytesRead = read(socketSessionDescriptor, inbuf, BUFFERSIZE - 1);

        if(bytesRead == 0){
            cout << "There Are 0 Bytes To Be Read ?" << endl;
        }

        string data(inbuf);
        cout << " *** NEW INCOMING REQUEST ***" << endl;
        cout << "BYTES READ: " << bytesRead << endl;
        cout << "Received Data: " << endl;
        cout << ">" << data << "<" << endl;


        CURL * curl;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();

        curl_easy_setopt(curl, CURLOPT_URL, "https://facebook.com/");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &headerCallback);

        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "User-Agent: Mozilla/5.0 (X11; Fedora; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/53.0.2785.101 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        curl_easy_perform(curl);

        cout << "CURL RESPONSE DATA" << endl;
        cout << endl << siteData << endl;

        cout << "CURL HEADER DATA" << endl;
        for_each(headers.begin(), headers.end(), [](std::pair<const string,string> mappingEntry){
            cout << "--------" << endl;
            cout << "Header: >" << mappingEntry.first << "<" << endl;
            cout << "Value: >" << mappingEntry.second << "<" << endl;
            cout << "--------" << endl;
        });

        curl_easy_cleanup(curl);
        curl_global_cleanup();

        if(headers.count("Location")){
            cout << "Found Location Header Present!" << endl;
            string redirectLocation = headers["Location"];
            cout << "Location Header: >" << redirectLocation << "<" << endl;

            siteData.clear();
            headers.clear();

            CURL * curl2;
            curl_global_init(CURL_GLOBAL_ALL);
            curl2 = curl_easy_init();

            curl_easy_setopt(curl2, CURLOPT_URL, redirectLocation.c_str());
            curl_easy_setopt(curl2, CURLOPT_WRITEFUNCTION, &writeCallback);
            //curl_easy_setopt(curl2, CURLOPT_HEADER, 1);
            //curl_easy_setopt(curl2, CURLOPT_HEADERFUNCTION, &headerCallback);
            curl_easy_setopt(curl2, CURLOPT_VERBOSE, 1L);

            struct curl_slist *chunk = NULL;
            chunk = curl_slist_append(chunk, "User-Agent: Mozilla/5.0 (X11; Fedora; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/53.0.2785.101 Safari/537.36");
            curl_easy_setopt(curl2, CURLOPT_HTTPHEADER, chunk);

            curl_easy_perform(curl2);

            cout << "CURL REDIRECT RESPONSE DATA" << endl;
            cout << endl << siteData << endl;

            curl_easy_cleanup(curl2);
            curl_global_cleanup();

        }


        cout << "DONE WITH CURL" << endl;

        char * buf;
        char * body;


        buf = "HTTP/1.1 200 OK\r\n";
        send(socketSessionDescriptor, buf, strlen(buf), 0);

        buf = "Server: eoPanel Server\r\n";
        send(socketSessionDescriptor, buf, strlen(buf), 0);

        buf = "Content-Type: text/html; charset=ISO-8859-1\r\n";
        send(socketSessionDescriptor, buf, strlen(buf), 0);

        string contLength;
        contLength = "Content-Length: " + to_string(siteData.length()) + "\r\n";
        send(socketSessionDescriptor, contLength.c_str(), contLength.length(), 0);

        buf = "\r\n";
        send(socketSessionDescriptor, buf, strlen(buf), 0);



        //body = "<h2> Weclome </h2>";
        send(socketSessionDescriptor, siteData.c_str(), siteData.length(), 0);


        //send(socketSessionDescriptor, siteData.c_str(), siteData.length(), 0);



        shutdown(socketSessionDescriptor, SHUT_RDWR);
        close(socketSessionDescriptor);

    }
}
