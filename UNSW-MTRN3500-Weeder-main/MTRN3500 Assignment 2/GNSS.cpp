#include "GNSS.h"

// Constructors
GNSS::GNSS(SM_ThreadManagement^ SM_TM, SM_GNSS^ SM_GNSS) {

	SM_TM_ = SM_TM;
	SM_GNSS_ = SM_GNSS;
	Watch = gcnew Stopwatch;
	ReadData = gcnew array<unsigned char>(1024);
	GNSSDataArray = gcnew array<unsigned char>(112);
}

GNSS::~GNSS() {
	Client->Close();
}

// Derived from UGVModule
error_state GNSS::processSharedMemory() {

	double northing = BitConverter::ToDouble(GNSSDataArray, 44);	// Starting byte location is 40 + 4
	double easting = BitConverter::ToDouble(GNSSDataArray, 52);		// Starting byte location is 48 + 4
	double height = BitConverter::ToDouble(GNSSDataArray, 60);		// Starting byte location is 56 + 4

	// Request GNSS Mutex
	Monitor::Enter(SM_GNSS_->lockObject);

	SM_GNSS_->Northing = northing;
	SM_GNSS_->Easting = easting;
	SM_GNSS_->Height = height;

	// Release Laser Mutex
	Monitor::Exit(SM_GNSS_->lockObject);

	Console::WriteLine("Northing = {0:F3}, Easting = {1:F3}, Height = {2:F3}", northing, easting, height);

	return SUCCESS;
}

bool GNSS::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_GNSS);
}

void GNSS::threadFunction() {

	Console::WriteLine("GNSS thread is starting.");
	// Connect to GNSS
	error_state state = connect(WEEDER_ADDRESS, 24000);
	if (state != SUCCESS) {
		printError(state);
		shutdownThreads();
	}
	// Setup the stopwatch
	Watch = gcnew Stopwatch;
	// Wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();
	// Start the watch
	Watch->Start();
	// Start the loop
	while (!getShutdownFlag()) {
		Console::WriteLine("GNSS thread is running.");

		processHeartbeats();
		if (communicate() == SUCCESS && checkData() == SUCCESS) {
			// Do something with the data...
			processSharedMemory();
		}
		Thread::Sleep(20);
	}
	Console::WriteLine("GNSS thread is terminating.");
}

// Derived from NetworkedModule
error_state GNSS::connect(String^ hostName, int portNumber) {

	try {
		Client = gcnew TcpClient(hostName, portNumber);
	}
	catch (...) {
		return ERR_CONNECTION;
	}

	Stream = Client->GetStream();
	if (Stream == nullptr) return ERR_GNSS_FAILURE;

	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	return SUCCESS;
}

error_state GNSS::communicate() {

	if (Stream == nullptr) return ERR_GNSS_FAILURE;
	try {
		// Read the response
		Stream->Read(ReadData, 0, ReadData->Length);
	}
	catch (...) {
		printError(ERR_CONNECTION);
		return ERR_CONNECTION;
	}

	// Header detection
	unsigned int Header = 0;
	unsigned char Data;
	int index = 0;
	do {
		Data = ReadData[index++];
		Header = (Header << 8) | Data;
	} while (Header != 0xaa44121c);

	// Store GNSS data into an array of size 112
	GNSSDataArray[0] = 0xaa;
	GNSSDataArray[1] = 0x44;
	GNSSDataArray[2] = 0x12;
	GNSSDataArray[3] = 0x1c;
	for (int i = 4; i < GNSSDataArray->Length; i++) {
		GNSSDataArray[i] = ReadData[index++];
	}

	return SUCCESS;
}

// Member functions
error_state GNSS::setupSharedMemory() {
	return SUCCESS;
}

error_state GNSS::processHeartbeats() {

	// Is the GNSS bit in the heartbeat byte down?
	if ((SM_TM_->heartbeat & bit_GNSS) == 0) {
		// Put the GNSS bit up
		SM_TM_->heartbeat |= bit_GNSS;
		// Restart timer
		Watch->Restart();
	}
	else {
		// Has the time elapsed exceeded the crash time limit?
		if (Watch->ElapsedMilliseconds > CRASH_LIMIT) {
			// Shutdown all threads
			shutdownThreads();
			return ERR_TMT_FAILURE;
		}
	}

	return SUCCESS;
}

void GNSS::shutdownThreads() {
	SM_TM_->shutdown = bit_GNSS;
}

error_state GNSS::checkData() {

	GNSSData myGNSSData;
	unsigned char* BytePtr = (unsigned char*)&myGNSSData;
	
	for (int i = 0; i < sizeof(GNSSData); i++) {
		*(BytePtr + i) = GNSSDataArray[i];
	}
	// Compare calculatedCRC with received CRC
	unsigned long calculatedCRC = CalculateBlockCRC32(sizeof(GNSSData), BytePtr);
	uint32_t receivedCRC = BitConverter::ToUInt32(GNSSDataArray, 108);	// Starting byte location is 104 + 4
	Console::WriteLine("Recieved CRC = {0:F3}, Calculated CRC = {1:F3}", receivedCRC, calculatedCRC);

	if ((uint32_t)calculatedCRC == receivedCRC) {
		return SUCCESS;
	}
	else {
		return ERR_INVALID_DATA;
	}
}

unsigned long GNSS::CRC32Value(int i) {

	int j;
	unsigned long ulCRC;
	ulCRC = i;
	for (j = 8; j > 0; j--) {
		if (ulCRC & 1)
			ulCRC = (ulCRC >> 1) ^ CRC32_POLYNOMIAL;
		else
			ulCRC >>= 1;
	}
	return ulCRC;
}

unsigned long GNSS::CalculateBlockCRC32(unsigned long ulCount, unsigned char* ucBuffer) {

	unsigned long ulTemp1;
	unsigned long ulTemp2;
	unsigned long ulCRC = 0;
	while (ulCount-- != 0) {
		ulTemp1 = (ulCRC >> 8) & 0x00FFFFFFL;
		ulTemp2 = CRC32Value(((int)ulCRC ^ * ucBuffer++) & 0xff);
		ulCRC = ulTemp1 ^ ulTemp2;
	}
	return(ulCRC);
}