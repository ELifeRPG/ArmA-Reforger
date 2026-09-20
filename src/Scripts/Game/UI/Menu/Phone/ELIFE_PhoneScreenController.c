//------------------------------------------------------------------------------------------------
//! Owns the shell painted onto the phone model. Only the local owner's shell is interactive and acts on
//! navigation; bystanders get a read-only one.
class ELIFE_PhoneScreenController
{
	protected ref ELIFE_PhoneScreenShell m_Shell;
	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected bool m_bInteractive;

	//------------------------------------------------------------------------------------------------
	void Init(notnull Widget root, notnull ELIFE_PhoneGadgetComponent phone)
	{
		m_Phone = phone;
		m_bInteractive = phone.IsLocalCharacterOwner();

		m_Shell = new ELIFE_PhoneScreenShell();
		m_Shell.Init(root, phone, m_bInteractive);

		if (!m_bInteractive)
			return;

		m_Shell.GetOnAppRequested().Insert(OnAppRequested);
		m_Shell.GetOnHomePill().Insert(OnHomePill);
		m_Shell.GetOnBack().Insert(OnNavBack);
	}

	//------------------------------------------------------------------------------------------------
	bool IsInteractive()
	{
		return m_bInteractive;
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
	//! Applied locally first, then replicated, so the tap doesn't wait on a round trip.
	protected void OnAppRequested(EPhoneScreenState state, string subState)
	{
		if (!m_Shell || !m_Phone)
			return;

		if (state == EPhoneScreenState.MAP)
		{
			ELIFE_PhoneMenu menu = ELIFE_PhoneMenu.GetOpen();
			if (menu)
			{
				menu.OpenMap();
				return;
			}
		}

		m_Shell.ShowState(state);
		if (subState != "")
			m_Shell.ApplySubState(subState);

		m_Phone.SetScreenState(state);
		m_Phone.SetScreenSubState(subState);
	}

	//------------------------------------------------------------------------------------------------
	//! The home pill always goes home; from home it puts the phone away.
	protected void OnHomePill()
	{
		if (!m_Shell)
			return;

		if (m_Shell.GetState() != EPhoneScreenState.HOME)
		{
			GoHome();
			return;
		}

		PutAway();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNavBack()
	{
		if (!m_Shell)
			return;

		//! An app gets first refusal on Back - statement back to accounts, thread back to the inbox.
		if (m_Shell.AppConsumedBack())
			return;

		if (m_Shell.GetOpenApp())
		{
			GoHome();
			return;
		}

		PutAway();
	}

	//------------------------------------------------------------------------------------------------
	//! Back key. Like on-screen Back, except on the lock screen it puts the phone away.
	void OnCloseAction()
	{
		if (m_Shell && m_Shell.GetState() == EPhoneScreenState.LOCKED)
		{
			PutAway();
			return;
		}

		OnNavBack();
	}

	//------------------------------------------------------------------------------------------------
	bool AcceptsPinKeys()
	{
		return m_Shell && m_Shell.AcceptsPinKeys();
	}

	//------------------------------------------------------------------------------------------------
	void OnPinDigit(int digit)
	{
		if (m_Shell)
			m_Shell.PinKeyDigit(digit);
	}

	//------------------------------------------------------------------------------------------------
	void OnPinBackspace()
	{
		if (m_Shell)
			m_Shell.PinKeyBackspace();
	}

	//------------------------------------------------------------------------------------------------
	protected void PutAway()
	{
		ELIFE_PhoneMenu menu = ELIFE_PhoneMenu.GetOpen();
		if (menu)
			menu.CloseAndHolster();
		else if (m_Phone)
			m_Phone.Holster();
	}

	//------------------------------------------------------------------------------------------------
	protected void GoHome()
	{
		m_Shell.ShowState(EPhoneScreenState.HOME);

		if (m_Phone)
			m_Phone.SetScreenState(EPhoneScreenState.HOME);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneScreenController()
	{
		if (m_Shell)
			m_Shell.Destroy();
	}
}
