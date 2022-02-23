/**
 * Author: Steven Butz
 */
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<time.h>
#include<netdb.h>

char passcode[32];

struct sockaddr_in server;
struct sockaddr_in client_addr;
int listen_socket;

typedef struct client_t {
  struct sockaddr_in address;
  char username[32];
} client_t;

client_t *clients[10];

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_client(client_t *client) {
  pthread_mutex_lock(&client_mutex);
  for (int i = 0; i < 10; i++) {
    if (!clients[i]) {
      clients[i] = client;
      break;
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

void remove_client(client_t *client) {
  pthread_mutex_lock(&client_mutex);
  for (int i = 0; i < 10; i++) {
    if (clients[i]) {
      if (clients[i] == client) {
        clients[i] = NULL;
        break;
      }
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

void broadcast(char *message, client_t *sender) {
  char buffer[2000];
  memset(buffer, '\0', sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s", message);

  printf("%s", buffer);
  pthread_mutex_lock(&client_mutex);
  for (int i = 0; i < 10; i++) {
    if (clients[i]) {
      if (clients[i] != sender) {
        //sendto(listen_socket, buffer, strlen(buffer), 0, (sockaddr*)&(clients[i]->address), sizeof(sockaddr_in));
        if (sendto(listen_socket, buffer, strlen(buffer), 0, (sockaddr*)&(clients[i]->address), sizeof(sockaddr_in)) < 0) {
          printf("Error sending to client %i\n", i);
          break;
        }
      }
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

void send_msg(char *message, client_t *sender) {
  char buffer[2000];
  memset(buffer, '\0', sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s: %s", sender->username, message);

  printf("%s", buffer);
  pthread_mutex_lock(&client_mutex);
  for (int i = 0; i < 10; i++) {
    if (clients[i]) {
      if (clients[i] != sender) {
        //sendto(listen_socket, buffer, strlen(buffer), 0, (sockaddr*)&(clients[i]->address), sizeof(sockaddr_in));
        if (sendto(listen_socket, buffer, strlen(buffer), 0, (sockaddr*)&(clients[i]->address), sizeof(sockaddr_in)) < 0) {
          printf("Error sending to client %i\n", i);
          break;
        }
      }
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

char *strip_newline(char stripped[]) {
  for (unsigned int i = 0; i < strlen(stripped); i++) {
    if (stripped[i] == '\n') {
      stripped[i] = '\0';
      return stripped;
    }
  }
  return stripped;
}

void *receive_input(void *) {
  char input[200];
  char output[2000] = "";
  while(1) {
    fgets(input, sizeof(input), stdin);
    if (strcmp(input, "listclients\n") == 0) {
      pthread_mutex_lock(&client_mutex);
      for (int i = 0; i < 10; i++) {
        if (clients[i]) {
          strcat(output, clients[i]->username);
          strcat(output, " ");
        }
      }
      pthread_mutex_unlock(&client_mutex);
      output[strlen(output)-1] = '\n';
      printf("%s", output);
      memset(output, '\0', sizeof(output));
    }
    memset(input, '\0', sizeof(input));
  }
}

int main(int argc, char *argv[]) {
  if (argc != 6) {
    printf("Command should be of the format: ./chatserver_udp -start -port <port> -passcode <passcode>\n");
    return 1;
  }
  if (strcmp(argv[1], "-start") == 0 && strcmp(argv[2], "-port") == 0 && strcmp(argv[4], "-passcode") == 0) {
    strcpy(passcode, argv[5]);
  } else {
    printf("Command should be of the format: ./chatserver_udp -start -port <port> -passcode <passcode>\n");
    return 1;
  }

	// Create listener socket
	listen_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (listen_socket == -1) {
		printf("Could not create socket\n");
    return 1;
	}
  server.sin_family = AF_INET;
  char host[] = "127.0.0.1";
  server.sin_addr.s_addr = inet_addr(host);
  server.sin_port = htons(atoi(argv[3]));
  if (bind(listen_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
    printf("Bind failed\n");
    return 1;
  }
  printf("Server started on port %hu. Accepting connections\n", ntohs(server.sin_port));

  pthread_t input_thread;
  if (pthread_create(&input_thread, NULL, receive_input, NULL) != 0) {
    printf("Thread could not be created\n");
    return 1;
  }

  char buffer[2000], msg[2000];
  socklen_t addr_len = sizeof(sockaddr_in);
  sockaddr_in s;
  int new_client, clientnum;
  while (1) {
    memset(msg, '\0', sizeof(msg));
    memset(buffer, '\0', sizeof(buffer));
    recvfrom(listen_socket, buffer, sizeof(buffer), 0, (sockaddr*)&s, &addr_len);

    // Check if client is new or has already sent messages
    new_client = 1;
    clientnum = -1;
    for (int i = 0; i < 10; i++) {
      if (clients[i]) {
        if (clients[i]->address.sin_addr.s_addr == s.sin_addr.s_addr && clients[i]->address.sin_port == s.sin_port) {
          new_client = 0;
          clientnum = i;
          break;
        }
      }
    }
    if (new_client) {
      // Check passcode
      if (strcmp(passcode, buffer) != 0) {
        sprintf(msg, "Incorrect passcode\n");
        sendto(listen_socket, msg, strlen(msg), 0, (sockaddr*)&s, addr_len);
      } else {
        memset(buffer, '\0', sizeof(buffer));
        struct client_t* client = (struct client_t*) malloc(sizeof(struct client_t));
        client->address = s;
        add_client(client);
        //sprintf(msg, "Connected to %s on port %hu\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
        snprintf(msg, sizeof(msg), "Connected to %s on port %hu\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
        sendto(listen_socket, msg, strlen(msg), 0, (sockaddr*)&s, addr_len);
      }
    } else {
      client_t *client = clients[clientnum];

      // Check if client hasn't sent username yet
      if (strcmp(client->username, "") == 0) {
        strlcpy(client->username, buffer, sizeof(client->username));
        //strncpy(client->username, buffer, 32);
        memset(msg, '\0', sizeof(msg));
        char temp[] = " joined the chatroom\n";
        snprintf(msg, sizeof(msg), "%s%s", client->username, temp);
        broadcast(msg, client);
        memset(msg, '\0', sizeof(msg));
        continue;
      }

      //todo: check if right size or need extra char for \0?
      char stripped[strlen(buffer)];
      memset(stripped, '\0', sizeof(stripped));
      //todo: replace with strlcpy()
      strncpy(stripped, buffer, strlen(buffer));
      strip_newline(stripped);
      if (strcmp(stripped, ":Exit") == 0) {
        char temp[] = " left the chatroom\n";
        snprintf(msg, sizeof(msg), "%s%s", client->username, temp);
        broadcast(msg, client);
        remove_client(client);
        free(client);
        continue;
      } else if (strcmp(stripped, ":)") == 0) {
        memset(buffer, '\0', sizeof(buffer));
        char temp[] = "[feeling happy]\n";
        snprintf(buffer, sizeof(buffer), "%s", temp);
      } else if (strcmp(stripped, ":(") == 0) {
        memset(buffer, '\0', sizeof(buffer));
        char temp[] = "[feeling sad]\n";
        snprintf(buffer, sizeof(buffer), "%s", temp);
      } else if (strcmp(stripped, ":mytime") == 0) {
        memset(buffer, '\0', sizeof(buffer));
        time_t t = time(NULL);
        snprintf(buffer, sizeof(buffer), "%s", ctime(&t));
      } else if (strcmp(stripped, ":+1hr") == 0) {
        memset(buffer, '\0', sizeof(buffer));
        time_t t = time(NULL) + 3600;
        snprintf(buffer, sizeof(buffer), "%s", ctime(&t));
      }
      send_msg(buffer, client);
      memset(buffer, '\0', sizeof(buffer));
    }
  }
  return 0;
}
