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

char username[32], passcode[32];
int socket_desc;
static int disconnect = 0;
pthread_mutex_t disconnect_mutex = PTHREAD_MUTEX_INITIALIZER;
struct sockaddr_in server;

void *send_message(void *) {
  char message[2000];
  memset(message, '\0', sizeof(message));
  while (!disconnect) {
    fgets(message, sizeof(message), stdin);
    if (strcmp(message, ":Exit\n") == 0) {
      pthread_mutex_lock(&disconnect_mutex);
      disconnect = 1;
      pthread_mutex_unlock(&disconnect_mutex);
    }
    send(socket_desc, message, strlen(message), 0);
    memset(message, '\0', sizeof(message));
  }
  pthread_exit(NULL);
  return NULL;
}

void *recv_message(void *) {
  char message[2000];
  memset(message, '\0', sizeof(message));
  int recv_size;
  while ( (recv_size = recv(socket_desc, message, sizeof(message), 0)) > 0 && !disconnect) {
    printf("%s", message);
    memset(message, '\0', sizeof(message));
  }
  pthread_mutex_lock(&disconnect_mutex);
  disconnect = 1;
  pthread_mutex_unlock(&disconnect_mutex);
  //pthread_detach(pthread_self());
  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 10) {
    printf("Command should be of the format: ./chatclient_tcp -join -host <hostname> -port <port> -username <username> -passcode <passcode>\n");
    return 1;
  }
  if (strcmp(argv[1], "-join") == 0 && strcmp(argv[2], "-host") == 0 && strcmp(argv[4], "-port") == 0
  && strcmp(argv[6], "-username") == 0 && strcmp(argv[8], "-passcode") == 0) {
    if (strlen(argv[7]) >= sizeof(username) - 1) {
      printf("Username is too long\n");
      return 1;
    }
    if (strlen(argv[9]) >= sizeof(passcode) - 1) {
      printf("Incorrect passscode\n");
      return 1;
    }
    strlcpy(username, argv[7], sizeof(username));
    strlcpy(passcode, argv[9], sizeof(username));
    // strcpy(username, argv[7]);
    // strcpy(passcode, argv[9]);
  } else {
    printf("Command should be of the format: ./chatclient_tcp -join -host <hostname> -port <port> -username <username> -passcode <passcode>\n");
    return 1;
  }

	// Create socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket\n");
    return 1;
	}

  // Currently configured to only accept connections to localhost
  if (strcmp(argv[3], "localhost") != 0 && strcmp(argv[3], "127.0.0.1") != 0) {
    printf("Must connect to localhost\n");
    return 1;
  }
  char host[] = "127.0.0.1";
  server.sin_addr.s_addr = inet_addr(host);
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[5]));

	// Connect to server
	if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		printf("Connect error\n");
		return 1;
	}
  if (send(socket_desc, passcode, strlen(passcode), 0) < 0) {
    printf("Failed to send passcode\n");
    return 1;
  }
  char reply[200];
  memset(reply, '\0', sizeof(reply));
  if ((recv(socket_desc, reply, sizeof(reply), 0)) < 0) {
    printf("Failed to receive reply\n");
    return 1;
  }
  printf("%s", reply);
  if (send(socket_desc, username, strlen(username), 0) < 0) {
    printf("Failed to send username\n");
    return 1;
  }

  // Create two threads, one for sending and one for receiving
  pthread_t send_thread, recv_thread;
  if (pthread_create(&send_thread, NULL, send_message, NULL) != 0) {
    printf("Send thread could not be created\n");
    return 1;
  }
  if (pthread_create(&recv_thread, NULL, recv_message, NULL) != 0) {
    printf("Receive thread could not be created\n");
    return 1;
  }
  pthread_join(send_thread, NULL);
  close(socket_desc);
	return 0;
}