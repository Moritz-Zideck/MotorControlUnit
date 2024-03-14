#include <string>
#include <iostream>
#include <thread>

extern "C"
{
  #include "axis_controller.h"
  #include "thread_utils.h"	
}

class AxisHandler {

private:
    // Member variables
    string axisName;
    string ipAddress;
    int port;
    SOCKET socket;


public:
    // Constructor
    AxisHandler(int axisName, string ipAddress, int port){
        this->axisName = axisName;
        this->ipAddress = ipAddress;
        this->port = port;
    }

    // Destructor
    ~AxisHandler(){
        cout << "AxisHandler object destroyed" << endl;
    }

   //Member functions

    bool initialise(){
        std::jthread t1(initialise, &socket);

        int status = initialise(socket);
        if (status == 0) {
            return true;
        } else {
            return false;
        }
    }




    void moveForward();
    void moveBackward();
    void stop();
    void setPosition(int position);
    int getPosition();


};