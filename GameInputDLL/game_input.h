#pragma once

#include <GameInput.h>
#include <functional>

using namespace GameInput::v3;

struct Controller {
	IGameInputDevice* device;
	IGameInputReading* lastReading = nullptr;
	const GameInputDeviceInfo* info = nullptr;
};

typedef void(CALLBACK* ControllerConnectCallback)(
	_In_ Controller* controller
);
typedef void(CALLBACK* ControllerDisconnectCallback)(
	_In_ IGameInputDevice* device
);

HRESULT initInput();

void CALLBACK onDeviceEnumerated(
	_In_ GameInputCallbackToken callbackToken,
	_In_ void* ctx,
	_In_ IGameInputDevice* device,
	_In_ uint64_t timestamp,
	_In_ GameInputDeviceStatus currStatus,
	_In_ GameInputDeviceStatus prevStatus
);

void setConnectCallback(void (CALLBACK* cb)(Controller* controller));
void setDisconnectCallback(void (CALLBACK* cb)(IGameInputDevice* device));

HRESULT updateReading(Controller* controller);
bool pollGamepad(Controller* controller, GameInputGamepadState* state);
bool pollController(Controller* controller, uint32_t* numButtons, bool** buttons, uint32_t* numAxes, float** axes);

void setRumble(Controller* controller, float lowFrequency, float highFrequency, float leftTrigger, float rightTrigger);

void releaseController(Controller* controller);
void releaseInput();