#include "Laser.h"

// Constructors
Laser::Laser(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser) {

	SM_TM_ = SM_TM;
	SM_Laser_ = SM_Laser;
	Watch = gcnew Stopwatch;
	SendData = gcnew array<unsigned char>(1024);
	ReadData = gcnew array<unsigned char>(2048);
	StringArray = gcnew array<String^>(1024);
}

Laser::~Laser() {
	Client->Close();
}

// Derived from UGVModule
error_state Laser::processSharedMemory() {

	double Resolution = Convert::ToInt32(StringArray[24], 16) / 10000.0 * (Math::PI / 180.0);
	array<double>^ Range = gcnew array<double>(STANDARD_LASER_LENGTH);

	// Request Laser Mutex
	Monitor::Enter(SM_Laser_->lockObject);

	for (int i = 0; i < STANDARD_LASER_LENGTH; i++) {
		Range[i] = Convert::ToInt32(StringArray[26 + i], 16);
		SM_Laser_->x[i] = Range[i] * Math::Sin(i * Resolution);
		SM_Laser_->y[i] = -Range[i] * Math::Cos(i * Resolution);
		//Console::WriteLine("x = {0, 10:F3}, y = {1, 10:F3}", SM_Laser_->x[i], SM_Laser_->y[i]);
	}

	// Release Laser Mutex
	Monitor::Exit(SM_Laser_->lockObject);

	return SUCCESS;
}

bool Laser::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_LASER);
}

void Laser::threadFunction() {

	Console::WriteLine("Laser thread is starting.");
	// Connect to laser
	error_state state = connect(WEEDER_ADDRESS, 23000);
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
		Console::WriteLine("Laser thread is running.");

		processHeartbeats();
		if (communicate() == SUCCESS && checkData() == SUCCESS) {
			// Do something with the data...
			processSharedMemory();
		}
		Thread::Sleep(20);
	}
	Console::WriteLine("Laser thread is terminating.");
}

// Derived from NetworkedModule
error_state Laser::connect(String^ hostName, int portNumber) {

	try {
		Client = gcnew TcpClient(hostName, portNumber);
	}
	catch (...) {
		return ERR_CONNECTION;
	}

	Stream = Client->GetStream();
	if (Stream == nullptr) return ERR_LASER_FAILURE;

	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	// Authentication
	try {
		SendData = Encoding::ASCII->GetBytes(AUTHENTICATION_REQUEST);
		Stream->Write(SendData, 0, SendData->Length);
		Threading::Thread::Sleep(10);
		Stream->Read(ReadData, 0, ReadData->Length);
		String^ Response = Encoding::ASCII->GetString(ReadData);
		if (String::Compare(Response, AUTHENTICATION_RESPONSE) != 0) return ERR_AUTHENTICATION_FAILURE;
	}
	catch (...) {
		return ERR_AUTHENTICATION_FAILURE;
	}
	
	return SUCCESS;
}

error_state Laser::communicate() {

	if (Stream == nullptr) return ERR_LASER_FAILURE;

	SendData = Encoding::ASCII->GetBytes("sRN LMDscandata");

	// Send start-of-text (STX) first
	Stream->WriteByte(0x02);
	// Send the command
	Stream->Write(SendData, 0, SendData->Length);
	// Send end-of-text (ETX)
	Stream->WriteByte(0x03);
	// Wait 10 ms
	Threading::Thread::Sleep(10);
	try {
		// Read the response
		Stream->Read(ReadData, 0, ReadData->Length);
	}
	catch (...) {
		printError(ERR_CONNECTION);
		return ERR_CONNECTION;
	}
	// Get ASCII response
	String^ Response = Encoding::ASCII->GetString(ReadData);
	// Input into string array
	StringArray->Clear;
	StringArray = Response->Split(' ');
	Threading::Thread::Sleep(25);

	return SUCCESS;
}

// Member functions
error_state Laser::setupSharedMemory() {
	return SUCCESS;
}

error_state Laser::processHeartbeats() {

	// Is the laser bit in the heartbeat byte down?
	if ((SM_TM_->heartbeat & bit_LASER) == 0) {
		// Put the laser bit up
		SM_TM_->heartbeat |= bit_LASER;
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

void Laser::shutdownThreads() {
	SM_TM_->shutdown = bit_LASER;
}

error_state Laser::checkData() {

	try {
		String^ typeOfCommand = StringArray[0];
		String^ command = StringArray[1];
		double startAngle = Convert::ToInt32(StringArray[23], 16);
		double resolution = Convert::ToInt32(StringArray[24], 16) / 10000.0;
		int numRanges = Convert::ToInt32(StringArray[25], 16);

		bool flag = true;
		//if (String::Compare(typeOfCommand, "sRN") != 0) flag = false;
		if (String::Compare(command, "LMDscandata") != 0) flag = false;
		if (startAngle != 0) flag = false;
		if (resolution != 0.5) flag = false;
		if (numRanges != STANDARD_LASER_LENGTH) flag = false;

		if (!flag) {
			printError(ERR_INVALID_DATA);
			return ERR_INVALID_DATA;
		}
	}
	catch (...) {
		printError(ERR_NO_DATA);
		return ERR_NO_DATA;
	}

	return SUCCESS;
}

