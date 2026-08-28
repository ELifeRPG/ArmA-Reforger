//------------------------------------------------------------------------------------------------
//! One phone app. Hosted by either the real phone menu or the 3D-mesh screen copy.
class ELIFE_PhoneAppBase
{
	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected Widget m_wRoot;

	//------------------------------------------------------------------------------------------------
	//! initialSubState restores nav state on open, so OnOpened() shouldn't broadcast its own default.
	void Open(notnull ELIFE_PhoneGadgetComponent phone, notnull Widget host, string initialSubState = "")
	{
		m_Phone = phone;
		m_wRoot = CreateRoot(host);
		if (m_wRoot)
		{
			OnOpened();
			ApplySubState(initialSubState);
		}
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		OnClosing();

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_Phone = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Return true if Back was consumed (e.g. statement → account list).
	bool OnBack()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Override for apps with their own in-app navigation (e.g. Bank's open statement).
	string GetSubState()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Restore navigation from a GetSubState() value received from another instance of this app.
	void ApplySubState(string subState)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Call after any internal navigation change (see GetSubState()).
	protected void NotifySubStateChanged()
	{
		if (m_Phone)
			m_Phone.SetScreenSubState(GetSubState());
	}

	//------------------------------------------------------------------------------------------------
	string GetTitle()
	{
		return "#ELIFE-Item_Phone_Name";
	}

	//------------------------------------------------------------------------------------------------
	EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.HOME;
	}

	//------------------------------------------------------------------------------------------------
	protected Widget CreateRoot(notnull Widget host)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOpened()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosing()
	{
	}
}
