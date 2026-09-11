//------------------------------------------------------------------------------------------------
//! Drives the passive 3D-mesh copy of the phone screen (see ELIFE_PhoneScreenRenderComponent) by
//! running the same ELIFE_PhoneScreenShell as the in-hand menu, just without click handlers.
class ELIFE_PhoneScreenController
{
	protected ref ELIFE_PhoneScreenShell m_Shell;

	//------------------------------------------------------------------------------------------------
	void Init(notnull Widget root, notnull ELIFE_PhoneGadgetComponent phone)
	{
		m_Shell = new ELIFE_PhoneScreenShell();
		m_Shell.Init(root, phone, false);
	}

	//------------------------------------------------------------------------------------------------
	void ShowScreenState(EPhoneScreenState state)
	{
		if (m_Shell)
			m_Shell.ShowState(state);
	}

	//------------------------------------------------------------------------------------------------
	void ApplySubState(string subState)
	{
		if (m_Shell)
			m_Shell.ApplySubState(subState);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneScreenController()
	{
		if (m_Shell)
			m_Shell.Destroy();
	}
}
