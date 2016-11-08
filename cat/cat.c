#include <unistd.h>
int main() {
    char buf[1024];
    ssize_t bytesRead;

    while((bytesRead = read(STDIN_FILENO, buf, sizeof(buf))) != 0) {
        if (bytesRead > 0) { // success
            ssize_t bytesWritten = 0;
            while(bytesWritten - bytesRead != 0) {
                ssize_t bytes = write(STDOUT_FILENO, buf + bytesWritten, bytesRead - bytesWritten);
                if(bytes == -1) // error
                    return 1;
                bytesWritten += bytes;
            }
        } else { // -1: error
            return 1;
        }
    }

    return 0;
}
