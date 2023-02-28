#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <filesystem>
#include <string>
#include <iostream>
#include <queue>
#include <pthread.h>
#include <cstring>
#include <unordered_map>
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace std;


queue<pair<string, int>> files_to_transfer; //queue that holds the paths of the files that need to be transfered and maps them to the socket descriptor of the client
pthread_mutex_t queue_mtx; //mutex for the synchronization of the queue
//cond variables for the synchronization of the queue
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
int max_queue_capacity;

unordered_map<int, pthread_mutex_t*> socket_mtxs; //maps the socket descriptor to a mutex so that only 1 worker can a access the same socket at a time
unordered_map<int, string> socket_folder; //maps socket descriptors to the file that the specific client asked to transfer at the beggining of the program
int block_size;

string createClientDir(string, string); //customizes the path placed in the client socket so that the cwd of the server is removed from it
void place(string, int); //places a file into the files_to_transfer queue while keeping the application correctly synchronized
pair<string, int> obtain();//pop a file from the files_to_transfer queue while keeping the application correctly synchronized
void perror_exit(char *message);
void *workerThread(void*); //function for the worker threads
void *communicationThread(void*); //function for the communication thread

int main(int argc, char *argv[]) {
    int port, sock, thread_pool_size;
    struct sockaddr_in server;
    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr_in client;


    //get the command line arguments of the program and error check them
    if (argc != 9) {
        cout<<"Wrong argument count!"<<endl;
        exit(1);
    }
    for(int i=1; i<argc; i+=2){
        if(!strcmp(argv[i], "-p")) {
            port = atoi(argv[i + 1]);
            if(port<0){
                cout<<"Invalid port number!"<<endl;
                exit(2);
            }
        }
        else if(!strcmp(argv[i], "-s")) {
            thread_pool_size = atoi(argv[i + 1]);
            if(thread_pool_size<=0){
                cout<<"Invalid thread pool size!"<<endl;
                exit(3);
            }
        }
        else if(!strcmp(argv[i], "-q")) {
            max_queue_capacity = atoi(argv[i + 1]);
            if(max_queue_capacity<=0){
                cout<<"Invalid max queue capacity"<<endl;
                exit(4);
            }
        }
        else if(!strcmp(argv[i], "-b")) {
            block_size = atoi(argv[i + 1]);
            if(block_size<=0){
                cout<<"Invalid block size!"<<endl;
                exit(5);
            }
        }
        else{
            cout<<"Invalid flag!"<<endl;
            exit(6);
        }
    }
    //initialize mutex and cond variables
    pthread_mutex_init(&queue_mtx,0);
    pthread_cond_init(&cond_nonempty,0);
    pthread_cond_init(&cond_nonfull,0);

    //start worker threads
    pthread_t threads[thread_pool_size];
    for(int i=0; i<thread_pool_size; i++)
        pthread_create(&threads[i], NULL, workerThread, NULL);

    /* Create socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror_exit("socket");
    server.sin_family = AF_INET; /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port); /* The given port */
    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0)
        perror_exit("bind");
    /* Listen for connections */
    if (listen(sock, 5) < 0) perror_exit("listen");
    printf("Listening for connections to port %d\n", port);

    while (1) {
        struct sockaddr *clientptr=(struct sockaddr *)&client;
        socklen_t clientlen;
        struct hostent *rem;
        clientlen = sizeof(client);
        int newsock;
    /* accept connection */
        if ((newsock = accept(sock, clientptr, &clientlen))
            < 0) perror_exit("accept");
    /* Find client's name */
        if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr,
                                 sizeof(client.sin_addr.s_addr), client.sin_family))
            == NULL) {
            herror("gethostbyaddr"); exit(1);}
        printf("Accepted connection from %s\n", rem->h_name);

        socket_mtxs.insert({newsock, (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t))});
        pthread_mutex_init(socket_mtxs.at(newsock), NULL);
        //initiate communication thread
        pthread_t com_t;
        pthread_create(&com_t ,NULL , communicationThread, &newsock);
        pthread_detach(com_t);
    }
}
void *communicationThread(void* socket) {
    int socket_d =*(int*)socket;
    int dir_length;
    //read the length of the asked directory
    if(read(socket_d, &dir_length, sizeof(int))<0)
        perror_exit("read dir length @comThread");
    dir_length=ntohl(dir_length);

    //read the name of the asked directory
    char* dir_name = (char*)malloc(dir_length);
    if(read(socket_d, dir_name, dir_length)<0)
        perror_exit("read dir name @comThread");

    //insert socket descriptor and dir name into map
    string dir(dir_name);
    socket_folder.insert({socket_d, dir});
    int counter=0;

    //count the number of files in the asked directory
    error_code ec;
    for(const auto & entry : filesystem::recursive_directory_iterator(dir)) {
        if(filesystem::is_regular_file(entry.path(), ec))
            counter++;
    }

    //send the number of files in the asked directory
    counter = htonl(counter);
    if(write(socket_d, &counter, sizeof(int))<0)
        perror_exit("write file count @comThread");

    //place the file paths into the queue
    for(const auto & entry : filesystem::recursive_directory_iterator(dir))
        if(filesystem::is_regular_file(entry.path(), ec))
            place(entry.path().string(), socket_d);

    free(dir_name);
    pthread_exit(NULL);
}

void *workerThread(void* ptr){
    while(1){
        //pop the first file of the queue
        pair<string, int> pair_res = obtain();
        //get the final form of the directory to send to the client
        string final_str = createClientDir(pair_res.first, socket_folder.at(pair_res.second));
        uint32_t path_len = htonl(final_str.size()+1);

        //lock the socket mutex to send to client while keeping the application synchronized
        pthread_mutex_lock(socket_mtxs.at(pair_res.second));
        //write the lenght of the path
        write(pair_res.second, &path_len, sizeof(path_len));
        //write the name of the path
        write(pair_res.second, final_str.c_str(), final_str.size()+1);
        //open the file to read
        int fd = open(pair_res.first.c_str(), O_RDONLY);
        //get the readable bytes of the file
        int readable_bytes;
        if(ioctl(fd, FIONREAD, &readable_bytes)<0){
            perror_exit("ioctl @workerthread");
            exit(1);
        }
        cout<<"Path len: "<<final_str.size()<<"  Path str:" <<final_str<<  "Readable bytes at server: "<<readable_bytes<<endl;
        uint32_t r_b=htonl(readable_bytes);
        //write the number of readable bytes to the socket
        write(pair_res.second, &r_b, sizeof(r_b));
        //parse the file and write to the socket - block size bytes at a time
        int bytes_read;
        char buffer[block_size];
        while((bytes_read=read(fd, buffer, block_size))==block_size)
            write(pair_res.second, buffer, bytes_read);
        write(pair_res.second, buffer, bytes_read);
        pthread_mutex_unlock(socket_mtxs.at(pair_res.second));
    }
}

void place(string dir, int socket){
    pthread_mutex_lock(&queue_mtx);
    while(files_to_transfer.size()>=max_queue_capacity)
        pthread_cond_wait(&cond_nonfull, &queue_mtx);

    files_to_transfer.push({dir, socket});
    pthread_mutex_unlock(&queue_mtx);
    pthread_cond_signal(&cond_nonempty);
}

pair<string, int> obtain(){
    pthread_mutex_lock(&queue_mtx);
    while(files_to_transfer.empty())
        pthread_cond_wait(&cond_nonempty, &queue_mtx);
    pair<string, int> ret_val = files_to_transfer.front();
    files_to_transfer.pop();
    pthread_mutex_unlock(&queue_mtx);
    pthread_cond_signal(&cond_nonfull);
    return ret_val;
}

string createClientDir(string whole, string to_remove){
    string folder_to_copy;
    if(to_remove.find_last_of('/') != string::npos){
        folder_to_copy = to_remove.substr(to_remove.find_last_of('/')+1);
    }
    else{
        folder_to_copy = to_remove;
    }
    int pos = whole.find(to_remove);
    string return_string = whole;
    if(pos!=string::npos)
        return_string.erase(pos, to_remove.length());

    return folder_to_copy+return_string;
}

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}