Guess the Number Game Server

Overview
This project implements a Guess the Number game server in C using TCP sockets and the select() system call to handle multiple concurrent player connections. Players connect to the server, make guesses, and compete to guess a randomly generated number.

Features:
✔ Handles multiple concurrent players (up to 100).
✔ Uses non-blocking I/O with select() to manage sockets efficiently.
✔ Supports real-time game updates where all players see each guess.
✔ Handles disconnections gracefully and notifies remaining players.
✔ Implements server-side validation of input parameters.
✔ Provides debugging messages to track server activity.
✔ Graceful shutdown on SIGINT (Ctrl+C).

Files Overview:
- gameServer.c   -> Main server implementation
- README.txt     -> Project documentation

How to Compile and Run:

Prerequisites:
- Linux/macOS terminal (or MinGW for Windows users)
- GCC compiler installed
- POSIX socket support

Compilation:
gcc -Wall -o server gameServer.c

Running the Server:
./server <port> <seed> <max-number-of-players>

Command-Line Arguments:
- <port> -> The port number the server will listen on (1-65535)
- <seed> -> Random seed used for srand()
- <max-number-of-players> -> Maximum number of players allowed (2-100)

Example Usage:
./server 8080 42 10

This starts the server on port 8080, with 10 maximum players, and initializes the random number generator using seed 42.

Game Flow:
1. The server generates a random number between 1 and 100.
2. Players connect using a TCP client (e.g., telnet localhost <port>).
3. When a player joins, they receive a unique ID and a welcome message:
   Welcome to the game, your id is <ID>
4. Other players are notified:
   Player <ID> joined the game
5. Players take turns guessing numbers:
   - The server broadcasts:
     Player <ID> guessed <X>
   - If the guess is incorrect:
     The guess <X> is too high
     or
     The guess <X> is too low
   - If a player guesses correctly, the server sends:
     Player <ID> wins
     The correct guessing is <X>
   - The game resets, and a new number is generated.

6. If a player disconnects, the server notifies all other players:
   Player <ID> disconnected

Debugging Messages:
The server prints useful logs during execution:
1. When the server is ready for new connections:
   Server is ready to read from welcome socket <socket_descriptor>
2. When reading from a player socket:
   Server is ready to read from player <ID> on socket <socket_descriptor>
3. When writing to a player socket:
   Server is ready to write to player <ID> on socket <socket_descriptor>

Testing the Server:
1. Open a terminal and run the server:
   ./server 8080 42 5
2. Open multiple terminals and connect as players using:
   telnet localhost 8080
3. Type numbers and verify the server responses.
4. To disconnect a player, press Ctrl-5 and type quit.

Error Handling:
- Invalid arguments → Prints usage and exits:
  Usage: ./server <port> <seed> <max-number-of-players>
- System call failures (socket errors, bind failures, etc.) → Logged with perror().
- Client disconnects → Handled properly without crashing the server.

Future Improvements:
- Implement leaderboards to track player wins.
- Add support for additional game modes.
- Improve performance optimizations for high player counts.

License:
This project is licensed under the MIT License.

Happy coding! 🚀
