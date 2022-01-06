#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
using namespace ATL;

#include <vsdebugeng.h>
#include <vsdebugeng.templates.h>
using namespace Microsoft::VisualStudio::Debugger;
using namespace Microsoft::VisualStudio::Debugger::Evaluation;
#pragma comment(lib, "VSDebugEng.lib")

#include "ecs-visualizer.Contract.h"

class ATL_NO_VTABLE DefaultVisualizerService :
    public DefaultVisualizerServiceContract,
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<DefaultVisualizerService, &DefaultVisualizerServiceContract::ClassId>
{
protected:
    DefaultVisualizerService() {}
    ~DefaultVisualizerService() {}

public:
    DECLARE_NO_REGISTRY();
    DECLARE_NOT_AGGREGATABLE(DefaultVisualizerService);

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
        hr = pVisualizedExpression->EvaluateExpressionCallback(
            rawInspectionContext,
            languageExpression,
            pVisualizedExpression->StackFrame(),
            ppResultObject);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UseDefaultEvaluationBehavior(
        _In_            DkmVisualizedExpression* pVisualizedExpression,
        _Out_           bool*                    pUseDefaultEvaluationBehavior,
        _Deref_out_opt_ DkmEvaluationResult**    ppDefaultEvaluationResult
    )
    {
        HRESULT hr;

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

        hr = pVisualizedExpression->EvaluateExpressionCallback(
            rawInspectionContext,
            languageExpression,
            pVisualizedExpression->StackFrame(),
            ppDefaultEvaluationResult);
        if (FAILED(hr))
            return hr;

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

OBJECT_ENTRY_AUTO(DefaultVisualizerService::ClassId, DefaultVisualizerService)