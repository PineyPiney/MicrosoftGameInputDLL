
#include <iostream>
#include <tchar.h>
#include <Windows.h>

#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include <GameInput.h>

using namespace GameInput::v3;

struct Controller {
	IGameInputDevice* device;
	IGameInputReading* lastReading = nullptr;
	const GameInputDeviceInfo* info = nullptr;
};

typedef int(__stdcall* f_init)();
typedef int(__stdcall* f_setConnectCallback)(const std::function<void(Controller*)>& cb);
typedef int(__stdcall* f_setDisconnectCallback)(const std::function<void(IGameInputDevice*)>& cb);
typedef int(__stdcall* f_updateReading)(Controller* controller);
typedef int(__stdcall* f_pollGamepad)(Controller* controller, GameInputGamepadState* state);
typedef int(__stdcall* f_pollController)(Controller* controller, uint32_t* numButtons, bool** buttons, uint32_t* numAxes, float** axes);
typedef int(__stdcall* f_setRumble)(Controller* controller, float lowFrequency, float highFrequency, float leftTrigger, float rightTrigger);
typedef int(__stdcall* f_releaseController)(Controller* controller);
typedef int(__stdcall* f_release)();


std::vector<Controller*> devices = std::vector<Controller*>();
std::mutex m;

std::map<HRESULT, const char*> hresultMessages = {
	{ 0x838A0001L, "DEVICE_DISCONNECTED" },
	{ 0x838A0002L, "DEVICE_NOT_FOUND" },
	{ 0x838A0003L, "READING_NOT_FOUND" },
	{ 0x838A0004L, "REFERENCE_READING_TOO_OLD" },
	{ 0x838A0007L, "FEEDBACK_NOT_SUPPORTED" },
	{ 0x838A0008L, "OBJECT_NO_LONGER_EXISTS" },
	{ 0x838A0009L, "CALLBACK_NOT_FOUND" },
	{ 0x838A000AL, "HAPTIC_INFO_NOT_FOUND" },
	{ 0x838A000BL, "AGGREGATE_OPERATION_NOT_SUPPORTED" },
	{ 0x838A000CL, "INPUT_KIND_NOT_PRESENT" }
};

const char* getErrorMessage(HRESULT result) {
	if (result >= 0) return nullptr;
	auto message = hresultMessages.find(result);
	if (message == hresultMessages.end()) {
		return "Unknown Error " + result;
	}
	else return message->second;
}

void processError(HRESULT result, const char* message) {
	const char* error = getErrorMessage(result);
	if (error != nullptr) std::cout << message << ": " << error << std::endl;
}


int main() {

	HMODULE dll = LoadLibraryW(_T("GameInput.dll"));
	if (!dll || dll == INVALID_HANDLE_VALUE) {
		std::cout << "Invalid handle for GameInput.dll";
		return -1;
	}

	std::cout << "Dll is " << dll << std::endl;
	
	f_init init = (f_init)GetProcAddress(dll, "initInput");
	f_setConnectCallback setConnectCallback = (f_setConnectCallback)GetProcAddress(dll, "setConnectCallback");
	f_setDisconnectCallback setDisconnectCallback = (f_setDisconnectCallback)GetProcAddress(dll, "setDisconnectCallback");
	f_updateReading updateReading = (f_updateReading)GetProcAddress(dll, "updateReading");
	f_pollGamepad pollGamepad = (f_pollGamepad)GetProcAddress(dll, "pollGamepad");
	f_pollController pollController = (f_pollController)GetProcAddress(dll, "pollController");
	f_setRumble setRumble = (f_setRumble)GetProcAddress(dll, "setRumble");
	f_releaseController releaseController = (f_releaseController)GetProcAddress(dll, "releaseController");
	f_release release = (f_release)GetProcAddress(dll, "releaseInput");

	processError(init(), "Failed to initialise GameInput");

	setConnectCallback([](Controller* controller) {
		std::cout << "Controller Connected" <<
			"\n\tName = " << controller->info->displayName <<
			"\n\tFamily = " << controller->info->deviceFamily <<
			"\n\tType = 0x" << std::hex << controller->info->supportedInput <<
			"\n\tGuid = 0x" << std::hex << controller->info->containerId.Data1 <<
			std::endl;

		std::lock_guard lk(m);
		devices.push_back(controller);
	});

	setDisconnectCallback([releaseController](IGameInputDevice* device) {

		std::lock_guard lk(m);
		auto it = std::find_if(devices.begin(), devices.end(), [device](Controller* con) { return con->device == device; });
		auto controller = *it;

		std::cout << "Controller Disconnected" <<
			"\n\tName = " << controller->info->displayName <<
			"\n\tFamily = " << controller->info->deviceFamily <<
			"\n\tType = 0x" << std::hex << controller->info->supportedInput <<
			"\n\tGuid = 0x" << std::hex << controller->info->containerId.Data1 <<
			std::endl;

		releaseController(controller);
		devices.erase(it);
	});

	GameInputGamepadState state;
	bool run = true;

	while (run) {
		std::lock_guard lk(m);
		for (Controller* device : devices) {

			HRESULT readError = updateReading(device);
			processError(readError, "Failed to get current reading");

			auto gamepadError = pollGamepad(device, &state);
			if (!gamepadError) std::cout << "Failed to get gamepad state" << std::endl;

			if (state.buttons & GameInputGamepadB) {
				run = false;
			}

			uint32_t numButtons, numAxes;
			bool* buttons;
			float* axes;
			if (pollController(device, &numButtons, &buttons, &numAxes, &axes)) {
				delete[] buttons;
				delete[] axes;
			}

			float lf = std::fmaxf(0.0f, state.leftThumbstickY - .2f) * 1.25f;
			float hf = std::fmaxf(0.0f, state.rightThumbstickY - .2f) * 1.25f;
			setRumble(device, lf, hf, state.leftTrigger, state.rightTrigger);

			
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}

	std::lock_guard lk(m);
	devices.clear();

	release();
	return 0;
}

