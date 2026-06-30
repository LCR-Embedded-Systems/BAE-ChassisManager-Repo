#include "../include/lcr_muxtest.hpp"

bool IsServiceRunning(const std::string& serviceName) {
    // -q suppresses stdout, returning only the exit status code
    std::string command = "systemctl -q is-active " + serviceName;
    int exitCode = std::system(command.c_str());
    
    // WEXITSTATUS extracts the real exit code from the wait() status
    return (WEXITSTATUS(exitCode) == 0);
}

muxtest::muxtest() 
{
    
};

muxtest::~muxtest() 
{

};

int muxtest::runtest() 
{

    if (IsServiceRunning("serial-getty@ttyPS1.service")) {
        std::system("chmod 660 /dev/ttyPS1 && systemctl stop serial-getty@ttyPS1.service && systemctl disable serial-getty@ttyPS1.service && systemctl mask serial-getty@ttyPS1.service");
    }

    int returnval = 0;
    for (int i = 0; i < 4; i++) {

        std::string command = "mux " + std::to_string(i);
        std::cout << "Running command: " << command << ", press enter to continue...";
        std::cin.get();
        std::system(command.c_str());

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        int serialPort = open("/dev/ttyPS1", O_RDWR);

        if (serialPort < 0) {
            std::cerr << "Error opening serial port\n";
            return 1;
        }
    
        //Erase any leftover data in serial port.
        tcflush(serialPort, TCIOFLUSH);

        struct termios tty;
        if(tcgetattr(serialPort, &tty) != 0) {
            std::cerr << "Error from tcgetattr\n";
            return 1;
        }
     
        cfmakeraw(&tty);

        // Configure the connection variables
        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);
        tty.c_cflag &= ~PARENB;        // Clear parity bit
        tty.c_cflag &= ~CSTOPB;        // Clear stop field (1 stop bit)
        tty.c_cflag &= ~CSIZE;         // Clear size bits
        tty.c_cflag |= CS8;            // 8 data bits
        tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
    
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 5;
    
        // Save tty settings
        if (tcsetattr(serialPort, TCSANOW, &tty) != 0) {
            std::cerr << "Error " << errno << " from tcsetattr" << std::endl;
            return 1;
        }
    
        // Send data
        unsigned char msg[] = { 'H', 'e', 'l', 'l', 'o' };
        int bytes_written = write(serialPort, msg, sizeof(msg));
        if (bytes_written < 0) {
            std::cerr << "Error writing to port" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        char read_buf [256];
        std::memset(read_buf, 0, sizeof(read_buf));
        int num_bytes = read(serialPort, read_buf, sizeof(read_buf));
    
        if (num_bytes < 0) {
            std::cerr << "Error reading from port" << std::endl;
        } else if (num_bytes == 0) {
            returnval = 1;
            std::cout << "Read timeout reached, no data received." << std::endl;
        } else {
            std::cout << "Read " << num_bytes << " bytes. Message: " << read_buf << std::endl;
        }
    
        // Close the file descriptor
        close(serialPort);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    }

    std::cout << "Returning mux to channel 0" << std::endl;
    std::system("mux 0");

    return returnval;

};
