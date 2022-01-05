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

struct TrivialIUnknown : IUnknown
{
    TrivialIUnknown()
    {
        m_refCount = 1;
    }

    // IUnknown Implementation

    volatile LONG m_refCount;

    ULONG __stdcall AddRef()
    {
        return (ULONG)InterlockedIncrement(&m_refCount);
    }

    ULONG __stdcall Release()
    {
        ULONG result = (ULONG)InterlockedDecrement(&m_refCount);
        if (result == 0)
        {
            delete this;
        }
        return result;
    }

    HRESULT __stdcall QueryInterface(REFIID riid, _Deref_out_ void** ppv)
    {
        if (riid == __uuidof(IUnknown))
        {
            *ppv = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
    }
};

struct __declspec(uuid("{8B9106D3-5262-4324-B948-66D1A2E76B26}")) ECSVisualizerDataItem : TrivialIUnknown
{
};

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

        // Duplicate the context and add ShowValueRaw to avoid recursively invoking this visualizer
        CComPtr<DkmInspectionContext> rawInspectionContext;
        hr = DkmInspectionContext::Create(
            pVisualizedExpression->InspectionContext()->InspectionSession(),
            pVisualizedExpression->InspectionContext()->RuntimeInstance(),
            pVisualizedExpression->InspectionContext()->Thread(),
            pVisualizedExpression->InspectionContext()->Timeout(),
            pVisualizedExpression->InspectionContext()->EvaluationFlags() | DkmEvaluationFlags::ShowValueRaw,
            pVisualizedExpression->InspectionContext()->FuncEvalFlags(),
            pVisualizedExpression->InspectionContext()->Radix(),
            pVisualizedExpression->InspectionContext()->Language(),
            pVisualizedExpression->InspectionContext()->ReturnValue(),
            pVisualizedExpression->InspectionContext()->AdditionalVisualizationData(),
            pVisualizedExpression->InspectionContext()->AdditionalVisualizationDataPriority(),
            pVisualizedExpression->InspectionContext()->ReturnValues(),
            pVisualizedExpression->InspectionContext()->SymbolsConnection(),
            &rawInspectionContext);
        if (FAILED(hr))
            return hr;

        // NOTE: We call back into the expression evaluator to get the default visualization
        // behavior. That means we never get invoked for children and this cast is guaranteed to
        // work.
        DkmRootVisualizedExpression* rootExpression = DkmRootVisualizedExpression::TryCast(pVisualizedExpression);

        CAutoDkmClosePtr<DkmLanguageExpression> languageExpression;
        hr = DkmLanguageExpression::Create(
            rawInspectionContext->Language(),
            rawInspectionContext->EvaluationFlags(),
            rootExpression->FullName(),
            DkmDataItem::Null(),
            &languageExpression);
        if (FAILED(hr))
            return hr;

        // NOTE: The default expression evaluator doesn't work well for functions that return arrays
        // of pointers in the Autos window. All we see is the address (e.g. {0xbaadf00d})

        // TODO: For some reason just calling back to the default expression evaluator doesn't work
        // for arrays.When we don't have a custom visualizer we see a non-expandable result that has
        // the expected one-line summary of the type (i.e. {{field1=... field2=... }}). When our
        // custom visualizer is enabled all we see is {???}. This can be seen in at least 3 cases:
        // 1) A local variable array of values in the Watch window
        // 2) A local variable array of pointers in the Watch window
        // 3) A function returning an array of values in the Autos window
        CComPtr<DkmEvaluationResult> intermediateResult;
        hr = pVisualizedExpression->EvaluateExpressionCallback(
            rawInspectionContext,
            languageExpression,
            pVisualizedExpression->StackFrame(),
            &intermediateResult);
        if (FAILED(hr))
            return hr;

        CComPtr<ECSVisualizerDataItem> data;
        data.Attach(new ECSVisualizerDataItem {});
        hr = pVisualizedExpression->SetDataItem(DkmDataCreationDisposition::CreateNew, data);
        if (FAILED(hr))
            return hr;

        DkmSuccessEvaluationResult* successIntermediateResult = DkmSuccessEvaluationResult::TryCast(intermediateResult);
        if (!successIntermediateResult)
        {
            // NOTE: This evaluation could end up returning a failed result. If it does we want to
            // return the failed result so the UI tells us something went wrong.
            *ppResultObject = intermediateResult.Detach();
            return S_OK;
        }

        CComPtr<DkmString> value;
        hr = DkmString::Create(L"Value!", &value);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmSuccessEvaluationResult> finalResult;
        hr = DkmSuccessEvaluationResult::Create(
            successIntermediateResult->InspectionContext(),
            successIntermediateResult->StackFrame(),
            successIntermediateResult->Name(),
            successIntermediateResult->FullName(),
            successIntermediateResult->Flags(),
            value,
            successIntermediateResult->EditableValue(),
            successIntermediateResult->Type(),
            successIntermediateResult->Category(),
            successIntermediateResult->Access(),
            successIntermediateResult->StorageType(),
            successIntermediateResult->TypeModifierFlags(),
            successIntermediateResult->Address(),
            successIntermediateResult->CustomUIVisualizers(),
            successIntermediateResult->ExternalModules(),
            successIntermediateResult->RefreshButtonText(),
            DkmDataItem::Null(),
            &finalResult
        );
        if (FAILED(hr))
            return hr;

        *ppResultObject = finalResult.Detach();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UseDefaultEvaluationBehavior(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _Out_           bool*                    pUseDefaultEvaluationBehavior,
        _Deref_out_opt_ DkmEvaluationResult**    ppDefaultEvaluationResult
    )
    {
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
        HRESULT hr;

        // NOTE: For some reason, trying to re-use the result from EvaluateVisualizedExpression (by storing
        // it in a data item and retrieving it) doesn't work. It recurses infinitely when we call
        // GetChildrenCallback.

        ECSVisualizerDataItem* data;
        hr = pVisualizedExpression->GetDataItem(&data);
        if (FAILED(hr))
            return hr;

        DkmRootVisualizedExpression* rootExpression = DkmRootVisualizedExpression::TryCast(pVisualizedExpression);

        CAutoDkmClosePtr<DkmLanguageExpression> languageExpression;
        hr = DkmLanguageExpression::Create(
            pInspectionContext->Language(),
            pInspectionContext->EvaluationFlags(),
            rootExpression->FullName(),
            DkmDataItem::Null(),
            &languageExpression);
        if (FAILED(hr))
            return hr;

        CComPtr<DkmEvaluationResult> rootEvalResult;
        hr = pVisualizedExpression->EvaluateExpressionCallback(
            pInspectionContext,
            languageExpression,
            pVisualizedExpression->StackFrame(),
            &rootEvalResult);
        if (FAILED(hr))
            return hr;

        // TODO: DkmFreeArray will release interfaces, but I don't think it closes the Dkm objects
        CAutoDkmArray<DkmEvaluationResult*> defaultInitialChildren;
        hr = pVisualizedExpression->GetChildrenCallback(
            rootEvalResult,
            InitialRequestSize,
            pInspectionContext,
            &defaultInitialChildren,
            ppEnumContext
        );
        if (FAILED(hr))
            return hr;

        // NOTE: It looks like we always get 0 and setting it to 1 is ignored by the default
        // expression evaluator. Just assume that's always the case so we can keep all the logic
        // in GetItems.
        if (InitialRequestSize != 0)
            return E_UNEXPECTED;

        if (defaultInitialChildren.Length != 0)
            return E_UNEXPECTED;

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

        ECSVisualizerDataItem* data;
        hr = pVisualizedExpression->GetDataItem(&data);
        if (FAILED(hr))
            return hr;

        CAutoDkmArray<DkmEvaluationResult*> defaultChildEvals;
        hr = pVisualizedExpression->GetItemsCallback(
            pEnumContext,
            StartIndex,
            Count,
            &defaultChildEvals
        );
        if (FAILED(hr))
            return hr;

        DWORD childCount = defaultChildEvals.Length;
        hr = DkmAllocArray(childCount, pItems);
        if (FAILED(hr))
            return hr;

        // NOTE: This seems like the easiest way to handle partial failures. If we attempt to return
        // a failure code from the middle of the loop the we'll technically leak the results we were
        // already able to add to it. If we return a success code and leave Length set to the amount
        // we allocated space for the debugger will blindly dereference the remaining null elements
        // and crash the debugging session. Instead, we set the length accurately, ignore failure
        // codes from DkmChildVisualizedExpression::Create, and always return success. This means
        // the debugger will show whichever results we managed to make, a red error icon for
        // those we didn't, and we won't leak anything. This is at the expense of swallowing error
        // codes, but the UI doesn't display them anyway.
        //
        // Many (all?) Dkm objects are automatically closed when the debugging session ends. But I
        // don't if that releases the COM interfaces and actually allows the objects to be
        // destroyed. We'd also leak objects during session even if it does, so I'd rather just be a
        // little safer here.
        pItems->Length = 0;

        for (DWORD i = 0; i < childCount; i++)
        {
            DkmEvaluationResult* defaultChildEval = defaultChildEvals.Members[i];
            DkmChildVisualizedExpression** defaultChildVis = &pItems->Members[i];

            CComPtr<DkmExpressionValueHome> defaultChildHome;
            DkmSuccessEvaluationResult* defaultChildEvalSuccess = DkmSuccessEvaluationResult::TryCast(defaultChildEval);
            if (defaultChildEvalSuccess)
            {
                DkmPointerValueHome* realHome;
                hr = DkmPointerValueHome::Create(defaultChildEvalSuccess->Address()->Value(), &realHome);
                defaultChildHome.Attach(realHome);
            }
            else
            {
                DkmFakeValueHome* fakeHome;
                hr = DkmFakeValueHome::Create(0, &fakeHome);
                defaultChildHome.Attach(fakeHome);
            }
            if (FAILED(hr))
                continue;

            hr = DkmChildVisualizedExpression::Create(
                defaultChildEval->InspectionContext(),
                pVisualizedExpression->VisualizerId(),
                pVisualizedExpression->SourceId(),
                pVisualizedExpression->StackFrame(),
                defaultChildHome,
                defaultChildEval,
                pVisualizedExpression,
                i,
                DkmDataItem::Null(),
                defaultChildVis
            );
            if (FAILED(hr))
                continue;

            pItems->Length++;
        }

        return S_OK;
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
