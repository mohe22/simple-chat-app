### Server:
This server file implements a simple multi-client chat server using epoll for efficient event notification and non-blocking sockets. The server listens on a specific port (`8080`), accepts incoming client connections, and broadcasts messages to all connected clients except the sender. The server processes incoming messages in JSON format, supporting client registration (by name) and message broadcasting It uses the `nlohmann::json` library for JSON manipulation, and `fcntl` for setting non-blocking sockets. The server utilizes `epoll` for event-driven I/O and handles multiple clients efficiently, ensuring the scalability of the server under heavy loads.

**Key Features:**
- Non-blocking socket setup for the server and clients.
- Efficient handling of multiple clients using epoll.
- Broadcasts messages to all clients except the sender.
- Registers clients by their name and manages their connections.
- Utilizes `nlohmann::json` for message serialization and deserialization.

---

### Client:
This client file implements a simple chat client that connects to the server, registers the user by name, and allows sending and receiving messages. The client uses a non-blocking socket to connect to the server and creates a separate thread for receiving messages. The client sends messages in JSON format, which includes the user name, message, and the time of sending. The `nlohmann::json` library is used to format the messages. It handles input from the user for message sending, displays received messages in a timestamped format, and ensures that the client is non-blocking to allow continuous message sending and receiving.

**Key Features:**
- Non-blocking socket for communication.
- Connects to a chat server and registers the user with a name.
- Sends and receives messages in JSON format.
- Displays received messages with the sender’s name and timestamp.
- Multi-threaded: Uses a separate thread for receiving messages, allowing continuous interaction.

