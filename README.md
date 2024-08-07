## Introduction
A simple client-server chatroom, implemented using C sockets programming. There are 2 pairs of client-server programs, one using TCP and one UDP. The TCP server program is multi-threaded, with one thread for each client connection (max. 10 concurrent clients).

## Compilation

The folders should compile into 4 distinct exectutables:
`chatserver_tcp, chatclient_tcp, chatserver_udp, chatclient_udp`.

Run `make` to compile everything, or `make build-[server|client]_[tcp|udp]` to compile a specific program.

## Execution

#### TCP

To run the TCP server and client, use the following commands:

`./chatserver_tcp -start -port [port] -passcode [passcode]`

`./chatclient_tcp -join -host [hostname] -port [port] -username [username] -passcode [passcode]`

#### UDP

To run the UDP server and client, use the following commands:

`./chatserver_udp -start -port [port] -passcode [passcode]`

`./chatclient_udp -join -host [hostname] -port [port] -username [username] -passcode [passcode]`
