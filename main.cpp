// ----------------------------------------------------------------------------
// C++ TEXT SERVER (main.cpp)
// ----------------------------------------------------------------------------
// This file serves raw text to the browser.
// No HTML. No CSS. Just C++ strings.
// ----------------------------------------------------------------------------

#include <iostream>
#include <string>
#include "httplib.h"

int main() {
    // 1. Initialize the Server object
    httplib::Server svr;

    // 2. Define the route
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        
        std::cout << "[LOG] Request received." << std::endl;

        // "text/plain" ensures the browser treats this as raw text, 
        // not attempting to parse any tags.
        res.set_content("Hello from C++!\nThis is pure text content.\nNo HTML tags are used here.", "text/plain");
    });

    // 3. Start the Server
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << " C++ Text Server Starting..." << std::endl;
    std::cout << " Open: http://localhost:8080" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    svr.listen("0.0.0.0", 8080);

    return 0;
}
