#pragma once

#include <NetworkedModule.h>
#include <SMObjects.h>

ref class Laser : NetworkedModule {
public:
	// Constructors
	Laser(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser);
	~Laser();

	// Derived from UGVModule
	error_state processSharedMemory() override;
	bool getShutdownFlag() override;
	void threadFunction() override;

	// Derived from NetworkedModule
	error_state connect(String^ hostName, int portNumber) override;
	error_state communicate() override;

	// Member functions
	error_state setupSharedMemory();
	error_state processHeartbeats();
	void shutdownThreads();
	error_state checkData();
	
private:
	SM_ThreadManagement^ SM_TM_;
	SM_Laser^ SM_Laser_;
	Stopwatch^ Watch;
	array<unsigned char>^ SendData;
	array<String^>^ StringArray;
};