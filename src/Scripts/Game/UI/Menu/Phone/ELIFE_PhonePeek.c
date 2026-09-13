//------------------------------------------------------------------------------------------------
//! Holstered notification glance. Not a menu: no cursor, no input context, one at a time.
class ELIFE_PhonePeek
{
	protected static ref ELIFE_PhonePeek s_Instance;

	protected const ResourceName LAYOUT = "{72F2A11B5C8D3A24}UI/layouts/Menus/Phone/PhoneMenu.layout";

	//! How far along the menu's slide path the peek rests. 0 is fully up, 1 is fully off-screen.
	protected const float PEEK_SLIDE_FRACTION = 0.62;

	//! Hold past the banner so the phone does not start leaving while you are still reading.
	protected const int PEEK_HOLD_MS = 5500;

	protected Widget m_wRoot;
	protected Widget m_wPhoneSize;
	protected ref ELIFE_PhoneScreenShell m_Shell;

	protected float m_fRestLeft, m_fRestTop, m_fRestRight, m_fRestBottom;
	protected float m_fProgress;
	protected bool m_bOpening;
	protected float m_fSlideOffset;

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

		//! Menu layout is a fullscreen overlay; a glance must not dim the world or eat clicks.
		IgnoreCursorTree(m_wRoot);

		Widget worldBlur = m_wRoot.FindAnyWidget("WorldBlur");
		if (worldBlur)
			worldBlur.SetVisible(false);

		Widget dimmer = m_wRoot.FindAnyWidget("Dimmer");
		if (dimmer)
			dimmer.SetVisible(false);

		m_wPhoneSize = m_wRoot.FindAnyWidget("PhoneSize");

		if (!BindScreen(phone))
		{
			Close();
			return false;
		}

		PlaySlideIn();

		GetGame().GetCallqueue().CallLater(OnHoldElapsed, PEEK_HOLD_MS, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BindScreen(notnull ELIFE_PhoneGadgetComponent phone)
	{
		Widget screen = m_wRoot.FindAnyWidget("PhoneScreen");
		if (!screen)
			return false;

		m_Shell = new ELIFE_PhoneScreenShell();
		m_Shell.Init(screen, phone, false);

		//! Lock has no banner, and its list sits at the bottom a glance would cut off.
		m_Shell.ShowState(EPhoneScreenState.HOME, false);

		m_Shell.RaisePendingBanners();
		IgnoreCursorTree(m_wRoot);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Second arrival while already up: banner the new one and restart the hold.
	protected void Restart()
	{
		if (m_Shell)
		{
			m_Shell.RaiseArrivedBanners();
			IgnoreCursorTree(m_wRoot);
		}

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
		if (!m_wPhoneSize)
			return;

		AlignableSlot.GetPadding(m_wPhoneSize, m_fRestLeft, m_fRestTop, m_fRestRight, m_fRestBottom);

		m_fSlideOffset = ELIFE_PhoneStyle.PHONE_SLIDE_OFFSET * PEEK_SLIDE_FRACTION;

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
		if (!m_wPhoneSize)
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
		float bottom = m_fRestBottom - ELIFE_PhoneStyle.PHONE_SLIDE_OFFSET + eased * (ELIFE_PhoneStyle.PHONE_SLIDE_OFFSET - m_fSlideOffset);

		AlignableSlot.SetPadding(m_wPhoneSize, m_fRestLeft, m_fRestTop, m_fRestRight, bottom);
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

		m_wPhoneSize = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void IgnoreCursorTree(Widget widget)
	{
		if (!widget)
			return;

		widget.SetFlags(WidgetFlags.IGNORE_CURSOR);

		Widget child = widget.GetChildren();
		while (child)
		{
			IgnoreCursorTree(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhonePeek()
	{
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(OnHoldElapsed);
		GetGame().GetCallqueue().Remove(Expire);
	}
}
