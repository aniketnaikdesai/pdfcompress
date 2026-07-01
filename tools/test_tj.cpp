#include <turbojpeg.h>
#include <iostream>
int main() {
    std::cerr << "Calling tjInitCompress..." << std::endl;
    tjhandle h = tjInitCompress();
    std::cerr << "tjInitCompress returned: " << h << std::endl;
    if (h) {
        std::cerr << "Calling tjDestroy..." << std::endl;
        tjDestroy(h);
        std::cerr << "SUCCESS" << std::endl;
    } else {
        std::cerr << "FAILED: " << tjGetErrorStr() << std::endl;
    }
    return h ? 0 : 1;
}
