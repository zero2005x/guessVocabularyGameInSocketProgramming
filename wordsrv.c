#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 59185
#endif
#define MAX_QUEUE 5


void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);

/* These are some of the function prototypes that we used in our solution
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */
/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf, struct client *exclude);
void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);

void introduce_player(struct game_state *game, struct client *p);
void move_to_active_players(struct game_state *game, struct client **new_players, struct client *p);
int read_name_from_client(struct game_state *game, struct client **new_players, struct client *p);
int read_move_from_client(struct game_state *game, struct client *p);
int check_curr_turn(struct game_state *game, struct client *p);
int process_move(struct game_state *game, struct client *p);
void check_game_finished(struct game_state *game, struct client *p, char *word_filename);
void disconnect_client(struct game_state *game, struct client *p);


/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;


/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}


int main(int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;

    if(argc != 2){
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }

    // Handle SIGPIPE
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);

    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;

    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new players have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;

    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);

    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if(write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, p->fd);
            }
        }

        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be
         * valid.
         */
        int cur_fd;
        for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if(FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player
                for(p = game.head; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        int err;
                        // read_move returns 0 if full read and no errors
                        if ((err = read_move_from_client(&game, p)) != 0) {
                            if (err == -2 && game.head != NULL) {
                                // it was p's turn to play
                                // there are still players left playing
                                announce_turn(&game);
                            }
                            break;
                        }

                        if (check_curr_turn(&game, p) != 0) {
                            // not client's turn or client disconnected
                            break;
                        }

                        // process_move returns 0 for valid guess and no errors
                        if ((err = process_move(&game, p)) != 0) {
                            if (err == -1) { // player disconnected
                                advance_turn(&game);
                                disconnect_client(&game, p);

                                // if we have players left announce turn
                                if (game.head != NULL) {
                                    announce_turn(&game);
                                }
                            }
                            break;
                        }

                        // Print status message to active players
                        char msg2[MAX_MSG];
                        char *out = status_message(msg2, &game);
                        broadcast(&game, out, NULL);

                        check_game_finished(&game, p, argv[1]);
                        announce_turn(&game);

                        break;
                    }
                }

                // Check if any new players are entering their names
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        // read name returns 0 if full read and valid name
                        if (read_name_from_client(&game, &new_players, p) == 0) {
                            move_to_active_players(&game, &new_players, p);
                            introduce_player(&game, p);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

/* Print disconnection message to users and remove player from
 * game linked list
 */
void disconnect_client(struct game_state *game, struct client *p) {
    fprintf(stderr, "Disconnect from client %s\n", inet_ntoa(p->ipaddr));

    char msg[MAX_MSG];
    sprintf(msg, "%s has disconnected.\n", p->name);
    broadcast(game, msg, p);

    remove_player(&(game->head), p->fd);

    if (game->head == NULL) {
        // if all players have disconnected, reset has_next_turn
        game->has_next_turn = NULL;
    }
}

/* Print outbuf message to all users in game->head linked list
 * Can specify a client to not print to by setting exclude to a non NULL client.
 */
void broadcast(struct game_state *game, char *outbuf, struct client *exclude) {
    struct client *p;
    for(p = game->head; p != NULL; p = p->next) {
        if (exclude == NULL || p != exclude) {
            if (write(p->fd, outbuf, strlen(outbuf)) == -1) {
                fprintf(stderr, "Write to client failed\n");
                if (game->has_next_turn == p) {
                    advance_turn(game);
                }
                disconnect_client(game, p);
            }
        }
    }
}

/* Print who's turn it is to all players except player who's turn it is.
 * Prompt player who's turn it is for a guess.
 */
void announce_turn(struct game_state *game) {
    struct client *p = game->has_next_turn;

    char line[MAX_MSG];
    sprintf(line, "It's %s's turn.\n", p->name);
    broadcast(game, line, p);
    printf("GAME: %s", line);

    char *message = PROMPT_GUESS;
    if (write(p->fd, message, strlen(message)) == -1) {
        fprintf(stderr, "Write to client failed\n");
        advance_turn(game);
        disconnect_client(game, p);
    }
}

/* Notify winner that they won the game and broadcast to all other players
 * who won the game.
 */
void announce_winner(struct game_state *game, struct client *winner) {
    char *msg = "Game Over! You Win!\n";
    if (write(winner->fd, msg, strlen(msg)) == -1) {
        fprintf(stderr, "Write to client failed\n");
        advance_turn(game);
        disconnect_client(game, winner);
    }

    char line[MAX_MSG];
    sprintf(line, "Game Over! %s Won!\n", winner->name);
    broadcast(game, line, winner);
    printf("GAME: %s", line);
}

/* Move the game's has_next_turn attribute one player forward,
 * loop around to the start if we reach the end.
 */
void advance_turn(struct game_state *game) {
    if (game->has_next_turn == NULL || (game->has_next_turn)->next == NULL) {
        game->has_next_turn = game->head;
    } else {
        game->has_next_turn = (game->has_next_turn)->next;
    }
}

/* Notify all players who just joined, print game status to new player.
 * Print who's turn it is in the game, or if p is the first player
 * prompt them to guess.
 */
void introduce_player(struct game_state *game, struct client *p) {
    char msg[MAX_MSG];
    sprintf(msg, "%s has just joined.\n", p->name);
    printf("GAME: %s has joined the game.\n", p->name);
    broadcast(game, msg, NULL);

    char *out = status_message(msg, game);
    if (write(p->fd, out, strlen(out)) == -1) {
        fprintf(stderr, "Write to client failed\n");
        disconnect_client(game, p);
    }

    if (game->has_next_turn != NULL) {
        // Game has active players
        sprintf(msg, "It's %s's turn.\n", game->has_next_turn->name);
        if (write(p->fd, msg, strlen(msg)) == -1) {
            fprintf(stderr, "Write to client failed\n");
            disconnect_client(game, p);
        }
    }

    if (game->head->next == NULL) {
        // p is the first player
        game->has_next_turn = p;
        char *message = PROMPT_GUESS;
        if (write(p->fd, message, strlen(message)) == -1) {
            fprintf(stderr, "Write to client failed\n");
            disconnect_client(game, p);
        }
    }
}

/* Remove player from new_players struct and add it to game.head
 */
void move_to_active_players(struct game_state *game, struct client **new_players, struct client *p) {
    // Remove p from new_players
    if (*new_players == p) {
        *new_players = p->next;
    } else {
        struct client *curr;
        for(curr = *new_players; curr != NULL; curr = curr->next) {
            if (curr->next == p) {
                curr->next = p->next;
            }
        }
    }

    // Add p to active players
    p->next = game->head;
    game->head = p;
}

/* Read from client p, if network newline is found check if the name is valid.
 * Return -1 for read error.
 * Return 1 for incomplete or invalid names.
 * Return 0 for valid name.
 */
int read_name_from_client(struct game_state *game, struct client **new_players, struct client *p) {
    int nbytes;
    if ((nbytes = read(p->fd, p->in_ptr, MAX_BUF - (p->in_ptr - p->inbuf))) <= 0) {
        if (nbytes == -1) { // errno set
            perror("read from client");
        } else {
            fprintf(stderr, "Disconnect from client %s\n", inet_ntoa(p->ipaddr));
        }
        remove_player(new_players, p->fd);
        return -1;
    }

    printf("[%d] Read %d bytes\n", p->fd, nbytes);
    p->in_ptr += nbytes;
    *(p->in_ptr) = '\0';

    if (strstr(p->inbuf, "\r\n") != NULL) {
        // cutoff network newline
        *(p->in_ptr - 2) = '\0';

        printf("[%d] Found newline %s\n", p->fd, p->inbuf);

        // Check if name is already being used
        int valid = 0;
        struct client *curr;
        for(curr = game->head; curr != NULL; curr = curr->next) {
            if (strcmp(curr->name, p->inbuf) == 0) {
                valid = 1;
            }
        }

        // Check if name is invalid
        if (valid == 1 || strlen(p->inbuf) == 0) {
            char *greeting = "Invalid name. What is your name? ";
            if(write(p->fd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(p->ipaddr));
                remove_player(new_players, p->fd);
                return -1;
            }
            return 1;
        }

        // name is valid, so copy it to p->name
        strncpy(p->name, p->inbuf, MAX_NAME - 1);
        p->name[MAX_NAME - 1] = '\0';
        p->in_ptr = p->inbuf;

        return 0;
    }

    return 1;
}

/* Read from client p, check if network newline.
 * Return -1 for read error and it wasn't client's turn.
 * Return -2 for read error and it was client's turn.
 * Return 0 for valid name.
 * Return 1 for incomplete read.
 */
int read_move_from_client(struct game_state *game, struct client *p) {
    int nbytes;
    if ((nbytes = read(p->fd, p->in_ptr, MAX_BUF - (p->in_ptr - p->inbuf))) <= 0) {
        if (nbytes == -1) { // errno set
            perror("read from client");
        } else {
            fprintf(stderr, "Read from client failed\n");
        }
        int ret = -1;
        if (game->has_next_turn == p) {
            ret = -2;
            // need to advance turn before disconnecting
            // so we correctly set the next player
            advance_turn(game);
        }
        disconnect_client(game, p);
        return ret;
    }

    printf("[%d] Read %d bytes\n", p->fd, nbytes);
    p->in_ptr += nbytes;
    *(p->in_ptr) = '\0';

    if (strstr(p->inbuf, "\r\n") != NULL) {
        // hide network newline
        *(p->in_ptr - 2) = '\0';

        printf("[%d] Found newline %s\n", p->fd, p->inbuf);
        return 0;
    }

    return 1;
}

/* Check if it's client p's turn, notify them if it's not.
 * Return -1 if a write error occurred.
 * Return 1 if it's not p's turn.
 * Return 0 if it's p's turn.
 */
int check_curr_turn(struct game_state *game, struct client *p) {
    if (game->has_next_turn != p) {
        printf("GAME: It's not %s's turn!\n", p->name);
        char *message = "It's not your turn to guess.\n";
        if (write(p->fd, message, strlen(message)) == -1) {
            fprintf(stderr, "Write to client failed\n");
            disconnect_client(game, p);
            return -1;
        }

        // Reset p's buffer and end pointer
        p->in_ptr = p->inbuf;
        p->inbuf[0] = '\0';
        return 1;
    }
    return 0;
}

/* Check if player's guess is a valid guess, prompt otherwise.
 * Handle a correct and incorrect guess by updating game status
 * Return -1 for read error and it wasn't client's turn.
 * Return 1 for invalid guess.
 * Return 0 for valid guess.
 */
int process_move(struct game_state *game, struct client *p) {
    char guess = (p->inbuf)[0];

    int len_move = strlen(p->inbuf);

    // Reset p's buffer and end pointer
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';

    // Check if guess is invalid
    if (len_move != 1 || guess < 'a' || guess > 'z' || game->letters_guessed[guess - 'a'] == 1) {
        printf("GAME: %s made an invalid guess\n", p->name);
        char *message = "Invalid guess. Enter a lowercase letter between a and z.\n";
        if (write(p->fd, message, strlen(message)) == -1) {
            fprintf(stderr, "Write to client failed\n");
            return -1;
        }
        message = PROMPT_GUESS;
        if (write(p->fd, message, strlen(message)) == -1) {
            fprintf(stderr, "Write to client failed\n");
            return -1;
        }
        return 1;
    }

    // Check if guess is in word (correct guess)
    if (strchr(game->word, guess) != NULL) {
        printf("GAME: %s made a correct guess!\n", p->name);

        // Update current guess (for example '-o-d')
        int len = strlen(game->word);
        for (int i = 0; i < len; i++) {
            if (game->guess[i] == '-' && game->word[i] == guess) {
                game->guess[i] = guess;
            }
        }
    } else {
        // incorrect guess
        printf("GAME: %s made an incorrect guess\n", p->name);

        char msg[MAX_MSG];
        sprintf(msg, "%c is not in the word.\n", guess);
        if (write(p->fd, msg, strlen(msg)) == -1) {
            fprintf(stderr, "Write to client failed\n");
            return -1;
        }
        game->guesses_left -= 1;
        advance_turn(game);
    }
    game->letters_guessed[guess - 'a'] = 1;

    // Notify players of valid guess
    char msg[MAX_MSG];
    sprintf(msg, "%s guessed: %c\n", p->name, guess);
    broadcast(game, msg, NULL);
    return 0;
}

/* Check if the game has finished, if so, announce winner/game over
 * and start a new game.
 */
void check_game_finished(struct game_state *game, struct client *p, char *word_filename) {
    if (strchr(game->guess, '-') == NULL) {
        // game finished because word was guessed
        announce_winner(game, p);
    } else if (game->guesses_left == 0) {
        char msg[MAX_MSG];
        sprintf(msg, "No guesses left. Game over. The word was %s.\n", game->word);
        broadcast(game, msg, NULL);
    } else {
        // game isn't finished yet
        return;
    }

    printf("GAME: New game.\n");
    init_game(game, word_filename);

    char *new_msg = "\nLet's start a new game.\n";
    broadcast(game, new_msg, NULL);

    char msg3[MAX_MSG];
    char *out = status_message(msg3, game);
    broadcast(game, out, NULL);
}
