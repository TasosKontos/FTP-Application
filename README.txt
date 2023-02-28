Kontos Anastasios // 1115201800080 // System Programming 2nd Project

Compilation & Execution
A makefile is included in the project so the compile command is just: make all

The execution command for the remoteClient and the dataManager of the project are as described in the assignment.
For example an execution can be:
 1) ./dataServer -p 10006 -q 10 -b 140 -s 5
 2) ./remoteClient -p 10006 -i 192.168.1.127 -d /home/kontos/Desktop/test_folder

 Results
 The results of the program (ie the folder that is copied) are in the directory where the remoteClient executable is executed.
 For example the above execution would produce a test_folder in the directory where remoteClient is stored.

 Code resources
 Several parts of the project code are taken from the course slides:
    1) Socket creation and connection, client-server communication on the internet // https://cgi.di.uoa.gr/~mema/courses/k24/lectures/topic5-Sockets.pdf
    2) Producer-consumer problem implementation, queue-mutexes-cond_variables-synchronization , (obtain, place functions) // https://cgi.di.uoa.gr/~mema/courses/k24/lectures/topic6-Threads.pdf
    3) Taking the ip from command line and transforming it to usefull information // https://cgi.di.uoa.gr/~antoulas/k24/lectures/l11.pdf

Program functionality

    >dataServer
        -Handles command line arguments
        -Starts all the worker threads
        -Creates socket
        while(1)
           -Accepts connection
           -Starts a communication thread for the accepted client

           (communication thread)
                -Reads the length of the asked directory
                -Reads the name of the asked directory
                -Calculates the number of files in the asked directory
                -Writes the previously calculated number in the client socket
                -Pushes all the files paths from the asked directory into the queue

           (worker thread)
                -Pops a file path from the queue
                -Writes the lenght of the path into the client socket
                -Writes the path name into the client socket
                -Writes the number of readable bytes of the file
                -Writes block by block the contents of the file

    >remoteClient
                -Handles command line arguments
                -Creates socket and connects to server
                -Writes the lenght of the asked directory in the socket
                -Writes the name of the asked directory in the socket
                -Reads the total number of files that need to be created

                (for every file)
                     -Reads the length of the file path
                     -Reads the name of the file path
                     -Reads the number of readable bytes in the file
                     -Creates all the needed directories for the files
                     -Creates the new file
                     -Reads from the socket and writes its contents to the newly created file

