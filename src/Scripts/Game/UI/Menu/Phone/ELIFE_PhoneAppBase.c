//------------------------------------------------------------------------------------------------
//! One phone app living inside ELIFE_PhoneMenu. Not a separate ChimeraMenu.
class ELIFE_PhoneAppBase
{
	protected ELIFE_PhoneMenu m_Menu;
	protected Widget m_wRoot;

	//------------------------------------------------------------------------------------------------
	void Open(notnull ELIFE_PhoneMenu menu, notnull Widget host)
	{
		m_Menu = menu;
		m_wRoot = CreateRoot(host);
		if (m_wRoot)
			OnOpened();
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

		m_Menu = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Return true if Back was consumed (e.g. statement → account list).
	bool OnBack()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	string GetTitle()
	{
		return "#ELIFE-Item_Phone_Name";
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
