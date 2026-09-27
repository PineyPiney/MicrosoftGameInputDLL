#pragma once

#include "pch.h"
#include "game_input.h"

#include <mutex>

IGameInput* input;
GameInputCallbackToken deviceCallbackToken;

std::function<void(Controller*)> connectCb = {};
std::function<void(IGameInputDevice*)> disconnectCb = {};

HRESULT initInput() {
	HRESULT i = GameInputCreate(&input);
	if (i < 0) return i;

	input->SetFocusPolicy(GameInputEnableBackgroundInput | GameInputEnableBackgroundGuideButton | GameInputEnableBackgroundShareButton);

	return input->RegisterDeviceCallback(
		nullptr,
		GameInputKindGamepad | GameInputKindController,
		GameInputDeviceAnyStatus,
		GameInputAsyncEnumeration,
		nullptr,
		onDeviceEnumerated,
		&deviceCallbackToken
	);
}

void CALLBACK onDeviceEnumerated(
	_In_ GameInputCallbackToken callbackToken,
	_In_ void* ctx,
	_In_ IGameInputDevice* device,
	_In_ uint64_t timestamp,
	_In_ GameInputDeviceStatus currStatus,
	_In_ GameInputDeviceStatus prevStatus
) {

	switch (currStatus) {
	case 0: {
		disconnectCb(device);
		break;
	}
	case 1: {
		// When device is disconnected it first switches from HapticInfoReady to Connected, 
		// don't add new controller for that callback
		if (prevStatus != GameInputDeviceNoStatus) break;
		Controller* controller = new  Controller { device };
		HRESULT infoResult = device->GetDeviceInfo(&controller->info);
		connectCb(controller);
	}
	}
}

void setConnectCallback(const std::function<void(Controller*)>& cb) {
	connectCb = cb;
}
void setDisconnectCallback(const std::function<void(IGameInputDevice*)>& cb) {
	disconnectCb = cb;
}

HRESULT updateReading(Controller* controller) {
	if (controller->lastReading) {
		controller->lastReading->Release();
		controller->lastReading = nullptr;
	}
	return input->GetCurrentReading(controller->info->supportedInput, controller->device, &controller->lastReading);
}

bool pollGamepad(Controller* controller, GameInputGamepadState* state) {
	return controller->lastReading && controller->lastReading->GetGamepadState(state);
}

bool pollController(Controller* controller, uint32_t* numButtons, bool** buttons, uint32_t* numAxes, float** axes) {

	if (!controller->lastReading) return false;
	
	*numButtons = controller->lastReading->GetControllerButtonCount();
	*buttons = new bool[*numButtons];
	uint32_t controllerButtonSize = controller->lastReading->GetControllerButtonState(*numButtons, *buttons);
	
	*numAxes = controller->lastReading->GetControllerAxisCount();
	*axes = new float[*numAxes];
	uint32_t controllerAxesSize = controller->lastReading->GetControllerAxisState(*numAxes, *axes);

	return true;
}

void setRumble(Controller* controller, float lowFrequency, float highFrequency, float leftTrigger, float rightTrigger) {
	GameInputRumbleParams rumble { lowFrequency, highFrequency, leftTrigger, rightTrigger };
	controller->device->SetRumbleState(&rumble);
}

void releaseController(Controller* controller) {
	setRumble(controller, 0, 0, 0, 0);
	//if (controller->lastReading) {
	//	controller->lastReading->Release();
	//	controller->lastReading = nullptr;
	//}
	controller->device->Release();
	delete controller;
}

void releaseInput() {
	input->UnregisterCallback(deviceCallbackToken);
	input->Release();
}