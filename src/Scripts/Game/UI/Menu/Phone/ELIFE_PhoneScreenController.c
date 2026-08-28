//------------------------------------------------------------------------------------------------
//! Drives the passive 3D-mesh copy of the phone screen (see ELIFE_PhoneScreenRenderComponent),
//! mirroring ELIFE_PhoneMenu 1:1 by instantiating the same real app pages into its own AppHost.
class ELIFE_PhoneScreenController
{
	protected Widget m_wHomeGrid;
	protected Widget m_wAppHost;
	protected Widget m_wNavSize;
	protected TextWidget m_wStatusBar;
	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected ref ELIFE_PhoneAppBase m_App;
	protected EPhoneScreenState m_eOpenAppState;

	//------------------------------------------------------------------------------------------------
	void Init(notnull Widget root, notnull ELIFE_PhoneGadgetComponent phone)
	{
		m_wHomeGrid = root.FindAnyWidget("HomeGrid");
		m_wAppHost = root.FindAnyWidget("AppHost");
		m_wNavSize = root.FindAnyWidget("NavSize");
		m_wStatusBar = TextWidget.Cast(root.FindAnyWidget("StatusBar"));
		m_Phone = phone;
	}

	//------------------------------------------------------------------------------------------------
	void ShowScreenState(EPhoneScreenState state)
	{
		bool appOpen = state != EPhoneScreenState.HOME && state != EPhoneScreenState.OFF && state != EPhoneScreenState.LOCKED;

		if (m_wHomeGrid)
			m_wHomeGrid.SetVisible(!appOpen);

		if (m_wAppHost)
			m_wAppHost.SetVisible(appOpen);

		if (!m_wStatusBar)
			return;

		switch (state)
		{
			case EPhoneScreenState.BANK:
				m_wStatusBar.SetText("#ELIFE-Phone_App_Bank");
				OpenApp(state, new ELIFE_PhoneBankingApp());
				break;
			case EPhoneScreenState.SETTINGS:
				m_wStatusBar.SetText("#ELIFE-Phone_App_Settings");
				OpenApp(state, new ELIFE_PhoneSettingsApp());
				break;
			case EPhoneScreenState.MESSAGES:
				//! No real app page for this state - ELIFE_PhoneMenu just shows a hint here too.
				m_wStatusBar.SetText("#ELIFE-Phone_App_Messages");
				CloseApp();
				break;
			case EPhoneScreenState.MAP:
				//! ELIFE_PhoneMenu opens a separate map menu for this state - no AppHost content.
				m_wStatusBar.SetText("#ELIFE-Phone_App_Map");
				CloseApp();
				break;
			default:
				m_wStatusBar.SetText("Home");
				CloseApp();
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenApp(EPhoneScreenState state, notnull ELIFE_PhoneAppBase app)
	{
		if (m_eOpenAppState == state && m_App)
			return;

		CloseApp();

		if (!m_wAppHost || !m_Phone)
			return;

		m_App = app;

		//! Passes the current sub-state into Open() so a fresh instance builds directly into the
		//! right navigation state without briefly broadcasting a wrong default first.
		m_App.Open(m_Phone, m_wAppHost, m_Phone.GetScreenSubState());
		m_eOpenAppState = state;

		if (m_wNavSize)
			m_wNavSize.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	void ApplySubState(string subState)
	{
		if (m_App)
			m_App.ApplySubState(subState);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseApp()
	{
		if (m_App)
		{
			m_App.Close();
			m_App = null;
		}

		m_eOpenAppState = EPhoneScreenState.OFF;

		if (m_wNavSize)
			m_wNavSize.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneScreenController()
	{
		CloseApp();
	}
}
