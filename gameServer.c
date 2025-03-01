#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>

#define MAX_PLAYERS 100

typedef struct {
    char* message;
    size_t length;
} Message;

typedef struct {
    Message messages[100];  // queue of messages to send to a client
    int message_count;      // number of messages in the queue
} MessageQueue;

int client_sockets[MAX_PLAYERS] = {0};    // active client sockets; index is (player_id - 1)
MessageQueue message_queues[MAX_PLAYERS]; // message queues for each client

fd_set read_fds, write_fds;  // master fd sets for reading and writing

int target_number;           // the number to be guessed
int cur_active = 0;          // current number of active players

// Signal handler to gracefully shut down the server
void handle_signal(int signal) {
    printf("\nServer shutting down...\n");
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (client_sockets[i] != 0) {
            close(client_sockets[i]);
        }
        for (int j = 0; j < message_queues[i].message_count; j++) {
            free(message_queues[i].messages[j].message);
        }
    }
    exit(EXIT_SUCCESS);
}

// Add a message to the message queue for player with id 'player_id' (0-indexed)
void add_message_to_queue(int player_id, const char* msg) {
    if (player_id < 0 || player_id >= MAX_PLAYERS)
        return;
    int idx = message_queues[player_id].message_count;
    message_queues[player_id].messages[idx].message = strdup(msg);
    message_queues[player_id].messages[idx].length = strlen(msg);
    message_queues[player_id].message_count++;
}

// Remove the first message in the queue for a given player
void remove_message_from_queue(int player_id) {
    if (player_id < 0 || player_id >= MAX_PLAYERS || message_queues[player_id].message_count == 0)
        return;
    free(message_queues[player_id].messages[0].message);
    for (int i = 1; i < message_queues[player_id].message_count; i++) {
        message_queues[player_id].messages[i - 1] = message_queues[player_id].messages[i];
    }
    message_queues[player_id].message_count--;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./server <port> <seed> <max-number-of-players>\n");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int seed = atoi(argv[2]);
    int max_players = atoi(argv[3]);

    if (port <= 0 || port > 65535 || max_players < 2 || max_players > MAX_PLAYERS) {
        fprintf(stderr, "Usage: ./server <port> <seed> <max-number-of-players>\n");
        exit(EXIT_FAILURE);
    }

    srand(seed);
    target_number = rand() % 100 + 1;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, max_players) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_signal);

    // Initialize master fd sets
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    // (We do not always add the server_socket to read_fds here.
    // It will be added in the main loop only when cur_active < max_players.)

    int max_sd = server_socket;

    // Initialize message queues for each player slot.
    for (int i = 0; i < MAX_PLAYERS; i++) {
        message_queues[i].message_count = 0;
    }

    while (1) {
        // Create temporary fd sets for select.
        fd_set temp_read_fds, temp_write_fds;
        FD_ZERO(&temp_read_fds);
        FD_ZERO(&temp_write_fds);

        // Add the server (welcome) socket only if there is room.
        if (cur_active < max_players) {
            FD_SET(server_socket, &temp_read_fds);
        }

        // Add each active client socket.
        for (int i = 0; i < max_players; i++) {
            if (client_sockets[i] != 0) {
                FD_SET(client_sockets[i], &temp_read_fds);
                // Only add to write set if there are pending messages.
                if (message_queues[i].message_count > 0)
                    FD_SET(client_sockets[i], &temp_write_fds);
            }
        }

        int activity = select(max_sd + 1, &temp_read_fds, &temp_write_fds, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            break;
        }

        // 1. Check the welcome (listening) socket.
        if (cur_active < max_players && FD_ISSET(server_socket, &temp_read_fds)) {
            printf("Server is ready to read from welcome socket %d\n", server_socket);

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket < 0) {
                perror("Accept failed");
                continue;
            }

            int player_id = -1;
            for (int i = 0; i < max_players; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    FD_SET(new_socket, &read_fds);  // add to master read set
                    player_id = i;
                    if (new_socket > max_sd)
                        max_sd = new_socket;
                    break;
                }
            }
            if (player_id == -1) {
                // Should not happen because we only accept when cur_active < max_players.
                const char *msg = "Server is full, try again later.\n";
                send(new_socket, msg, strlen(msg), 0);
                close(new_socket);
                continue;
            }
            cur_active++;

            // Send welcome message to the new player.
            char welcome_msg[128];
            snprintf(welcome_msg, sizeof(welcome_msg), "Welcome to the game, your id is %d\n", player_id + 1);
            add_message_to_queue(player_id, welcome_msg);

            // Notify all other players.
            char join_msg[128];
            snprintf(join_msg, sizeof(join_msg), "Player %d joined the game\n", player_id + 1);
            for (int i = 0; i < max_players; i++) {
                if (client_sockets[i] != 0 && i != player_id) {
                    add_message_to_queue(i, join_msg);
                }
            }
        }

        // 2. Check all active client sockets for reading.
        for (int i = max_players - 1; i >= 0; i--) {
            int sd = client_sockets[i];
            if (sd != 0 && FD_ISSET(sd, &temp_read_fds)) {
                printf("Server is ready to read from player %d on socket %d\n", i + 1, sd);
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                int bytes_read = read(sd, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) {
                    // Client disconnected.
                    close(sd);
                    FD_CLR(sd, &read_fds);
                    client_sockets[i] = 0;
                    cur_active--;

                    char disconnect_msg[128];
                    snprintf(disconnect_msg, sizeof(disconnect_msg), "Player %d disconnected\n", i + 1);
                    for (int j = 0; j < max_players; j++) {
                        if (client_sockets[j] != 0) {
                            add_message_to_queue(j, disconnect_msg);
                        }
                    }
                    continue;
                }

                // Process the guess.
                int guess = atoi(buffer);
                char guess_msg[128];
                snprintf(guess_msg, sizeof(guess_msg), "Player %d guessed %d\n", i + 1, guess);
                // Queue guess message for all active players.
                for (int j = 0; j < max_players; j++) {
                    if (client_sockets[j] != 0) {
                        add_message_to_queue(j, guess_msg);
                    }
                }

                if (guess != target_number) {
                    const char *response_msg = (guess > target_number) ?
                                               "The guess in too high\n" : "The guess in too low\n";
                    for (int j = 0; j < max_players; j++) {
                        if (client_sockets[j] != 0) {
                            add_message_to_queue(j, response_msg);
                        }
                    }
                } else {
                    // Correct guess: queue win messages to all players.
                    char win_msg[128], correct_msg[128];
                    snprintf(win_msg, sizeof(win_msg), "Player %d wins\n", i + 1);
                    snprintf(correct_msg, sizeof(correct_msg), "The correct guessing is %d\n", target_number);
                    for (int j = 0; j < max_players; j++) {
                        if (client_sockets[j] != 0) {
                            add_message_to_queue(j, correct_msg);
                            add_message_to_queue(j, win_msg);
                        }
                    }
                    // (When a client's write handler sends a message that contains "wins",
                    // that client will be closed.)
                    // After all active players disconnect, we generate a new target number.
                }
            }

            if (sd != 0 && message_queues[i].message_count > 0 && FD_ISSET(sd, &temp_write_fds)) {
                printf("Server is ready to write to player %d on socket %d\n", i + 1, sd);
                Message *msg = &message_queues[i].messages[0];
                // Write only one line (one message) per select iteration.
                send(sd, msg->message, msg->length, 0);
                // If this message indicates a win, then close the client.
                int close_after_write = (strstr(msg->message, "wins") != NULL);
                remove_message_from_queue(i);
                if (close_after_write) {
                    close(sd);
                    FD_CLR(sd, &read_fds);
                    FD_CLR(sd, &write_fds);
                    client_sockets[i] = 0;
                    cur_active--;
                }
            }
        }

        // 4. Check if the game round ended (i.e. all clients closed after a win).
        // When cur_active becomes 0 after a winning guess, reset the game.
        if (cur_active == 0) {
            // Clear all message queues.
            for (int i = 0; i < MAX_PLAYERS; i++) {
                while (message_queues[i].message_count > 0) {
                    remove_message_from_queue(i);
                }
            }
            // Generate a new target number.
            target_number = rand() % 100 + 1;
            // The server now waits for new players.
        }
    }

    close(server_socket);
    return 0;
}
