#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <zconf.h>
#include <curl/curl.h>
#include <map>
#include <algorithm>


#include <netinet/tcp.h>
#include <arpa/inet.h>
//#include <linux/ip.h>



#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)

//#include <errno.h>

using namespace std;

char * buffer = new char[65536];

string siteData;
map<string, string> headers;


/*
    96 bit (12 bytes) pseudo header needed for tcp header checksum calculation
*/
struct pseudo_header
{
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

unsigned short in_cksum(unsigned short *ptr, int nbytes)
{
    register long		sum;		/* assumes long == 32 bits */
    u_short			oddbyte;
    register u_short	answer;		/* assumes u_short == 16 bits */

    /*
     * Our algorithm is simple, using a 32-bit accumulator (sum),
     * we add sequential 16-bit words to it, and at the end, fold back
     * all the carry bits from the top 16 bits into the lower 16 bits.
     */

    sum = 0;
    while (nbytes > 1)  {
        sum += *ptr++;
        nbytes -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nbytes == 1) {
        oddbyte = 0;		/* make sure top half is zero */
        *((u_char *) &oddbyte) = *(u_char *)ptr;   /* one byte only */
        sum += oddbyte;
    }

    /*
     * Add back carry outs from top 16 bits to low 16 bits.
     */

    sum  = (sum >> 16) + (sum & 0xffff);	/* add high-16 to low-16 */
    sum += (sum >> 16);			/* add carry */
    answer = ~sum;		/* ones-complement, then truncate to 16 bits */
    return(answer);
}

/*
    Generic checksum calculation function
*/
unsigned short csum(unsigned short *ptr,int nbytes)
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }

    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;

    return(answer);
}

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

void sendPacket(string sourceAddress, string destinationAddress, int sourcePort, int destPort, unsigned int sequence, unsigned int ack, const char * payloadData){
    cout << "Received Source Address Of: " << sourceAddress << endl;
    cout << "Received Destination Address Of: " << destinationAddress << endl;

    int s;
    if((s = socket(AF_INET, SOCK_RAW,  IPPROTO_TCP)) == -1){
        perror("Failed To Create Raw Socket");
        exit(1);
    }

    char datagram[4096];
    char source_ip[32];
    char *data;
    char *pseudogram;

    memset(datagram, 0, 4096);

    //IP header
    struct iphdr *iph = (struct iphdr *) datagram;

    //TCP header
    struct tcphdr *tcph = (struct tcphdr *) (datagram + sizeof (struct ip));
    struct sockaddr_in sin;
    struct pseudo_header psh;

    //Data part
    data = datagram + sizeof(struct iphdr) + sizeof(struct tcphdr);
    //strcpy(data , "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    strcpy(data , payloadData);

    //some address resolution
    strcpy(source_ip , sourceAddress.c_str()); // source address
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);
    sin.sin_addr.s_addr = inet_addr (destinationAddress.c_str()); //destination address



    //Fill in the IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof (struct iphdr) + sizeof (struct tcphdr) + strlen(data);
    iph->id = htonl (54321); //Id of this packet
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;      //Set to 0 before calculating checksum
    iph->saddr = inet_addr ( source_ip );    //Spoof the source ip address
    iph->daddr = sin.sin_addr.s_addr;

    //Ip checksum
    iph->check = csum ((unsigned short *) datagram, iph->tot_len);

    //TCP Header
    tcph->source = htons (sourcePort);
    tcph->dest = htons (destPort);
    tcph->seq = htonl(sequence);
    tcph->ack_seq = htonl(ack);
    tcph->doff = 5;  //tcp header size
    tcph->fin=0;
    tcph->syn=0;
    tcph->rst=0;
    tcph->psh=1;
    tcph->ack=1;
    tcph->urg=0;
    tcph->window = htons (5840); /* maximum allowed window size */
    tcph->check = 0; //leave checksum 0 now, filled later by pseudo header
    tcph->urg_ptr = 0;

    //Now the TCP checksum
    psh.source_address = inet_addr( source_ip );
    psh.dest_address = sin.sin_addr.s_addr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + strlen(data) );

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + strlen(data);
    pseudogram = (char *)malloc(psize);

    memcpy(pseudogram , (char*) &psh , sizeof (struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header) , tcph , sizeof(struct tcphdr) + strlen(data));

    //IP_HDRINCL to tell the kernel that headers are included in the packet
    int one = 1;
    const int *val = &one;

    if (setsockopt (s, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) < 0)
    {
        perror("Error setting IP_HDRINCL");
        exit(0);
    }

    //Send the packet
    if (sendto (s, datagram, iph->tot_len ,  0, (struct sockaddr *) &sin, sizeof (sin)) < 0)
    {
        perror("sendto failed");
    }
        //Data send successfully
    else
    {
        printf ("Packet Send. Length : %d \n" , iph->tot_len);
    }

    close(s);

}

int main(int argc, char *argv[]){


    cout << "Creating Socket" << endl;

    int socketDescriptor;
    if((socketDescriptor = socket(AF_INET, SOCK_STREAM,  0)) == -1){
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

    cout << "Now Listening" << endl;
    //set socket to start listening
    listen(socketDescriptor, 10);


    cout << "Now Setting Up Raw Socket To Listen" << endl;

    //the connection has been accepted, now get a raw socket going to start listening
    int rawSocketDescriptor;
    if((rawSocketDescriptor = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) == -1){
        perror("Failed To Create Raw Socket");
        exit(1);
    }


    struct iphdr *iph;
    struct tcphdr *tcph;
    char * payload;
    long payloadlen;


    struct  sockaddr_in client;
    int socketSessionDescriptor;
    socklen_t client_len= sizeof(client);
    if((socketSessionDescriptor = accept(socketDescriptor, (struct sockaddr *)&client,&client_len)) == -1){
        cout << " ERROR - Can't Accept Client Connection Request" << endl;
        exit(1);
    }

    cout << "Connecting Was Accepted" << endl;



    while(1) {


        long bytesRead = read(rawSocketDescriptor, buffer, 9999);

        if (bytesRead == 0) {
            cout << "There Are 0 Bytes To Be Read ?" << endl;
        }

        //get ip header data
        iph = (struct iphdr*)buffer;
        int iphdrlen = (iph->ihl*4);
        tcph = (struct tcphdr*)(buffer + iphdrlen);
        int tcphdrlen = (tcph->doff*4);

        payload = (buffer + iphdrlen + tcphdrlen);
        payloadlen = bytesRead - iphdrlen - tcphdrlen;

        //i want the initial push packet
        if (ntohs(tcph->th_dport) == 8080 && tcph->psh == 1) {
            cout << "FOUND A MATCH" << endl;
            cout << "RAW: " << tcph->th_dport << endl;
            cout << ntohs(tcph->th_dport) << endl;
            cout << tcph->psh << endl;
            break;
        } else {

            //cout << "DEST IS NOT 8080. ITS NOT OUR PACKET" << endl;
            //cout << "RAW: " << recv_pkt.tcp.th_dport << endl;
            //cout << ntohs(recv_pkt.tcp.th_dport) << endl;
            continue;
        }
    }

   /* cout << " Cycling Payload" << endl;
    cout << "Payload Length: " << payloadlen << endl;
    for(int i = 0 ; i < payloadlen; ++i){
        cout << "Character PRinting" << endl;
        printf(">%c<", payload[i]);
    }
*/
    string data(payload, payloadlen);
    cout << " *** NEW INCOMING REQUEST ***" << endl;
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
        curl_easy_setopt(curl2, CURLOPT_HEADER, 1);
        curl_easy_setopt(curl2, CURLOPT_HEADERFUNCTION, &headerCallback);
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
    cout << "Site Data Length: " << siteData.length() << " bytes" << endl;

    char * buf;
    char * body;

    char recvDest[INET_ADDRSTRLEN];
    char recvSrc[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &iph->daddr, recvDest, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &iph->saddr, recvSrc, INET_ADDRSTRLEN);

    unsigned int ack = ntohl(tcph->ack_seq);
    unsigned int seq = ntohl(tcph->seq);

    cout << "Received Src: " << recvSrc << endl;
    cout << "Received Dest: " << recvDest << endl;
    cout << "Received SrcPort: " << ntohs(tcph->th_sport) << endl;
    cout << "Received DestPort: " << ntohs(tcph->th_dport) << endl;
    cout << "Received Sequence Number: " << seq << endl;
    cout << "Received Acknowledgement Number: " << ack << endl;

    string http = "HTTP/1.1 200 OK\r\nServer: eoPanel Server\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: 18\r\n\r\n<h2>Welcome</h2>";

    sendPacket(recvDest, recvSrc, ntohs(tcph->th_dport), ntohs(tcph->th_sport), ack, seq + 1, http.c_str());


    close(socketSessionDescriptor);



    /*buf = "HTTP/1.1 200 OK\r\n";
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
    close(socketSessionDescriptor);*/

}
