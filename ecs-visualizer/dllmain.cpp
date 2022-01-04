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
#include <typeinfo>

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

        DkmArray<BYTE> componentType;
        targetProcess->ReadMemoryString(
            size_t(archetype.componentType),
            DkmReadMemoryFlags::None,
            sizeof(char),
            1024,
            &componentType
         );

        CComPtr<DkmString> value;
        CString rawValue = {};
        rawValue.Format(L"Count: %d { %S }", archetype.entityCount, componentType.Members);
        hr = DkmString::Create(rawValue, &value);
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
            nullptr,
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

        *ppResultObject = result.Detach();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UseDefaultEvaluationBehavior(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _Out_           bool*                    pUseDefaultEvaluationBehavior,
        _Deref_out_opt_ DkmEvaluationResult**    ppDefaultEvaluationResult
    )
    {
        auto rootExpression = DkmRootVisualizedExpression::TryCast(pVisualizedExpression);
        if (!rootExpression)
            return E_NOTIMPL;

        *pUseDefaultEvaluationBehavior = false;
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
        // NOTE: Just ignore the request size. This isn't an array so we don't have any large
        // memory concerns here.

        HRESULT hr;

        CComPtr<DkmEvaluationResultEnumContext> enumContext;
        hr = DkmEvaluationResultEnumContext::Create(
            1,
            pVisualizedExpression->StackFrame(),
            pVisualizedExpression->InspectionContext(),
            DkmDataItem::Null(),
            &enumContext
        );
        if (FAILED(hr))
            return hr;

        *ppEnumContext = enumContext.Detach();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetItems(
        _In_  DkmVisualizedExpression*                 pVisualizedExpression,
        _In_  DkmEvaluationResultEnumContext*          pEnumContext,
        _In_  UINT32                                   StartIndex,
        _In_  UINT32                                   Count,
        _Out_ DkmArray<DkmChildVisualizedExpression*>* pItems
    )
    {
        HRESULT hr;

        if (StartIndex != 0 || Count != 1)
            return E_UNEXPECTED;

        hr = DkmAllocArray(1, pItems);
        if (FAILED(hr))
            return hr;

        DkmVisualizedExpression* parentExpression = pVisualizedExpression;
        DkmInspectionContext* parentContext = parentExpression->InspectionContext();

        DkmPointerValueHome* parentValueHome = DkmPointerValueHome::TryCast(parentExpression->ValueHome());
        if (!parentValueHome)
            return E_UNEXPECTED;


        CComPtr<DkmString> childExpressionText;
        hr = DkmString::Create(L"entityCount", &childExpressionText);
        if (FAILED(hr))
            return hr;

        DkmEvaluationFlags_t flags = DkmEvaluationFlags::TreatAsExpression;
        CComPtr<DkmLanguageExpression> childExpression;
        hr = DkmLanguageExpression::Create(
            parentContext->Language(),
            flags,
            childExpressionText,
            DkmDataItem::Null(),
            &childExpression
        );
        if (FAILED(hr))
            return hr;

        CComPtr<DkmInspectionContext> childContext;
        hr = DkmInspectionContext::Create(
            parentContext->InspectionSession(),
            parentContext->RuntimeInstance(),
            parentContext->Thread(),
            parentContext->Timeout(),
            flags,
            parentContext->FuncEvalFlags(),
            parentContext->Radix(),
            parentContext->Language(),
            parentContext->ReturnValue(),
            &childContext
        );
        if (FAILED(hr))
            return hr;

        CComPtr<DkmEvaluationResult> childResult;
        hr = parentExpression->EvaluateExpressionCallback(
            childContext,
            childExpression,
            parentExpression->StackFrame(),
            &childResult
        );
        if (FAILED(hr))
            return hr;

        auto childSuccessResult = DkmSuccessEvaluationResult::TryCast(childResult);
        if (!childSuccessResult)
            return E_FAIL;

        size_t childAddress = childSuccessResult->Address()->Value();
        CComPtr<DkmPointerValueHome> childHome;
        hr = DkmPointerValueHome::Create(childAddress, &childHome);
        if (FAILED(hr))
            return hr;

        DkmString* displayValue = childSuccessResult->Value();
        DkmString* displayName = childExpressionText;

        auto& type = typeid(Archetype::entityCount);
        CString rawDisplayType;
        rawDisplayType.Format(L"%S", type.name());
        CComPtr<DkmString> displayType;
        hr = DkmString::Create(rawDisplayType, &displayType);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmSuccessEvaluationResult> childDisplaySuccessResult;
        hr = DkmSuccessEvaluationResult::Create(
            parentContext,
            parentExpression->StackFrame(),
            displayName,
            childSuccessResult->FullName(),
            childSuccessResult->Flags(),
            displayValue,
            nullptr,
            displayType,
            childSuccessResult->Category(),
            childSuccessResult->Access(),
            childSuccessResult->StorageType(),
            childSuccessResult->TypeModifierFlags(),
            childSuccessResult->Address(),
            childSuccessResult->CustomUIVisualizers(),
            childSuccessResult->ExternalModules(),
            DkmDataItem::Null(),
            &childDisplaySuccessResult
        );
        if (FAILED(hr))
            return hr;

        CComPtr<DkmChildVisualizedExpression> childDisplayVisExpression;
        hr = DkmChildVisualizedExpression::Create(
            parentContext,
            parentExpression->VisualizerId(),
            parentExpression->SourceId(),
            parentExpression->StackFrame(),
            childHome,
            childDisplaySuccessResult,
            parentExpression,
            0,
            DkmDataItem::Null(),
            &childDisplayVisExpression
        );
        if (FAILED(hr))
            return hr;

        pItems->Members[0] = childDisplayVisExpression.Detach();
        return S_OK;

#if false
        CComPtr<DkmPointerValueHome> entityCountValueHome;
        hr = DkmPointerValueHome::Create(
            archetypeValueHome->Address() + offsetof(Archetype, entityCount),
            &entityCountValueHome
        );
        if (FAILED(hr))
            return hr;

        auto rootExpression = DkmRootVisualizedExpression::TryCast(pVisualizedExpression);
        if (!rootExpression)
            return E_NOTIMPL;

        CAutoDkmClosePtr<DkmLanguageExpression> languageExpression;
        hr = DkmLanguageExpression::Create(
            pVisualizedExpression->InspectionContext()->Language(),
            DkmEvaluationFlags::TreatAsExpression,
            rootExpression->FullName(),
            DkmDataItem::Null(),
            &languageExpression
        );
        if (FAILED(hr))
            return hr;

        CComPtr<DkmEvaluationResult> entityCountResult;
        pVisualizedExpression->EvaluateExpressionCallback(
            pVisualizedExpression->InspectionContext(),
            languageExpression,
            pVisualizedExpression->StackFrame(),
            &entityCountResult
        );
        if (FAILED(hr))
            return hr;

        hr = DkmChildVisualizedExpression::Create(
            pVisualizedExpression->InspectionContext(),
            pVisualizedExpression->VisualizerId(),
            pVisualizedExpression->SourceId(),
            pVisualizedExpression->StackFrame(),
            entityCountValueHome,
            entityCountResult.p,
            pVisualizedExpression,
            0,
            DkmDataItem::Null(),
            &pItems->Members[0]
        );
        if (FAILED(hr))
            return hr;
#endif
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
