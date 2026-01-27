
#include <iostream>
#include <string>
#include <chrono>
#include "httplib.h"

int main() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::cout << "[LOG] Request received." << std::endl;

        // Simulate logic processing time if needed, or just measure the overhead
        // calculating the response creation time.
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        std::string content = "hello from vamsi!\n";
        content += "this is pure text content.\n";
        content += "no html tags are used here.\n\n";
        content += "server render time: " + std::to_string(elapsed.count()) + " ms";

        res.set_content(content, "text/plain");
    });

    svr.Get("/2", [] (const httplib:: Request& reg, httplib::Response& res) {
        std::cout<<"[LOG2] req recieved"<< std::endl;
        res.set_content(" this is pure genius ntenti am the best \t ","text/plain");

    });

    // 3. Start the Server
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << " C++ Text Server Starting..." << std::endl;
    std::cout << " Open: http://localhost:8080" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    svr.listen("0.0.0.0", 8080);

    return 0;
}
