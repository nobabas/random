#pragma once

#include <NetworkedModule.h>

#define CRC32_POLYNOMIAL 0xEDB88320L

#pragma pack(push, 4)
struct GNSSData {
	unsigned int Header;
	unsigned char Discards1[40];
	double Northing;
	double Easting;
	double Height;
	unsigned char Discards2[40];
};
#pragma pack(pop, 4)

ref class GNSS : NetworkedModule {
public:
	// Constructors
	GNSS(SM_ThreadManagement^ SM_TM, SM_GNSS^ SM_GNSS);
	~GNSS();

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
	unsigned long CRC32Value(int i);
	unsigned long CalculateBlockCRC32(unsigned long ulCount, unsigned char* ucBuffer);

private:
	SM_ThreadManagement^ SM_TM_;
	SM_GNSS^ SM_GNSS_;
	Stopwatch^ Watch;
	array<unsigned char>^ GNSSDataArray;
};
