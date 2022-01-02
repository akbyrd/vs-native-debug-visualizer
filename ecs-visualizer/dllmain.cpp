// -------------------------------------------------------------------------------------------------
// Visual Studio Extensibility

#define _ATL_FREE_THREADED

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
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

// -------------------------------------------------------------------------------------------------
// Concord Extensibility

#include <vsdebugeng.h>
#include <vsdebugeng.templates.h>
using namespace Microsoft::VisualStudio::Debugger;
using namespace Microsoft::VisualStudio::Debugger::Evaluation;
#pragma comment(lib, "VSDebugEng.lib")

#include "ecs-visualizer.Contract.h"
#include "types.h"

class ATL_NO_VTABLE ECSVisualizerService :
    public ECSVisualizerServiceContract,
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<ECSVisualizerService, &ECSVisualizerServiceContract::ClassId>
{
protected:
    ECSVisualizerService() {}
    ~ECSVisualizerService() {}

public:
    DECLARE_NO_REGISTRY();
    DECLARE_NOT_AGGREGATABLE(ECSVisualizerService);

    HRESULT STDMETHODCALLTYPE EvaluateVisualizedExpression(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _Deref_out_opt_ DkmEvaluationResult**    ppResultObject
    )
    {
        HRESULT hr;

        // NOTE: Returning E_NOTIMPL is equivalent to falling back to the default behavior.

        // NOTE: If this cast works the debugged value exists in the debuggee processes memory.
        // I don't know which cases exists where this wouldn't be true.
        auto pointerValueHome = DkmPointerValueHome::TryCast(pVisualizedExpression->ValueHome());
        if (!pointerValueHome)
            return E_NOTIMPL;

        // TODO: ????
        auto rootExpression = DkmRootVisualizedExpression::TryCast(pVisualizedExpression);
        if (!rootExpression)
            return E_NOTIMPL;

        DkmRuntimeInstance* runtimeInstance = pVisualizedExpression->RuntimeInstance();
        DkmProcess* targetProcess = runtimeInstance->Process();

        Archetype archetype;
        hr = targetProcess->ReadMemory(
            pointerValueHome->Address(),
            DkmReadMemoryFlags::None,
            &archetype, sizeof(Archetype),
            nullptr);
        if (FAILED(hr))
            return E_NOTIMPL;

        CComPtr<DkmString> value;
        hr = DkmString::Create(CString("Value!"), &value);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmString> editableValue;
        hr = DkmString::Create(CString("Editable Value!"), &editableValue);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmDataAddress> address;
        hr = DkmDataAddress::Create(
            runtimeInstance,
            pointerValueHome->Address(),
            nullptr,
            &address);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmSuccessEvaluationResult> result;
        hr = DkmSuccessEvaluationResult::Create(
            pVisualizedExpression->InspectionContext(),
            pVisualizedExpression->StackFrame(),
            rootExpression->Name(),
            rootExpression->FullName(),
            DkmEvaluationResultFlags::Expandable,
            value,
            editableValue,
            rootExpression->Type(),
            DkmEvaluationResultCategory::Class,
            DkmEvaluationResultAccessType::None,
            DkmEvaluationResultStorageType::None,
            DkmEvaluationResultTypeModifierFlags::None,
            address,
            nullptr,
            nullptr,
            DkmDataItem::Null(),
            &result);
        if (FAILED(hr))
            return hr;

        //*ppResultObject = result.Detach();

        DkmInspectionContext* context = pVisualizedExpression->InspectionContext();

        CAutoDkmClosePtr<DkmLanguageExpression> expression;
        hr = DkmLanguageExpression::Create(
            context->Language(),
            DkmEvaluationFlags::TreatAsExpression,
            rootExpression->FullName(),
            DkmDataItem::Null(),
            &expression);
        if (FAILED(hr))
            return hr;

        pVisualizedExpression->EvaluateExpressionCallback(
            context,
            expression,
            pVisualizedExpression->StackFrame(),
            ppResultObject
        );
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UseDefaultEvaluationBehavior(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _Out_           bool*                    pUseDefaultEvaluationBehavior,
        _Deref_out_opt_ DkmEvaluationResult**    ppDefaultEvaluationResult
    )
    {
        HRESULT hr;

        auto rootExpression = DkmRootVisualizedExpression::TryCast(pVisualizedExpression);
        if (!rootExpression)
            return E_NOTIMPL;

        DkmInspectionContext* context = pVisualizedExpression->InspectionContext();
        CAutoDkmClosePtr<DkmLanguageExpression> expression;
        hr = DkmLanguageExpression::Create(
            context->Language(),
            DkmEvaluationFlags::TreatAsExpression,
            rootExpression->FullName(),
            DkmDataItem::Null(),
            &expression);
        if (FAILED(hr))
            return hr;

        // Duplicate the context and add ShowValueRaw to avoid recursively invoking this visualizer
        CComPtr<DkmInspectionContext> newContext;
        hr = DkmInspectionContext::Create(
            context->InspectionSession(),
            context->RuntimeInstance(),
            context->Thread(),
            context->Timeout(),
            context->EvaluationFlags() | DkmEvaluationFlags::ShowValueRaw,
            context->FuncEvalFlags(),
            context->Radix(),
            context->Language(),
            context->ReturnValue(),
            context->AdditionalVisualizationData(),
            context->AdditionalVisualizationDataPriority(),
            context->ReturnValues(),
            context->SymbolsConnection(),
            &newContext);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmEvaluationResult> result;
        hr = pVisualizedExpression->EvaluateExpressionCallback(
            newContext,
            expression,
            pVisualizedExpression->StackFrame(),
            &result);
        if (FAILED(hr))
            return hr;

        *ppDefaultEvaluationResult = result.Detach();
        *pUseDefaultEvaluationBehavior = true;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetChildren(
        _In_        DkmVisualizedExpression*                 pVisualizedExpression,
        _In_        UINT32                                   InitialRequestSize,
        _In_        DkmInspectionContext*                    pInspectionContext,
        _Out_       DkmArray<DkmChildVisualizedExpression*>* pInitialChildren,
        _Deref_out_ DkmEvaluationResultEnumContext**         ppEnumContext
    )
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetItems(
        _In_  DkmVisualizedExpression*                 pVisualizedExpression,
        _In_  DkmEvaluationResultEnumContext*          pEnumContext,
        _In_  UINT32                                   StartIndex,
        _In_  UINT32                                   Count,
        _Out_ DkmArray<DkmChildVisualizedExpression*>* pItems
    )
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetValueAsString(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _In_            DkmString*               pValue,
        _In_            UINT32                   Timeout,
        _Deref_out_opt_ DkmString**              ppErrorText
    )
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetUnderlyingString(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _Deref_out_opt_ DkmString**              ppStringValue
    )
    {
        return E_NOTIMPL;
    }
};

OBJECT_ENTRY_AUTO(ECSVisualizerService::ClassId, ECSVisualizerService)
