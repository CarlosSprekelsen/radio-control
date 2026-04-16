#pragma once
#include <memory>
#include <string>
class SilvusMock;

class HttpServer {
public:
    HttpServer(int port, SilvusMock* mock);
    void serve();
private:
    int port_;
    SilvusMock* mock_;
};
