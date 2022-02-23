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
static int current_id = 0;

struct sockaddr_in server;
struct sockaddr_in client_addr;

typedef struct client_t {
  struct sockaddr_in address;
  int socket_desc;
  int id;
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
      if (clients[i]->id == client->id) {
        clients[i] = NULL;
        break;
      }
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

void broadcast(char *message, int client_id) {
  pthread_mutex_lock(&client_mutex);
  printf("%s", message);
  for (int i = 0; i < 10; i++) {
    if (clients[i]) {
      if (clients[i]->id != client_id) {
        if (send(clients[i]->socket_desc, message, strlen(message), 0) < 0) {
          printf("Error sending to client %i", clients[i]->id);
          break;
        }
      }
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

void send_msg(char *message, client_t *sender) {
  pthread_mutex_lock(&client_mutex);
  char buffer[2000];
  snprintf(buffer, sizeof(buffer), "%s: %s", sender->username, message);
  printf("%s", buffer);
  for (int i = 0; i < 10; i++) {
    if (clients[i]) {
      if (clients[i]->id != sender->id) {
        if (send(clients[i]->socket_desc, buffer, strlen(buffer), 0) < 0) {
          printf("Error sending to client %i", clients[i]->id);
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

void *connection_handler(void *c) {
  client_t *client = (client_t*) c;
  char msg[2000], pw[32], name[32];

  // Receive password from client and validate
  recv(client->socket_desc, pw, 32, 0);
  if (strcmp(passcode, pw) != 0) {
    const char *incorrect = "Incorrect passcode\n";
    send(client->socket_desc, incorrect, strlen(incorrect), 0);
    close(client->socket_desc);
    free(client);
    pthread_detach(pthread_self());
    return NULL;
  }
  memset(msg, '\0', sizeof(msg));
  snprintf(msg, sizeof(msg), "Connected to %s on port %hu\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
  send(client->socket_desc, msg, strlen(msg), 0);

  // Receive username from client and notify other clients
  recv(client->socket_desc, name, 32, 0);
  strlcpy(client->username, name, sizeof(client->username));
  //strncpy(client->username, name, strlen(name));
  add_client(client);
  memset(msg, '\0', sizeof(msg));
  char temp[] = " joined the chatroom\n";
  snprintf(msg, sizeof(msg), "%s%s", client->username, temp);
  broadcast(msg, client->id);
  memset(msg, '\0', sizeof(msg));

  // Loop, receiving messages from client
  int read_size = 0, disconnect = 0;
  while ( (read_size = recv(client->socket_desc, msg, sizeof(msg), 0) > 0) && !disconnect) {
    char stripped[strlen(msg)];
    memset(stripped, '\0', sizeof(stripped));
    //strncpy(stripped, msg, strlen(msg));
    strlcpy(stripped, msg, sizeof(stripped));
    strip_newline(stripped);

    // Check for special messages
    if (strcmp(stripped, ":Exit") == 0) {
      memset(msg, '\0', sizeof(msg));
      char temp[] = " left the chatroom\n";
      snprintf(msg, sizeof(msg), "%s%s", client->username, temp);
      broadcast(msg, client->id);
      disconnect = 1;
      break;
    } else if (strcmp(stripped, ":)") == 0) {
      memset(msg, '\0', sizeof(msg));
      char temp[] = "[feeling happy]\n";
      snprintf(msg, sizeof(msg), "%s", temp);
    } else if (strcmp(stripped, ":(") == 0) {
      memset(msg, '\0', sizeof(msg));
      char temp[] = "[feeling sad]\n";
      snprintf(msg, sizeof(msg), "%s", temp);
    } else if (strcmp(stripped, ":mytime") == 0) {
      memset(msg, '\0', sizeof(msg));
      time_t t = time(NULL);
      snprintf(msg, sizeof(msg), "%s", ctime(&t));
    } else if (strcmp(stripped, ":+1hr") == 0) {
      memset(msg, '\0', sizeof(msg));
      time_t t = time(NULL) + 3600;
      snprintf(msg, sizeof(msg), "%s", ctime(&t));
    }
    send_msg(msg, client);
    memset(msg, '\0', sizeof(msg));
  }

  close(client->socket_desc);
  remove_client(client);
  free(client);
  pthread_detach(pthread_self());
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 6) {
    printf("Command should be of the format: ./chatserver_tcp -start -port <port> -passcode <passcode>\n");
    return 1;
  }
  if (strcmp(argv[1], "-start") == 0 && strcmp(argv[2], "-port") == 0 && strcmp(argv[4], "-passcode") == 0) {
    // strncpy(passcode, argv[5], strlen(argv[5]));
    strlcpy(passcode, argv[5], sizeof(passcode));
  } else {
    printf("Command should be of the format: ./chatserver_tcp -start -port <port> -passcode <passcode>\n");
    return 1;
  }

  // Create listener socket
	int listen_socket;
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == -1)
	{
		printf("Could not create socket\n");
	}
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr("127.0.0.1");
  server.sin_port = htons(atoi(argv[3]));

  // Bind listener socket to server address
  if (bind(listen_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
    printf("Bind failed\n");
    return 1;
  }

  // Listen on listener socket for client connections
  listen(listen_socket, 10);
  printf("Server started on port %hu. Accepting connections\n", ntohs(server.sin_port));

  // Create thread for receiving server-side CLI input
  pthread_t input_thread;
  if (pthread_create(&input_thread, NULL, receive_input, NULL) != 0) {
    printf("Thread could not be created\n");
    return 1;
  }

  socklen_t addr_len = sizeof(sockaddr_in);
  int new_socket;

  // Loop, accepting connections from clients
  while ( (new_socket = accept(listen_socket, (sockaddr*)&(client_addr), &addr_len)) ) {
    /* new_socket = accept(listen_socket, (sockaddr*)&(client_addr), &addr_len); */
    if (new_socket < 0) {
      printf("Accept failed\n");
      return 1;
    }
    struct client_t* client = (struct client_t*) malloc(sizeof(struct client_t));
    client->address = client_addr;
    client->socket_desc = new_socket;
    if (current_id < 0) {
      printf("Max clients connected\n");
      free(client);
      return 1;
    }
    client->id = current_id++;
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, connection_handler, (void*)client) != 0) {
      printf("Thread could not be created\n");
      free(client);

      // TODO: free other clients???

      return 1;
    }
  }
  return 0;
}
