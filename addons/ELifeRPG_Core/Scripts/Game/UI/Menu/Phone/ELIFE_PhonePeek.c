//------------------------------------------------------------------------------------------------
//! Holstered notification glance: a banner strip slides in from the top edge, holds, and slides back.
//! Not a menu: no cursor, no input context, one at a time.
class ELIFE_PhonePeek
{
	protected static ref ELIFE_PhonePeek s_Instance;

	protected const ResourceName LAYOUT = "{5A3C91E7D2B84F0F}UI/layouts/Menus/Phone/PhonePeek.layout";

	//! Top padding of the strip at rest and fully hidden.
	protected const float PEEK_REST_TOP = 24;
	protected const float PEEK_HIDDEN_TOP = -180;

	//! Hold past the banner so it does not start leaving while you are still reading.
	protected const int PEEK_HOLD_MS = 5500;

	protected Widget m_wRoot;
	protected Widget m_wPeekSize;
	protected ref ELIFE_PhoneScreenShell m_Shell;

	protected float m_fProgress;
	protected bool m_bOpening;

	//! Taken when the phone comes out; consumed by the menu on the same open.
	protected static string s_sHandoffThreadId;

	//------------------------------------------------------------------------------------------------
	//! No-ops while the phone is already in hand — that screen has its own banner.
	static void Show(ELIFE_PhoneGadgetComponent phone)
	{
		if (!phone || phone.GetMode() == EGadgetMode.IN_HAND)
			return;

		if (s_Instance)
		{
			s_Instance.Restart();
			return;
		}

		s_Instance = new ELIFE_PhonePeek();
		if (!s_Instance.Open(phone))
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (s_Instance)
			s_sHandoffThreadId = "";

		CloseInstance();
	}

	//------------------------------------------------------------------------------------------------
	//! Stash the banner's thread, then tear the peek down.
	static void TakeDoorAndHide()
	{
		if (s_Instance)
			s_sHandoffThreadId = s_Instance.GetDoorThreadId();

		CloseInstance();
	}

	//------------------------------------------------------------------------------------------------
	static string ConsumeHandoff()
	{
		string threadId = s_sHandoffThreadId;
		s_sHandoffThreadId = "";
		return threadId;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CloseInstance()
	{
		if (!s_Instance)
			return;

		s_Instance.Close();
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetDoorThreadId()
	{
		if (!m_Shell)
			return "";

		return m_Shell.GetNewestNotificationThreadId();
	}

	//------------------------------------------------------------------------------------------------
	protected bool Open(notnull ELIFE_PhoneGadgetComponent phone)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		m_wRoot = workspace.CreateWidgets(LAYOUT, null);
		if (!m_wRoot)
			return false;

		m_wPeekSize = m_wRoot.FindAnyWidget("PeekSize");
		Widget host = m_wRoot.FindAnyWidget("PeekHost");
		if (!m_wPeekSize || !host)
		{
			Close();
			return false;
		}

		Widget screen = workspace.CreateWidgets(ELIFE_PhoneScreenRenderComponent.PHONE_SCREEN_LAYOUT, host);
		if (!screen)
		{
			Close();
			return false;
		}

		//! CreateWidgets() doesn't give the returned root a fill slot by default.
		AlignableSlot.SetHorizontalAlign(screen, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(screen, LayoutVerticalAlign.Stretch);

		m_Shell = new ELIFE_PhoneScreenShell();
		m_Shell.Init(screen, phone, false);
		m_Shell.UseBannerOnly();

		//! Banners only raise on an awake screen; lock would suppress them.
		m_Shell.ShowState(EPhoneScreenState.HOME, false);
		m_Shell.RaisePendingBanners();

		PlaySlideIn();

		GetGame().GetCallqueue().CallLater(OnHoldElapsed, PEEK_HOLD_MS, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Second arrival while already up: banner the new one and restart the hold.
	protected void Restart()
	{
		if (m_Shell)
			m_Shell.RaiseArrivedBanners();

		m_bOpening = true;

		GetGame().GetCallqueue().Remove(Expire);
		GetGame().GetCallqueue().Remove(OnHoldElapsed);
		GetGame().GetCallqueue().CallLater(OnHoldElapsed, PEEK_HOLD_MS, false);

		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().CallLater(Tick, ELIFE_PhoneStyle.TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaySlideIn()
	{
		m_fProgress = 0;
		m_bOpening = true;

		Apply();

		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().CallLater(Tick, ELIFE_PhoneStyle.TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHoldElapsed()
	{
		m_bOpening = false;

		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().CallLater(Tick, ELIFE_PhoneStyle.TICK_MS, true);

		GetGame().GetCallqueue().CallLater(Expire, ELIFE_PhoneStyle.DURATION_PRESENT_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Instance method so Restart() can cancel this exact callback before it fires.
	protected void Expire()
	{
		if (m_bOpening)
			return;

		Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!m_wPeekSize)
		{
			GetGame().GetCallqueue().Remove(Tick);
			return;
		}

		float step = ELIFE_PhoneStyle.TICK_MS / (float)ELIFE_PhoneStyle.DURATION_PRESENT_MS;
		if (m_bOpening)
			m_fProgress = Math.Min(1, m_fProgress + step);
		else
			m_fProgress = Math.Max(0, m_fProgress - step);

		Apply();

		if ((m_bOpening && m_fProgress >= 1) || (!m_bOpening && m_fProgress <= 0))
			GetGame().GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply()
	{
		float eased = ELIFE_PhoneStyle.EaseOut(m_fProgress);
		float top = PEEK_HIDDEN_TOP + eased * (PEEK_REST_TOP - PEEK_HIDDEN_TOP);

		AlignableSlot.SetPadding(m_wPeekSize, 0, top, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Close()
	{
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(OnHoldElapsed);
		GetGame().GetCallqueue().Remove(Expire);

		if (m_Shell)
		{
			m_Shell.Destroy();
			m_Shell = null;
		}

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_wPeekSize = null;
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhonePeek()
	{
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(OnHoldElapsed);
		GetGame().GetCallqueue().Remove(Expire);
	}
}
