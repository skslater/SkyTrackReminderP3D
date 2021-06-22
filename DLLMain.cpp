#include "initpdk.h"
#include <WinError.h>
#include "PdkPlugin.h"
#include <string>

constexpr auto MENU_TITLE = L"SkyTrack Reminder";
constexpr auto MENU_ITEM_ACKNOWLEDGE = L"Hide Reminder";
constexpr auto REMINDER_TEXT = L"*** START SKYTRACK ***";

using namespace P3D;

class SkyTrackReminderP3D : public PdkPlugin
{
    ///----------------------------------------------------------------------------
    ///  Public Access
    ///----------------------------------------------------------------------------
public:

    //
    // Constructor
    //
    SkyTrackReminderP3D() : PdkPlugin()
    {
        m_uOneSecondTick = 0;
        m_bShouldDisplay = false;
        m_bAcknowledged = false;

        m_spMenuTop.Attach(P3D::PdkServices::GetMenuService()->CreateMenuItem());
        m_spMenuTop->SetType(P3D::MenuTypePdk::MENU_ITEM);
        m_spMenuTop->SetText(MENU_TITLE);
        PdkServices::GetMenuService()->AddItem(m_spMenuTop->GetId(), NO_PARENT, 0);

        m_spMenuAcknowledge.Attach(P3D::PdkServices::GetMenuService()->CreateMenuItem());
        m_spMenuAcknowledge->SetType(P3D::MenuTypePdk::MENU_CHECK_ITEM);
        m_spMenuAcknowledge->SetText(MENU_ITEM_ACKNOWLEDGE);
        m_spMenuAcknowledge->SetChecked(m_bAcknowledged);
        SkyTrackReminderP3D::MenuCallback* callback1 = new SkyTrackReminderP3D::MenuCallback(ACKNOWLEDGE_REMINDER);
        m_spMenuAcknowledge->RegisterCallback(callback1);
        PdkServices::GetMenuService()->AddItem(m_spMenuAcknowledge->GetId(), m_spMenuTop->GetId(), 0);
    }

    //
    // Update member variables based on aircraft state
    //
    void CheckAircraftStatus(IPanelSystemV400* spPanelSystem)
    {
        ENUM eVarNumEngines = spPanelSystem->GetAircraftVarEnum("NUMBER OF ENGINES");
        ENUM eVarCombustion = spPanelSystem->GetAircraftVarEnum("GENERAL ENG COMBUSTION");
        ENUM eVarBatteryOn = spPanelSystem->GetAircraftVarEnum("ELECTRICAL MASTER BATTERY");
        ENUM eUnitNumber = spPanelSystem->GetUnitsEnum("Number");
        ENUM eUnitBool = spPanelSystem->GetUnitsEnum("Bool");

        // Is the main battery on?
        int batteryOn = (int)spPanelSystem->AircraftVarget(eVarBatteryOn, eUnitNumber, 1);

        // Check each engine and count up how many are running
        int numEngines = (int)spPanelSystem->AircraftVarget(eVarNumEngines, eUnitNumber, 1);
        int enginesRunning = 0;

        for (int i = 1; i <= numEngines; i++)
        {
            enginesRunning += (int)spPanelSystem->AircraftVarget(eVarCombustion, eUnitBool, i);
        }

        m_bShouldDisplay = (enginesRunning == 0) && batteryOn;
    }

    //
    // Called per frame for redraw
    //
    virtual void OnCustomRender(IParameterListV400* pParams) override
    {
        RenderFlags renderFlags;
        renderFlags.DrawFromBase = true;
        renderFlags.DrawWithVC = true;

        TextDescription textDescription;
        textDescription.CalculateBox = true;
        textDescription.DisplayOnTop = true;
        textDescription.Font = TEXT_FONT_DEFAULT;

        CComPtr<IObjectRendererV440>    spRenderService = NULL;

        // Query the COM interface for an object renderer that we can use to draw the text on the current view
        HRESULT hr = pParams->GetServiceProvider()->QueryService(SID_ObjectRenderer, IID_IObjectRendererV440, (void**)&spRenderService);

        if (SUCCEEDED(hr))
        {
            if (m_bShouldDisplay && !m_bAcknowledged)
            {
                // If this is the main window calculate the centre of the screen so
                // we can put the message there. This isn't exact centre because P3D doesn't
                // seem to have a function to measure pixel width of a text string :(
                IWindowV400* pWindow = PdkServices::GetWindowPluginSystem()->GetCurrentWindow();
                int xOffset = 200;
                int yOffset = 500;
                if (pWindow != nullptr && pWindow->IsMainAppWindow())
                {
                    unsigned int w, h;
                    pWindow->GetSize(w, h);
                    xOffset = (w - 200) / 2;
                    yOffset = (h - 50) / 2;
                }

                // Put the message on the screen with a drop shadow, alternating colour every second
                ARGBColor col = (m_uOneSecondTick & 2) == 2 ? ARGBColor(255, 255, 0, 0) : ARGBColor(255, 255, 255, 0);
                spRenderService->DrawText2D(xOffset, yOffset, REMINDER_TEXT, col, textDescription, renderFlags);
                spRenderService->DrawText2D(xOffset + 1, yOffset - 1, REMINDER_TEXT, ARGBColor(255, 0, 0, 0), textDescription, renderFlags);
            }
        }

    }

    //
    // Tick function called every 1 second
    //
    virtual void OnOneHz(IParameterListV400* pParams) override
    {
        CComPtr<IPanelSystemV400> spPanelSystem;

        // Increment seconds counter to allow colour switching
        m_uOneSecondTick++;

        // Query the COM interface for the P3D panel system so we can get some useful
        // info from a few simvars to decide if a reminder is needed
        HRESULT hr = pParams->GetServiceProvider()->QueryService(SID_PanelSystem, IID_IPanelSystemV400, (void**)&spPanelSystem);
        if (SUCCEEDED(hr))
        {
            CheckAircraftStatus(spPanelSystem);
        }
    }

    //
    // If the aircraft is changed we should reset
    //
    virtual void OnVehicleChanged(IParameterListV400* pParams) override
    {
        m_bAcknowledged = false;
    }


    ///----------------------------------------------------------------------------
    ///  Protected Access
    ///----------------------------------------------------------------------------
protected:

    int m_uOneSecondTick;
    bool m_bShouldDisplay;
    bool m_bAcknowledged;

    CComPtr<P3D::IMenuItemV410> m_spMenuTop;
    CComPtr<P3D::IMenuItemV410> m_spMenuAcknowledge;

    enum CALLBACK_IDS { ACKNOWLEDGE_REMINDER };

    ///----------------------------------------------------------------------------
    ///  Private Access
    ///----------------------------------------------------------------------------
private:

    class MenuCallback : public P3D::ICallbackV400
    {
    public:

        // Which menu item this callback handles
        CALLBACK_IDS m_EventID;

        //
        // Constructor
        //
        MenuCallback(CALLBACK_IDS eventID) : m_EventID(eventID), m_RefCount(1) {}

        //
        // Basic COM handling
        //
        DEFAULT_REFCOUNT_INLINE_IMPL();
        STDMETHODIMP QueryInterface(REFIID riid, PVOID* ppv)
        {
            HRESULT hr = E_NOINTERFACE;

            if (ppv == NULL)
            {
                return E_POINTER;
            }

            *ppv = NULL;

            if (IsEqualIID(riid, P3D::IID_ICallbackV400))
            {
                *ppv = static_cast<P3D::ICallbackV400*>(this);
            }
            else if (IsEqualIID(riid, IID_IUnknown))
            {
                *ppv = static_cast<IUnknown*>(this);
            }
            if (*ppv)
            {
                hr = S_OK;
                AddRef();
            }

            return hr;
        };

        //
        // Callback handler (defined later to avoid scoping issues)
        //
        virtual void Invoke(IParameterListV400* pParams) override;

    };

};

    ///----------------------------------------------------------------------------
    /// Prepar3D DLL start and end entry points
    ///----------------------------------------------------------------------------
    
    static SkyTrackReminderP3D* s_pSkyTrackReminderPlugin = nullptr;

    void __stdcall DLLStart(__in __notnull IPdk* pPdk)
    {
        PdkServices::Init(pPdk);
        s_pSkyTrackReminderPlugin = new SkyTrackReminderP3D();
    }

    void __stdcall DLLStop(void)
    {
        if (s_pSkyTrackReminderPlugin != nullptr)
        {
            delete s_pSkyTrackReminderPlugin;
        }
        PdkServices::Shutdown();
    }

    //
    // Override the menu handler Invoke method to handle menu item selection.
    // We define this here as the main plugin variable isn't in scope until this point
    // and i'm too lazy to refactor things into a header file...
    //
    void SkyTrackReminderP3D::MenuCallback::Invoke(IParameterListV400* pParams)
    {
        switch (m_EventID)
        {
            case ACKNOWLEDGE_REMINDER:
                s_pSkyTrackReminderPlugin->m_bAcknowledged = !s_pSkyTrackReminderPlugin->m_bAcknowledged;
                s_pSkyTrackReminderPlugin->m_spMenuAcknowledge->SetChecked(s_pSkyTrackReminderPlugin->m_bAcknowledged);
                break;
            default:
                break;
        }
    };