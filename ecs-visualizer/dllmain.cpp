// -------------------------------------------------------------------------------------------------
// Visual Studio Extensibility

#define _ATL_FREE_THREADED

#include <atlbase.h>
using namespace ATL;

class ECSVisualizerModule : public CAtlDllModuleT<ECSVisualizerModule> {};
extern class ECSVisualizerModule _AtlModule;
ECSVisualizerModule _AtlModule;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	hInstance;
	return _AtlModule.DllMain(dwReason, lpReserved);
}

STDAPI DllCanUnloadNow(void)
{
	return _AtlModule.DllCanUnloadNow();
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}
