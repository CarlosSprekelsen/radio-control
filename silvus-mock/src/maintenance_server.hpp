#pragma once

#include <memory>

class SilvusMock;

class MaintenanceServer {
public:
    MaintenanceServer(int port, SilvusMock* mock);
    ~MaintenanceServer();

    void start();
    void stop();

private:
    int port_;
    SilvusMock* mock_;
    class Impl;
    std::unique_ptr<Impl> impl_;
};
