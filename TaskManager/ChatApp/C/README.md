# ChatApp

A secure chat application with SSL/TLS encryption, file transfer capabilities, and room management.

## Features

- Secure communication using SSL/TLS
- Private messaging
- Chat rooms
- File transfer
- User status management
- Text-based UI

## Requirements

- OpenSSL 3.0 or later
- GCC compiler
- POSIX-compliant system (Linux, macOS, etc.)
- pthread support

## Building

1. Make sure you have OpenSSL installed:
   - On macOS: `brew install openssl@3`
   - On Linux: `sudo apt-get install libssl-dev`

2. Build the application:
   ```bash
   make clean
   make all
   ```

## Running

You can start both the server and client using the provided script:

```bash
./start_chat.sh
```

Or run them separately:

1. Start the server:
   ```bash
   ./bin/chat_server 8080
   ```

2. Start the client:
   ```bash
   ./bin/chat_client 127.0.0.1 8080
   ```

## Commands

- `/msg <username> <message>` - Send private message
- `/status <online|away|busy> [message]` - Change status
- `/join <room_id>` - Join a chat room
- `/create <room_name>` - Create a new chat room
- `/list` - Show online users
- `/rooms` - List available rooms
- `/help` - Show help message
- `/file <username> <filepath>` - Send file to user
- `/files` - List received files
- `/download <file_id>` - Download received file
- `/exit` - Exit the chat

## Directory Structure

- `bin/` - Compiled executables
- `obj/` - Object files
- `certs/` - SSL certificates
- `downloads/` - Downloaded files
- `uploads/` - Files to be sent

## Security

The application uses OpenSSL for secure communication:
- TLS 1.2 or later
- Self-signed certificates for development
- SHA-256 for file checksums
- Secure file transfer with verification 