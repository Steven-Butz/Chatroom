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
int udp_socket, disconnect = 0;
pthread_mutex_t disconnect_mutex = PTHREAD_MUTEX_INITIALIZER;
struct sockaddr_in server;

void *send_message(void *) {
  char message[2000];
  while (!disconnect) {
    fgets(message, sizeof(message), stdin);
    if (strcmp(message, ":Exit\n") == 0) {
      pthread_mutex_lock(&disconnect_mutex);
      disconnect = 1;
      pthread_mutex_unlock(&disconnect_mutex);
    }
    sendto(udp_socket, message, strlen(message), 0, (sockaddr*)&server, (socklen_t)sizeof(sockaddr_in));
    //bzero(message, sizeof(message));
    memset(message, '\0', sizeof(message));
  }
  pthread_exit(NULL);
  return NULL;
}

void *recv_message(void *) {
  char message[2000];
  //bzero(message, sizeof(message));
  memset(message, '\0', sizeof(message));
  socklen_t addr_len;
  int x;
  while ( (x = recvfrom(udp_socket, message, sizeof(message), 0, (sockaddr*)&server, &addr_len)) >= 0 && !disconnect ) {
    
    printf("%s", message);
    //bzero(message, sizeof(message));
    memset(message, '\0', sizeof(message));
  }
  pthread_mutex_lock(&disconnect_mutex);
  disconnect = 1;
  pthread_mutex_unlock(&disconnect_mutex);
  pthread_detach(pthread_self());
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 10) {
    printf("Command should be of the format: ./chatclient_udp -join -host <hostname> -port <port> -username <username> -passcode <passcode>\n");
    return 1;
  }
  if (strcmp(argv[1], "-join") == 0 && strcmp(argv[2], "-host") == 0 && strcmp(argv[4], "-port") == 0
  && strcmp(argv[6], "-username") == 0 && strcmp(argv[8], "-passcode") == 0) {
    //strcpy(username, argv[7]);
    //strcpy(passcode, argv[9]);
    strlcpy(username, argv[7], sizeof(username));
    strlcpy(passcode, argv[9], sizeof(passcode));
  } else {
    printf("Command should be of the format: ./chatclient_udp -join -host <hostname> -port <port> -username <username> -passcode <passcode>\n");
    return 1;
  }

	//Create socket
	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_socket == -1)
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

  // Send passcode to server for authentication
  sendto(udp_socket, passcode, strlen(passcode), 0, (sockaddr*)&server, sizeof(sockaddr_in));
  char reply[200];
  memset(reply, '\0', sizeof(reply));
  socklen_t addr_len;
  recvfrom(udp_socket, reply, sizeof(reply), 0, (sockaddr*)&server, &addr_len);
  printf("%s", reply);
  if (strcmp(reply, "Incorrect passcode\n") == 0) {
    return 1;
  }

  // Send username to server
  sendto(udp_socket, username, strlen(username), 0, (sockaddr*)&server, sizeof(sockaddr_in));

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
  close(udp_socket);
	return 0;
}
