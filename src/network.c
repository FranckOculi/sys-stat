#include "system.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>

#define SOCKET_FAMILY AF_UNIX
#define MAX_CONNECTION_REQUEST 1
#define BUFFER_SIZE 1024
#define SOCKET_NAME "/tmp/sysstat.sock"

int connection_socket = -1;
int connected_socket = -1;
volatile int stop = 0;

static void (*cleanup_callback)(void) = NULL;

static void handle_signal(int sig_number) {
    (void)sig_number;
    stop = 1;

    if (connected_socket != -1) {
        print_log(stdout, "(Server) Caught user request to close the connection.\n");

        /* To force stopping recv. */
        shutdown(connected_socket, SHUT_RDWR);
        close(connected_socket);
        connected_socket = -1;
    }

    if (cleanup_callback) {
        cleanup_callback();
    }
}

int run(void close_log_file()) {
    cleanup_callback = close_log_file;

    signal(SIGTERM, handle_signal);

    /* Create local socket.  */
    connection_socket = socket(SOCKET_FAMILY, SOCK_STREAM, 0);
    if (connection_socket == -1) {
        print_log(stderr, "");
        perror("(Server) socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un socket_name;
    int socket_name_size = sizeof(socket_name);

    /*
    * For portability clear the whole structure, since some implementations have 
    * additional (nonstandard) fields in the structure.
    */
    memset(&socket_name, 0, socket_name_size);
    
    /* Bind socket to socket name.  */
    socket_name.sun_family = SOCKET_FAMILY;
    strncpy(socket_name.sun_path, SOCKET_NAME, sizeof(socket_name.sun_path) - 1);

    int bind_return_code = bind(connection_socket, (const struct sockaddr*) &socket_name, socket_name_size);
    if (bind_return_code == -1) {
        print_log(stderr, "");
        perror("(Server) bind");
        exit(EXIT_FAILURE);
    }

    /*
    * Prepare for accepting connections. The backlog size is set to 2.  
    * So while one request is being processed other request can be waiting.
    */
    if(listen(connection_socket, MAX_CONNECTION_REQUEST) == -1) {
        print_log(stderr, "");
        perror("(Server) listen");
        exit(EXIT_FAILURE);
    };

    print_log(stdout, "Waiting for new connection...\n");

    /**
     * fcntl() is used to manipulate a file descriptor.
     * F_GETFL retrieve the current file status flags on the socket.
     * 0_NONBLOCK is used to set socket to non-blocking mode.
     * 
     * Calls such as accept(), recv() and send() will no longer block
     * if no connection or data is available.
     */
    int flag = fcntl(connection_socket, F_GETFL, 0);
    fcntl(connection_socket, F_SETFL, flag | O_NONBLOCK);
    fd_set readfds;
    struct timeval tv;

    /* This is the main loop for handling connections.  */
    for(;;) {
        if (stop) break;

        /*
        * We are using select() witch allows a program to monitor multiple file 
        * descriptors, waiting until one or more of the file descriptors become "ready".
        * 
        * select() can monitor only file descriptors numbers that are less than 
        * FD_SETSIZE (1024). Use poll() instead, if needed, witch do not suffer 
        * this limitation.
        * 
        * select() is used to wait for activity on the socket with a timeout. 
        * This allows the main loop to periodically regain control and check
        * whether a termination signal has been received.
        * 
        * Without select(), the program would remain blocked in accept() or recv()
        * functions preventing handle_signal() to stopping the server.
        */
        FD_ZERO(&readfds);
        FD_SET(connection_socket, &readfds);
        tv.tv_sec = 1;  /* 1 second timout to check stop variable. */
        tv.tv_usec = 0;
        int ready_fds = select(connection_socket + 1, &readfds, NULL, NULL, &tv);
        if (ready_fds == -1) {
            if (stop) break;
            print_log(stderr, "");
            perror("(Server) select");
            exit(EXIT_FAILURE);
        } else if (ready_fds == 0) {
            /* Timeout expired. We need to check stop variable. */
            continue;
        }

        /* 
        * At this point -> connected_selected = number of file descriptors contained in
        * the three returned descriptor sets (that is, the total number of bits that are
        * set in readfds, writefds, exceptfds).
        *
        * We can test if a file descriptor is still present in a set.
        */
        if (FD_ISSET(connection_socket, &readfds)) {
            connected_socket = accept(connection_socket, NULL, NULL);
            if (connected_socket == -1) {
                if (stop) break;
                print_log(stderr, "");
                perror("(Server) accept");
                continue;
            }
    
            char rec_buffer[BUFFER_SIZE];
            struct system_stats system_stats = {0};
            int received_bytes;

            while((received_bytes = recv(connected_socket, rec_buffer, BUFFER_SIZE - 1, 0)) > 0) {
                /* Ensure buffer is 0-terminated.  */
                rec_buffer[received_bytes] = '\0';
                print_log(stdout, "(Server) message received : %s\n", rec_buffer);

                /* Retrieve system stats. */
                if ((system_infos(&system_stats)) != 0) {
                    print_log(stderr, "");
                    perror("(Server) system_infos");
                    break;
                };

                /* Send serialized system stats. */
                if ((send(connected_socket, &system_stats, sizeof(system_stats), 0)) == -1) {
                    print_log(stderr, "");
                    perror("(Server) send");
                    break;
                }
            }

            if (received_bytes == -1) {
                if (stop) break;
                print_log(stderr, "");
                perror("(Server) recv");
            } else if (received_bytes == 0) {
                print_log(stdout, "(Server) client disconnected.\n");
                close(connected_socket);
                connected_socket = -1;
            }
        }
        fflush(stdout);
    }

    close(connected_socket);
    close(connection_socket);

    /* Unlink the socket.  */
    unlink(SOCKET_NAME);

    return EXIT_SUCCESS;
}