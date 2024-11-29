# CTRP Network Programming Project

This project consists of a client-server application built using Qt and C++. The server listens for incoming connections and handles account management, while the client connects to the server to perform various operations.

## Prerequisites

- Qt 6.7.2 or later
- C++17 compatible compiler
- Homebrew (for macOS users)

## Installation

1. **Install Qt**:
  ```sh
  brew install qt
  ```

2. **Clone the repository**
  ```sh
  git clone <repository-url>
  cd <repository-directory>
  ```

## Building the Project
### Server
1. **Navigate to the server project directory**
  ```sh
  cd CTRP_Server
  ```
2. **Run qmake to generate the Makefile**
3. **Build the project using make**
4. **Run the server application**
  ```sh
  ./CTRP_Server.app/Contents/MacOS/CTRP_Server
  ```

### Client
1. **Navigate to the server project directory**
  ```sh
  cd CTRP_Client
2. **Run qmake to generate the Makefile**
3. **Build the project using make**
4. **Run the server application**
  ```sh
  ./CTRP_Client.app/Contents/MacOS/CTRP_Client
  ```

## Usage
1. **Start the server application**
2. **Start the client application and connect to the server**
3. **Perform operations such as logging in, signing up, etc**

## Project Structure
- CTRP_Server/: Contains the server-side code.
- CTRP_Client/: Contains the client-side code.
- CTRP_Server.pro: Project file for the server.
- CTRP_Client.pro: Project file for the client.

## Contributing
Contributions are welcome! Please open an issue or submit a pull request.

## Contact
For any questions or issues, please contact [SmithMerch2301@gmail.com].

