#include "Display.h"

// Constructors
Display::Display(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser) {
	SM_TM_ = SM_TM;
	SM_Laser_ = SM_Laser;
	Watch = gcnew Stopwatch;
	SendData = gcnew array<unsigned char>(64);
	ReadData = gcnew array<unsigned char>(64);
}

Display::~Display() {
	Client->Close();
}

// Derived from UGVModule
error_state Display::processSharedMemory() {

	array<double>^ xAxisData = gcnew array<double>(STANDARD_LASER_LENGTH);
	array<double>^ yAxisData = gcnew array<double>(STANDARD_LASER_LENGTH);

	// Request Laser Mutex
	Monitor::Enter(SM_Laser_->lockObject);
	
	for (int i = 0; i < STANDARD_LASER_LENGTH; i++) {
		xAxisData[i] = -SM_Laser_->y[i];
		yAxisData[i] = SM_Laser_->x[i];
	}

	// Release Laser Mutex
	Monitor::Exit(SM_Laser_->lockObject);

	sendDisplayData(xAxisData, yAxisData);

	return SUCCESS;
}

bool Display::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_DISPLAY);
}

void Display::threadFunction() {

	Console::WriteLine("Display thread is starting.");
	// Connect to display
	error_state state = connect(DISPLAY_ADDRESS, 28000);
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
		Console::WriteLine("Display thread is running.");

		processHeartbeats();
		if (communicate() == SUCCESS && checkData() == SUCCESS) {
			// Do something with the data...
			processSharedMemory();
		}
		Thread::Sleep(20);
	}
	Console::WriteLine("Display thread is terminating.");
}

// Derived from NetworkedModule
error_state Display::connect(String^ hostName, int portNumber) {

	try {
		Client = gcnew TcpClient(hostName, portNumber);
	}
	catch (...) {
		return ERR_CONNECTION;
	}

	Stream = Client->GetStream();
	if (Stream == nullptr) return ERR_DISPLAY_FAILURE;

	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	return SUCCESS;
}

error_state Display::communicate() {
	return SUCCESS;
}

// Member functions
error_state Display::setupSharedMemory() {
	return SUCCESS;
}

error_state Display::processHeartbeats() {

	// Is the display bit in the heartbeat byte down?
	if ((SM_TM_->heartbeat & bit_DISPLAY) == 0) {
		// Put the display bit up
		SM_TM_->heartbeat |= bit_DISPLAY;
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

void Display::shutdownThreads() {
	SM_TM_->shutdown = bit_DISPLAY;
}

error_state Display::checkData() {
	return SUCCESS;
}

void Display::sendDisplayData(array<double>^ xData, array<double>^ yData) {

	// Serialize the data arrays to a byte array (format required for sending)
	array<Byte>^ dataX = gcnew array<Byte>(xData->Length * sizeof(double));
	Buffer::BlockCopy(xData, 0, dataX, 0, dataX->Length);
	array<Byte>^ dataY = gcnew array<Byte>(yData->Length * sizeof(double));
	Buffer::BlockCopy(yData, 0, dataY, 0, dataY->Length);

	// Check display connection
	if (Stream == nullptr) {
		printError(ERR_DISPLAY_FAILURE);
		shutdownThreads();
		return;
	}
	// Send byte data over connection
	Stream->Write(dataX, 0, dataX->Length);
	Thread::Sleep(10);
	Stream->Write(dataY, 0, dataY->Length);
}