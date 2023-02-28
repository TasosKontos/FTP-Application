#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <arpa/inet.h>
#include <vector>

using namespace std;

void perror_exit(char *message);

int main(int argc, char *argv[]) {
    int port, sock, i, dir_l;
    struct sockaddr_in server;
    struct in_addr address;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    char *server_ip, *directory;

    //get the command line arguments of the program and error check them
    if (argc != 7) {
        cout<<"Wrong argument count!"<<endl;
        exit(1);
    }

    for(int j=1; j<argc; j+=2){
        if(!strcmp(argv[j], "-p")) {
            port = atoi(argv[j + 1]);
            if(port<0){
                cout<<"Invalid port number!"<<endl;
                exit(2);
            }
        }
        else if(!strcmp(argv[j], "-i")) {
            server_ip = (char*)malloc(strlen(argv[j+1]));
            strcpy(server_ip, argv[j+1]);
        }
        else if(!strcmp(argv[j], "-d")) {
            directory = (char*)malloc(strlen(argv[j+1])+1);
            strcpy(directory, argv[j+1]);
            dir_l = strlen(directory)+1;
        }
        else{
            cout<<"Invalid flag!"<<endl;
            exit(3);
        }
    }
    /* Create socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        perror_exit("socket");
    /* Find server address */
    inet_aton(server_ip, &address);
    if ((rem = gethostbyaddr((const char*)&address, sizeof(address), AF_INET)) == NULL) {
        herror("gethostbyaddr"); exit(1);
    }
    server.sin_family = AF_INET;       /* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(port);         /* Server port */
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0)
        perror_exit("connect");

    //write the length of the dir asked in the socket
    dir_l=htonl(dir_l);
    write(sock, &dir_l, sizeof(int));
    //write the name of the dir asked in the socket
    write(sock, directory, dir_l);
    //read the total number of file that need to be created
    int files_to_create;
    read(sock, &files_to_create, sizeof(files_to_create));
    files_to_create= ntohl(files_to_create);
    cout<<"Files to create:"<<files_to_create<<endl;
    uint32_t path_len;
    char* path_buffer;
    char buffer[500];
    for(int file=0; file<files_to_create; file++) {
        //read the length of the file path
        read(sock, &path_len, sizeof(path_len));
        path_len = ntohl(path_len);
        //read the name of the file path
        path_buffer = (char*)malloc(path_len*sizeof(char));
        read(sock, path_buffer, path_len);
        string path(path_buffer);
        //read the number of readable bytes of the file
        int file_bytes;
        read(sock, &file_bytes, sizeof(file_bytes));
        file_bytes=ntohl(file_bytes);
        //create all the directories needed
        std::filesystem::path local_path = std::filesystem::current_path() / path;
        std::filesystem::remove(local_path);
        std::filesystem::create_directories(local_path.parent_path());
        //create the new file
        std::ofstream f(local_path.string());
        cout<<"Path len: "<<path_len<<"  Path: "<<path<<" Readable bytes: "<<file_bytes<<endl;
        //write the contents of the socket into the newly created file
        int bytes_read;
        while (file_bytes>=500) {
            bytes_read=read(sock, buffer, 500);
            f.write(buffer, bytes_read);
            memset(buffer, 0, 500);
            file_bytes-=bytes_read;
        }
        bytes_read = read(sock, buffer, file_bytes);
        file_bytes-=bytes_read;
        f.write(buffer,bytes_read);
        f.close();
        memset(buffer, 0, 500);
        free(path_buffer);
    }
    free(directory);
    free(server_ip);
}

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}
