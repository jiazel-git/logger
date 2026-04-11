#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT        9999
#define BUFFER_SIZE 4096

void handle_client( int client_fd, struct sockaddr_in* client_addr ) {
    char client_ip[ INET_ADDRSTRLEN ];
    inet_ntop( AF_INET, &( client_addr->sin_addr ), client_ip, INET_ADDRSTRLEN );

    printf( "[SERVER] Client connected from %s:%d\n", client_ip, ntohs( client_addr->sin_port ) );
    printf( "[SERVER] Waiting for logs...\n" );

    char    buffer[ BUFFER_SIZE ];
    ssize_t bytes_received = 0;

    while ( 1 ) {
        bytes_received = recv( client_fd, buffer, BUFFER_SIZE - 1, 0 );
        if ( bytes_received <= 0 ) {
            if ( bytes_received == 0 ) {
                printf( "[SERVER] Client disconnected\n" );
            } else {
                printf( "[SERVER] Receive error: %s\n", strerror( errno ) );
            }
            break;
        }

        buffer[ bytes_received ] = '\0';

        fwrite( buffer, 1, bytes_received, stdout );
        fflush( stdout );
    }

    close( client_fd );
}

int main() {
    int server_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( server_fd < 0 ) {
        perror( "[SERVER] Failed to create socket" );
        exit( EXIT_FAILURE );
    }

    int opt = 1;
    if ( setsockopt( server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) ) < 0 ) {
        perror( "[SERVER] Failed to set SO_REUSEADDR" );
        close( server_fd );
        exit( EXIT_FAILURE );
    }

    struct sockaddr_in server_addr;
    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons( PORT );

    if ( bind( server_fd, (struct sockaddr*)&server_addr, sizeof( server_addr ) ) < 0 ) {
        perror( "[SERVER] Failed to bind" );
        close( server_fd );
        exit( EXIT_FAILURE );
    }

    if ( listen( server_fd, 5 ) < 0 ) {
        perror( "[SERVER] Failed to listen" );
        close( server_fd );
        exit( EXIT_FAILURE );
    }

    printf( "[SERVER] TCP server started on 127.0.0.1:%d\n", PORT );
    printf( "[SERVER] Waiting for connections...\n" );

    while ( 1 ) {
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof( client_addr );

        int client_fd = accept( server_fd, (struct sockaddr*)&client_addr, &client_len );
        if ( client_fd < 0 ) {
            perror( "[SERVER] Failed to accept connection" );
            continue;
        }

        handle_client( client_fd, &client_addr );
    }

    close( server_fd );
    return 0;
}
