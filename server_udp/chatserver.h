/**
 * Author: Steven Butz
 */

#pragma once

int main(int argc, char *argv[]);

void *receive_input(void *);

void add_client(client_t *client);
void remove_client(client_t *client);
void send_msg(char *message, client_t *sender);
void broadcast(char *message, client_t *sender);
char *strip_newline(char stripped[]);
