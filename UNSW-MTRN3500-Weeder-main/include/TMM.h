#pragma once

#include <conio.h>
#include <UGVModule.h>

#include "Controller.h"
#include "Laser.h"
#include "GNSS.h"
#include "VC.h"
#include "Display.h"
#include "CrashAvoidance.h"

ref struct ThreadProperties {
public:
    ThreadProperties(ThreadStart^ start, bool crit, String^ threadName, uint8_t bit_id) {
        ThreadStart_ = start;
        Critical = crit;
        ThreadName = threadName;
        BitID = bit_id;
    }
public:
    ThreadStart^ ThreadStart_;
    bool Critical;
    String^ ThreadName;
    uint8_t BitID;
};

ref class ThreadManagement : public UGVModule {
public:
    // Send/Recieve data from shared memory structures
    error_state processSharedMemory() override;

    // Get Shutdown signal for module, from Thread Management SM
    bool getShutdownFlag() override;

    // Thread function for TMM
    void threadFunction() override;

    // Create shared memory objects
    error_state setupSharedMemory();

    // Shutdown all modules in the software
    void shutdownModules();

    // Process heartbeats
    error_state processHeartbeats();

private:
    // Add any additional data members or helper functions here
    SM_Laser^ SM_Laser_;
    SM_GNSS^ SM_GNSS_;
    SM_VehicleControl^ SM_VehicleControl_;

    array<Thread^>^ ThreadList;
    array<ThreadProperties^>^ ThreadPropertiesList;
};
