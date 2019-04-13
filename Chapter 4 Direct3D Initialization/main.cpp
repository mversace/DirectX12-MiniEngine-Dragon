#include "GameApp.h"

int WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nShowCmd )
{
	GameApp* app = new GameApp();
	GameCore::RunApplication(*app, hInstance, L"CrossGate");
	delete app;
	return 0;
}