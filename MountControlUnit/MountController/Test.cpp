
extern "C"
{
  #include "axis_controller.h"
  #include "thread_utils.h"	
}

#include <iostream>
#include <thread>
using namespace std;

int main() {
    // Create a socket
    SOCKET socket;
    char ip_Address[] = "127.0.0.1";
    int port = 1234;
    int connectionStatus = createConnection(&socket, ip_Address, port);
    
    if (connectionStatus == 0) {
        // Connection successful, perform desired operation
        int boardStatus;
        int status = getSocketStatus(socket, &boardStatus);
        
        if (status == 0) {
            // Successfully retrieved board status
            // Perform further operations as needed
        } else {
            // Error occurred while retrieving board status
        }
    } else {
        // Error occurred while creating connection
    }
    
    // Close the socket
    closesocket(socket);
    
    return 0;
}