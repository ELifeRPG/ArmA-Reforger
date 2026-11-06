//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Banking", description: "The ATM's screen: draws the server's state and turns side buttons and the keypad into requests.")]
class ELIFE_ATMScreenComponentClass : ELIFE_WorldScreenComponentClass
{
}

//------------------------------------------------------------------------------------------------
enum ELIFE_EATMScreen
{
	IDLE,
	LOADING,
	NO_ACCOUNT,
	LOAD_FAILED,
	CHOOSE_ACCOUNT,
	HOME,
	TRANSACTIONS,
	WITHDRAW,
	OTHER_AMOUNT,
	CONFIRM,
	PROCESSING,
	CHECKING,
	RESULT
}

//------------------------------------------------------------------------------------------------
//! One on-screen label beside its physical side button.
class ELIFE_ATMSideLabel
{
	string m_sId;
	TextWidget m_wText;
	bool m_bEnabled;
}

//------------------------------------------------------------------------------------------------
//! Client-local view of ELIFE_ATMComponent. It computes no money: every figure comes from the server.
class ELIFE_ATMScreenComponent : ELIFE_WorldScreenComponent
{
	protected const ResourceName LAYOUT_SIDE_LABEL = "{B7D2E94A10C6F358}UI/layouts/Menus/Banking/ATMSideLabel.layout";
	protected const ResourceName LAYOUT_ROW = "{4C81F6A3D95E07B2}UI/layouts/Menus/Banking/ATMTransactionRow.layout";

	protected const string BUTTON_L1 = "side_l1";
	protected const string BUTTON_L2 = "side_l2";
	protected const string BUTTON_L3 = "side_l3";
	protected const string BUTTON_L4 = "side_l4";
	protected const string BUTTON_R1 = "side_r1";
	protected const string BUTTON_R2 = "side_r2";
	protected const string BUTTON_R3 = "side_r3";
	protected const string BUTTON_R4 = "side_r4";
	protected const string KEY_CANCEL = "fn_cancel";
	protected const string KEY_CLEAR = "fn_clear";
	protected const string KEY_CONFIRM = "fn_confirm";
	protected const string KEY_DIGIT_PREFIX = "key_";
	protected const int KEY_DIGIT_LENGTH = 5;

	//! Canvas Y of each physical button's centre: the buttons aren't evenly spaced, and the labels must read straight across.
	protected const float BUTTON_CENTRE_1 = 114;
	protected const float BUTTON_CENTRE_2 = 308;
	protected const float BUTTON_CENTRE_3 = 511;
	protected const float BUTTON_CENTRE_4 = 730;

	//! Canvas px around a label that still counts as clicking it. The buttons are far enough apart that neighbours can't touch.
	protected const float LABEL_HIT_PADDING = 8;

	//! Content areas in canvas px. Each keeps clear of the labels its screen shows.
	protected const float CONTENT_LEFT = 48;
	protected const float CONTENT_WIDTH = 928;
	protected const float CONTENT_TOP = 84;
	protected const float CONTENT_BOTTOM = 900;
	protected const float ABOVE_R4_BOTTOM = 690;
	protected const float BELOW_R1_TOP = 176;
	protected const float BELOW_R2_TOP = 372;
	protected const float CONFIRM_WIDTH = 680;
	//! Keeps clear of the account labels on the right.
	protected const float ACCOUNTS_TEXT_WIDTH = 400;
	//! Canvas px from a button's centre to the centre of the bank and balance line under its label.
	protected const float ACCOUNT_DETAIL_OFFSET = 44;
	protected const float WITHDRAW_LEFT = 232;
	protected const float WITHDRAW_WIDTH = 488;
	//! Above WideNotice, which sits in the gap between the third and fourth button rows.
	protected const float WITHDRAW_STACK_BOTTOM = 540;

	protected const float PAGE_ROWS_TOP = 76;
	protected const float PAGE_ROWS_WIDTH = 800;
	protected const int PAGE_ROW_COUNT = 8;
	//! One per button in m_aAccountButtons.
	protected const int ACCOUNTS_PER_PAGE = 3;

	protected const int PRESET_L1 = 50;
	protected const int PRESET_L2 = 100;
	protected const int PRESET_L3 = 200;
	protected const int PRESET_L4 = 500;
	protected const int PRESET_R1 = 1000;
	protected const int PRESET_R2 = 2000;

	protected const string UNKNOWN_VALUE = "-";
	protected const string META_SEPARATOR = " · ";
	protected const string ACTIVITY_DOT = "·";
	protected const string ACTIVITY_GAP = " ";
	protected const int ACTIVITY_DOTS = 3;
	protected const int ACTIVITY_STEP_MS = 350;
	protected const int PRESS_FLASH_MS = 150;

	//! Stand-in withdrawals for bystanders: a random multiple of the step, with a fee of 1-2 percent.
	protected const int STAND_IN_AMOUNT_STEP = 10;
	protected const int STAND_IN_AMOUNT_MIN_STEPS = 2;
	protected const int STAND_IN_AMOUNT_MAX_STEPS = 200;
	protected const int STAND_IN_FEE_MIN_PERCENT = 1;
	protected const int STAND_IN_FEE_MAX_PERCENT = 3;
	//! How long a stand-in typed amount is, whatever the occupant actually typed.
	protected const int STAND_IN_TYPED_MIN_DIGITS = 2;
	protected const int STAND_IN_TYPED_MAX_DIGITS = 6;

	protected const int KEEP_ALIVE_INTERVAL_MS = 5000;
	//! Well inside the server's 20 s heartbeat window, so a lost one or two don't matter.
	protected const int HEARTBEAT_INTERVAL_MS = 5000;
	//! How long an outcome stays up once there's no session left to dismiss it with.
	protected const int RESULT_LINGER_MS = 15000;
	//! Vanilla has no cash sound; this one is paper being folded.
	protected const string PAID_SOUND = SCR_SoundEvent.SOUND_HUD_MAP_CLOSE;
	protected const float PAID_HINT_SECONDS = 5;

	protected ELIFE_ATMComponent m_ATM;
	protected ELIFE_WorldScreenInteractComponent m_Interact;

	protected Widget m_wStackArea;
	protected Widget m_wRows;
	protected TextWidget m_wAccountNumber;
	protected TextWidget m_wPageMark;
	protected TextWidget m_wBankName;
	protected TextWidget m_wHeading;
	protected Widget m_wLine1;
	protected TextWidget m_wLine1Label;
	protected TextWidget m_wLine1Value;
	protected Widget m_wLine2;
	protected TextWidget m_wLine2Label;
	protected TextWidget m_wLine2Value;
	protected TextWidget m_wCaption;
	protected TextWidget m_wFigure;
	protected TextWidget m_wBody;
	protected TextWidget m_wNotice;
	protected TextWidget m_wWideNotice;
	protected TextWidget m_wActivity;
	protected ref array<ref ELIFE_ATMSideLabel> m_aLabels = {};
	protected string m_sHoveredButton;
	protected string m_sPressedButton;
	protected bool m_bActivityRunning;
	//! Dots showing; 0 until the first step, so a page that's gone within it never shows any.
	protected int m_iActivityDots;

	//! From our grant until the menu closes or the server releases us.
	protected bool m_bSessionOpen;
	protected bool m_bPayloadSeen;
	protected ref ELIFE_BankStatementDto m_Statement;
	protected ref ELIFE_ATMAccountList m_Accounts;
	protected ref array<TextWidget> m_aAccountDetails = {};
	protected int m_iAccountPage;
	protected ref array<string> m_aAccountButtons = {BUTTON_R1, BUTTON_R2, BUTTON_R3};
	protected int m_iLastKeepAliveTick;

	//! Where the session is, while the server's state doesn't override it.
	protected ELIFE_EATMScreen m_eView = ELIFE_EATMScreen.HOME;
	protected ELIFE_EATMScreen m_eConfirmOrigin = ELIFE_EATMScreen.WITHDRAW;
	protected int m_iTransactionPage;
	protected string m_sTyped;

	//! A refusal shown on the page that asked, in place of the result screen.
	protected ELIFE_EATMReason m_eNotice;

	//! The amount a quote was asked for; 0 once answered or abandoned, so a late answer can't change the page.
	protected int m_iQuoteAsked;
	protected int m_iQuoteId;
	protected int m_iQuoteAmount;
	protected int m_iQuoteFeeCents;
	protected int m_iQuoteTotalCents;
	protected int m_iWithdrawingAmount;
	protected int m_iProcessingStartTick;

	protected ELIFE_EATMReason m_eResult;
	protected int m_iResultAmount;
	protected int m_iResultFeeCents;
	protected int m_iResultBalanceCents;
	protected bool m_bResultBalanceKnown;
	protected int m_iResultDropped;
	//! A paid result waiting out PROCESSING_MIN_MS.
	protected bool m_bResultPending;
	//! Whether m_Statement is newer than the last result, so its balance can stand in for one the server didn't know.
	protected bool m_bStatementAfterResult;

	//! Last page sent to the server, so only a change costs a message. -1 forces the next one.
	protected int m_iSentMirrorScreen = -1;
	protected int m_iSentMirrorPage;
	protected bool m_bSentMirrorTyped;
	protected int m_iSentMirrorReason;
	//! The mirrored page the stand-in figures were last rolled for.
	protected ELIFE_EATMScreen m_eMirrorSeen = ELIFE_EATMScreen.IDLE;
	protected bool m_bHeldLock;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		m_ATM = ELIFE_ATMComponent.Cast(owner.FindComponent(ELIFE_ATMComponent));
		m_Interact = ELIFE_WorldScreenInteractComponent.Cast(owner.FindComponent(ELIFE_WorldScreenInteractComponent));
		if (!m_Interact)
			return;

		m_Interact.GetOnButtonPressed().Insert(OnPropButtonPressed);
		m_Interact.GetOnButtonHover().Insert(OnPropButtonHover);
		m_Interact.GetOnActiveChanged().Insert(OnInteractActiveChanged);
	}

	//------------------------------------------------------------------------------------------------
	void OnGrant(bool granted, int reason)
	{
		if (!granted)
		{
			StoreResult(reason, 0, 0, 0, false);
			PresentResult();
			return;
		}

		//! The phone may have opened while the request travelled.
		if (ELIFE_PhoneMenu.GetOpen() || !m_Interact)
		{
			SendClose();
			return;
		}

		GetGame().GetCallqueue().Remove(ClearLingeringResult);
		RemoveResultTimers();
		m_bResultPending = false;

		m_bSessionOpen = true;
		m_eResult = ELIFE_EATMReason.NONE;
		SetView(ELIFE_EATMScreen.HOME);
		GetGame().GetCallqueue().Remove(SendHeartbeat);
		GetGame().GetCallqueue().CallLater(SendHeartbeat, HEARTBEAT_INTERVAL_MS, true);
		m_Interact.SetActive(true);
		ELIFE_WorldScreenMenu.Open(m_Interact);
		Render();
	}

	//------------------------------------------------------------------------------------------------
	void OnRealPayload(string json)
	{
		m_bStatementAfterResult = true;
		ApplyPayload(json);
	}

	//------------------------------------------------------------------------------------------------
	//! The occupant has the real figures and ignores the broadcast.
	void OnRedactedPayload(string json)
	{
		if (!IsLocalOccupant())
			ApplyPayload(json);
	}

	//------------------------------------------------------------------------------------------------
	void OnRealAccounts(string json)
	{
		ApplyAccounts(json);
	}

	//------------------------------------------------------------------------------------------------
	//! The occupant has the real figures and ignores the broadcast.
	void OnRedactedAccounts(string json)
	{
		if (!IsLocalOccupant())
			ApplyAccounts(json);
	}

	//------------------------------------------------------------------------------------------------
	void OnQuote(int quoteId, int amount, int feeCents, int totalCents)
	{
		if (m_iQuoteAsked == 0 || amount != m_iQuoteAsked)
			return;

		m_iQuoteId = quoteId;
		m_iQuoteAmount = amount;
		m_iQuoteFeeCents = feeCents;
		m_iQuoteTotalCents = totalCents;
		m_eConfirmOrigin = m_eView;
		SetView(ELIFE_EATMScreen.CONFIRM);
		Render();
	}

	//------------------------------------------------------------------------------------------------
	//! Stays on the page that asked, so the player can correct it there. An answer nobody is waiting for is dropped.
	void OnQuoteRefused(int reason, int amount)
	{
		if (m_iQuoteAsked == 0 || amount != m_iQuoteAsked)
			return;

		m_iQuoteAsked = 0;
		m_eNotice = reason;
		Render();
	}

	//------------------------------------------------------------------------------------------------
	void OnResult(int reason, int amount, int feeCents, int balanceCents, bool balanceKnown, int droppedAmount)
	{
		m_iQuoteId = 0;
		m_sTyped = "";
		m_bStatementAfterResult = false;

		if (reason == ELIFE_EATMReason.QUOTE_INVALID)
		{
			m_iWithdrawingAmount = 0;
			SetView(ELIFE_EATMScreen.WITHDRAW);
			m_eNotice = reason;
			Render();
			return;
		}

		StoreResult(reason, amount, feeCents, balanceCents, balanceKnown, droppedAmount);

		int holdMs = ELIFE_ATMComponent.PROCESSING_MIN_MS - (System.GetTickCount() - m_iProcessingStartTick);
		if (reason == ELIFE_EATMReason.PAID && m_bSessionOpen && m_eView == ELIFE_EATMScreen.PROCESSING && holdMs > 0)
		{
			m_bResultPending = true;
			GetGame().GetCallqueue().CallLater(PresentResult, holdMs);
			return;
		}

		PresentResult();
	}

	//------------------------------------------------------------------------------------------------
	void OnMirrorChanged()
	{
		if (!m_ATM || IsLocalOccupant() || m_eResult != ELIFE_EATMReason.NONE)
			return;

		ApplyMirror();
		Render();
	}

	//------------------------------------------------------------------------------------------------
	//! The server's lock decides: losing it closes the menu without another close request.
	void OnOccupancyChanged()
	{
		bool mine = HoldsLock();

		//! The figures of a bystander's stand-in page must not reach the player's own.
		if (mine && !m_bHeldLock)
			ClearStandIns();

		m_bHeldLock = mine;

		if (!mine)
		{
			ClearStandIns();
			m_iSentMirrorScreen = -1;
			m_Accounts = null;
			m_Statement = null;
			m_bPayloadSeen = false;
			m_iQuoteId = 0;
			m_iQuoteAsked = 0;
			m_eNotice = ELIFE_EATMReason.NONE;

			//! The server keeps the lock for as long as a paid result is up, so this ends it.
			if (m_eResult == ELIFE_EATMReason.PAID && !m_bResultPending)
			{
				GetGame().GetCallqueue().Remove(ClearLingeringResult);
				m_eResult = ELIFE_EATMReason.NONE;
				SetView(ELIFE_EATMScreen.HOME);
			}
		}

		if (m_bSessionOpen && !mine)
		{
			m_bSessionOpen = false;
			RemoveResultTimers();

			//! Already paid, so the player still hears about it.
			if (m_bResultPending)
				PresentResult();
			else
				LingerResult();

			ELIFE_WorldScreenMenu menu = ELIFE_WorldScreenMenu.GetOpen();
			if (menu && menu.GetBoundScreen() == m_Interact)
				menu.CloseMenu();
		}

		if (!IsLocalOccupant() && m_eResult == ELIFE_EATMReason.NONE)
			ApplyMirror();

		Render();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearStandIns()
	{
		m_sTyped = "";
		m_iTransactionPage = 0;
		m_iAccountPage = 0;
		m_iQuoteAmount = 0;
		m_iWithdrawingAmount = 0;
		m_bStatementAfterResult = false;
		m_eMirrorSeen = ELIFE_EATMScreen.IDLE;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the fields the pages already draw from with stand-ins, so a bystander runs the same rendering as the occupant.
	protected void ApplyMirror()
	{
		ELIFE_EATMScreen screen = m_ATM.GetMirrorScreen();

		if (screen == ELIFE_EATMScreen.CHOOSE_ACCOUNT)
			m_iAccountPage = m_ATM.GetMirrorPage();
		else
			m_iTransactionPage = m_ATM.GetMirrorPage();

		m_eNotice = ELIFE_EATMReason.NONE;
		if (screen != ELIFE_EATMScreen.RESULT)
			m_eNotice = m_ATM.GetMirrorReason();

		if (screen == ELIFE_EATMScreen.OTHER_AMOUNT)
			SetStandInTyped(m_ATM.IsMirrorTyped());

		if (screen == ELIFE_EATMScreen.CONFIRM || screen == ELIFE_EATMScreen.PROCESSING || screen == ELIFE_EATMScreen.CHECKING || screen == ELIFE_EATMScreen.RESULT)
		{
			//! A new Confirm page means a new amount; the pages after it keep showing the same one.
			if (screen == ELIFE_EATMScreen.CONFIRM && m_eMirrorSeen != ELIFE_EATMScreen.CONFIRM)
				m_iQuoteAmount = 0;

			if (m_iQuoteAmount == 0)
				RollStandInAmount();

			//! Reads the redacted statement's balance, which the result then shows.
			m_bResultBalanceKnown = false;
			m_bStatementAfterResult = true;
		}

		m_eMirrorSeen = screen;
	}

	//------------------------------------------------------------------------------------------------
	//! One rolled amount for as long as something is typed: the length of the real one would give its size away.
	protected void SetStandInTyped(bool typed)
	{
		if (!typed)
		{
			m_sTyped = "";
			return;
		}

		if (m_sTyped != "")
			return;

		int count = Math.RandomInt(STAND_IN_TYPED_MIN_DIGITS, STAND_IN_TYPED_MAX_DIGITS);
		while (m_sTyped.Length() < count)
		{
			int low = 0;
			if (m_sTyped == "")
				low = 1;

			int digit = Math.RandomInt(low, 10);
			m_sTyped += digit.ToString();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What was "typed" carries over to Confirm, as it does for the occupant.
	protected void RollStandInAmount()
	{
		int amount = Math.RandomInt(STAND_IN_AMOUNT_MIN_STEPS, STAND_IN_AMOUNT_MAX_STEPS) * STAND_IN_AMOUNT_STEP;
		if (m_eMirrorSeen == ELIFE_EATMScreen.OTHER_AMOUNT && m_sTyped != "")
			amount = m_sTyped.ToInt();

		m_iQuoteAmount = amount;
		m_iQuoteFeeCents = amount * Math.RandomInt(STAND_IN_FEE_MIN_PERCENT, STAND_IN_FEE_MAX_PERCENT);
		m_iQuoteTotalCents = amount * 100 + m_iQuoteFeeCents;
		m_iWithdrawingAmount = amount;
		m_iResultAmount = amount;
		m_iResultFeeCents = m_iQuoteFeeCents;
	}

	//------------------------------------------------------------------------------------------------
	//! Only for the occupant. A failed send stays unrecorded, so the next render tries again.
	protected void SendMirror(ELIFE_EATMScreen screen)
	{
		if (!HoldsLock())
			return;

		int reason = m_eNotice;
		if (screen == ELIFE_EATMScreen.RESULT)
			reason = m_eResult;

		int page;
		if (screen == ELIFE_EATMScreen.TRANSACTIONS)
			page = m_iTransactionPage;
		else if (screen == ELIFE_EATMScreen.CHOOSE_ACCOUNT)
			page = m_iAccountPage;

		bool typed = screen == ELIFE_EATMScreen.OTHER_AMOUNT && m_sTyped != "";

		if (screen == m_iSentMirrorScreen && page == m_iSentMirrorPage && typed == m_bSentMirrorTyped && reason == m_iSentMirrorReason)
			return;

		ELIFE_ATMCharacterComponent link = GetLink();
		if (!link)
			return;

		m_iSentMirrorScreen = screen;
		m_iSentMirrorPage = page;
		m_bSentMirrorTyped = typed;
		m_iSentMirrorReason = reason;
		link.RequestMirror(GetOwner(), screen, page, typed, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! A result from the occupant's page, or this client's own if it has one.
	protected ELIFE_EATMReason GetShownResult()
	{
		if (m_eResult != ELIFE_EATMReason.NONE || IsLocalOccupant())
			return m_eResult;

		return m_ATM.GetMirrorReason();
	}

	//------------------------------------------------------------------------------------------------
	void OnStateChanged()
	{
		//! The old statement must not show as the next account's while that one's payload travels.
		if (m_ATM && m_ATM.GetState() == ELIFE_EATMState.CHOOSE_ACCOUNT && IsLocalOccupant())
		{
			m_Statement = null;
			if (m_eView != ELIFE_EATMScreen.RESULT && m_eView != ELIFE_EATMScreen.PROCESSING)
				SetView(ELIFE_EATMScreen.HOME);
		}

		Render();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HoldsLock()
	{
		return m_ATM && m_ATM.GetOccupantPlayerId() != 0 && m_ATM.GetOccupantPlayerId() == SCR_PlayerController.GetLocalPlayerId();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalOccupant()
	{
		return m_bSessionOpen || HoldsLock();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPayload(string json)
	{
		m_bPayloadSeen = true;
		m_Statement = null;
		if (json != "")
		{
			m_Statement = new ELIFE_BankStatementDto();
			m_Statement.ExpandFromRAW(json);
		}

		Render();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyAccounts(string json)
	{
		m_bPayloadSeen = true;
		m_Accounts = null;
		if (json != "")
		{
			m_Accounts = new ELIFE_ATMAccountList();
			m_Accounts.ExpandFromRAW(json);
		}

		Render();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasSeveralAccounts()
	{
		return m_Accounts && m_Accounts.items.Count() > 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void StoreResult(int reason, int amount, int feeCents, int balanceCents, bool balanceKnown, int droppedAmount = 0)
	{
		m_eResult = reason;
		m_iResultAmount = amount;
		m_iResultFeeCents = feeCents;
		m_iResultBalanceCents = balanceCents;
		m_bResultBalanceKnown = balanceKnown;
		m_iResultDropped = droppedAmount;
	}

	//------------------------------------------------------------------------------------------------
	//! The stored result goes on screen. A paid one is announced here, once, and releases the player at the same moment.
	protected void PresentResult()
	{
		m_bResultPending = false;
		m_iWithdrawingAmount = 0;
		SetView(ELIFE_EATMScreen.RESULT);
		LingerResult();

		if (m_eResult == ELIFE_EATMReason.PAID)
			AnnouncePaid();

		Render();

		if (m_eResult == ELIFE_EATMReason.PAID)
			CloseScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! The hint stays silent, so the paper sound is the only one.
	protected void AnnouncePaid()
	{
		SCR_UISoundEntity.SoundEvent(PAID_SOUND);
		SCR_HintManagerComponent.ShowCustomHint(GetPaidHintText(), "#ELIFE-ATM_Hint_Cash_Title", PAID_HINT_SECONDS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetPaidHintText()
	{
		if (m_iResultDropped <= 0)
			return WidgetManager.Translate("#ELIFE-ATM_Hint_Cash", Whole(m_iResultAmount));

		if (m_iResultDropped >= m_iResultAmount)
			return WidgetManager.Translate("#ELIFE-ATM_Hint_Cash_Ground", Whole(m_iResultDropped));

		return WidgetManager.Translate("#ELIFE-ATM_Hint_Cash_Split", Whole(m_iResultAmount - m_iResultDropped), Whole(m_iResultDropped));
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveResultTimers()
	{
		GetGame().GetCallqueue().Remove(PresentResult);
	}

	//------------------------------------------------------------------------------------------------
	//! A result with no session behind it (refused open, or the menu closed mid-withdrawal) has no Done to clear it.
	protected void LingerResult()
	{
		if (m_bSessionOpen || m_eResult == ELIFE_EATMReason.NONE)
			return;

		GetGame().GetCallqueue().Remove(ClearLingeringResult);
		GetGame().GetCallqueue().CallLater(ClearLingeringResult, RESULT_LINGER_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearLingeringResult()
	{
		m_eResult = ELIFE_EATMReason.NONE;
		SetView(ELIFE_EATMScreen.HOME);

		if (!IsLocalOccupant())
			ApplyMirror();

		Render();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetView(ELIFE_EATMScreen view)
	{
		m_eView = view;
		m_eNotice = ELIFE_EATMReason.NONE;
		m_iQuoteAsked = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ATMCharacterComponent GetLink()
	{
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (!character)
			return null;

		return ELIFE_ATMCharacterComponent.Cast(character.FindComponent(ELIFE_ATMCharacterComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! With no character left to send through (death), the server's tick releases the lock.
	protected void SendClose()
	{
		ELIFE_ATMCharacterComponent link = GetLink();
		if (link)
			link.RequestClose(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnInteractActiveChanged(bool active)
	{
		if (active || !m_bSessionOpen)
			return;

		m_bSessionOpen = false;
		m_iQuoteId = 0;
		m_sTyped = "";
		RemoveResultTimers();

		//! Already paid, so the player still hears about it.
		if (m_bResultPending)
			PresentResult();

		//! The player has left, but the server keeps the machine until the result has been shown.
		if (m_eResult == ELIFE_EATMReason.PAID && m_eView == ELIFE_EATMScreen.RESULT)
		{
			LingerResult();
			Render();
			return;
		}

		m_eResult = ELIFE_EATMReason.NONE;
		SetView(ELIFE_EATMScreen.HOME);
		SendClose();
		Render();
	}

	//------------------------------------------------------------------------------------------------
	//! Every recheck: a bystander near a busy ATM that hasn't had its data yet asks for it (streamed in late, or just bound).
	protected override void OnProximityChecked(float distance)
	{
		if (!m_bBound || m_bPayloadSeen || !m_ATM || m_ATM.GetOccupantPlayerId() == 0 || IsLocalOccupant())
			return;

		ELIFE_ATMCharacterComponent link = GetLink();
		if (link)
			link.RequestRedacted(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! The server's state outranks the player's page, and a result outlasts the load that follows it.
	protected ELIFE_EATMScreen ResolveScreen()
	{
		if (!IsLocalOccupant())
		{
			if (m_eResult != ELIFE_EATMReason.NONE)
				return ELIFE_EATMScreen.RESULT;

			if (m_ATM.GetOccupantPlayerId() != 0)
				return ResolveMirrorScreen();

			return ELIFE_EATMScreen.IDLE;
		}

		ELIFE_EATMState state = m_ATM.GetState();
		if (state == ELIFE_EATMState.PROCESSING)
			return ELIFE_EATMScreen.PROCESSING;

		if (state == ELIFE_EATMState.CHECKING)
			return ELIFE_EATMScreen.CHECKING;

		//! The view is PROCESSING from the confirm press until the answer is shown, whatever order state and answer arrive in.
		if (m_eView == ELIFE_EATMScreen.RESULT || m_eView == ELIFE_EATMScreen.PROCESSING)
			return m_eView;

		if (state == ELIFE_EATMState.CHOOSE_ACCOUNT)
		{
			if (m_Accounts)
				return ELIFE_EATMScreen.CHOOSE_ACCOUNT;

			return ELIFE_EATMScreen.LOADING;
		}

		if (state == ELIFE_EATMState.NO_ACCOUNT)
			return ELIFE_EATMScreen.NO_ACCOUNT;

		if (state == ELIFE_EATMState.LOAD_FAILED)
			return ELIFE_EATMScreen.LOAD_FAILED;

		if (state != ELIFE_EATMState.READY || !m_Statement)
			return ELIFE_EATMScreen.LOADING;

		return m_eView;
	}

	//------------------------------------------------------------------------------------------------
	//! The occupant's page, once there is one and the redacted statement to fill it with has arrived.
	protected ELIFE_EATMScreen ResolveMirrorScreen()
	{
		ELIFE_EATMScreen screen = m_ATM.GetMirrorScreen();
		if (screen == ELIFE_EATMScreen.IDLE)
			return ELIFE_EATMScreen.LOADING;

		if (screen == ELIFE_EATMScreen.LOADING || screen == ELIFE_EATMScreen.NO_ACCOUNT || screen == ELIFE_EATMScreen.LOAD_FAILED || screen == ELIFE_EATMScreen.PROCESSING || screen == ELIFE_EATMScreen.CHECKING)
			return screen;

		if (screen == ELIFE_EATMScreen.CHOOSE_ACCOUNT)
		{
			if (m_Accounts)
				return screen;

			return ELIFE_EATMScreen.LOADING;
		}

		if (!m_Statement)
			return ELIFE_EATMScreen.LOADING;

		return screen;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPropButtonHover(string id)
	{
		m_sHoveredButton = id;
		PaintLabels();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPropButtonPressed(string id)
	{
		if (!m_bSessionOpen || !m_ATM)
			return;

		ELIFE_ATMCharacterComponent link = GetLink();
		if (!link)
			return;

		SendKeepAlive(link);
		FlashPressed(id);

		switch (ResolveScreen())
		{
			case ELIFE_EATMScreen.LOADING:
				PressLoading(id);
				break;
			case ELIFE_EATMScreen.NO_ACCOUNT:
			case ELIFE_EATMScreen.LOAD_FAILED:
				PressLoadProblem(id, link);
				break;
			case ELIFE_EATMScreen.CHOOSE_ACCOUNT:
				PressChooseAccount(id, link);
				break;
			case ELIFE_EATMScreen.HOME:
				PressHome(id, link);
				break;
			case ELIFE_EATMScreen.TRANSACTIONS:
				PressTransactions(id);
				break;
			case ELIFE_EATMScreen.WITHDRAW:
				PressWithdraw(id, link);
				break;
			case ELIFE_EATMScreen.OTHER_AMOUNT:
				PressOtherAmount(id, link);
				break;
			case ELIFE_EATMScreen.CONFIRM:
				PressConfirm(id, link);
				break;
			case ELIFE_EATMScreen.RESULT:
				PressResult(id);
				break;
		}

		Render();
	}

	//------------------------------------------------------------------------------------------------
	protected void FlashPressed(string id)
	{
		m_sPressedButton = id;
		GetGame().GetCallqueue().Remove(ClearPressed);
		GetGame().GetCallqueue().CallLater(ClearPressed, PRESS_FLASH_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPressed()
	{
		m_sPressedButton = "";
		PaintLabels();
	}

	//------------------------------------------------------------------------------------------------
	protected void PressLoading(string id)
	{
		if (id == KEY_CANCEL)
			CloseScreen();
	}

	//------------------------------------------------------------------------------------------------
	protected void PressLoadProblem(string id, notnull ELIFE_ATMCharacterComponent link)
	{
		if (id == BUTTON_R1)
		{
			link.RequestRefresh(GetOwner());
		}
		else if (id == BUTTON_R3 && HasSeveralAccounts())
		{
			m_iAccountPage = 0;
			link.RequestChangeAccount(GetOwner());
		}
		else if (id == BUTTON_R4 || id == KEY_CANCEL)
		{
			CloseScreen();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PressChooseAccount(string id, notnull ELIFE_ATMCharacterComponent link)
	{
		if (id == BUTTON_L1 && m_iAccountPage > 0)
		{
			m_iAccountPage--;
		}
		else if (id == BUTTON_L2 && HasNextAccountPage())
		{
			m_iAccountPage++;
		}
		else if (id == BUTTON_R4 || id == KEY_CANCEL)
		{
			CloseScreen();
		}
		else
		{
			int slot = m_aAccountButtons.Find(id);
			int index = m_iAccountPage * ACCOUNTS_PER_PAGE + slot;
			if (slot >= 0 && m_Accounts && index < m_Accounts.items.Count())
			{
				SetView(ELIFE_EATMScreen.HOME);
				link.RequestSelectAccount(GetOwner(), m_Accounts.items[index].number);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasNextAccountPage()
	{
		return m_Accounts && (m_iAccountPage + 1) * ACCOUNTS_PER_PAGE < m_Accounts.items.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected void PressHome(string id, notnull ELIFE_ATMCharacterComponent link)
	{
		if (id == BUTTON_R1)
		{
			SetView(ELIFE_EATMScreen.WITHDRAW);
		}
		else if (id == BUTTON_R3 && HasSeveralAccounts())
		{
			m_iAccountPage = 0;
			link.RequestChangeAccount(GetOwner());
		}
		else if (id == BUTTON_R2)
		{
			m_iTransactionPage = 0;
			SetView(ELIFE_EATMScreen.TRANSACTIONS);
		}
		else if (id == BUTTON_R4 || id == KEY_CANCEL)
		{
			CloseScreen();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PressTransactions(string id)
	{
		if (id == BUTTON_R1 && HasNewerPage())
			m_iTransactionPage--;
		else if (id == BUTTON_R2 && HasOlderPage())
			m_iTransactionPage++;
		else if (id == BUTTON_R4 || id == KEY_CANCEL)
			SetView(ELIFE_EATMScreen.HOME);
	}

	//------------------------------------------------------------------------------------------------
	protected void PressWithdraw(string id, notnull ELIFE_ATMCharacterComponent link)
	{
		if (id == BUTTON_R3)
		{
			m_sTyped = "";
			SetView(ELIFE_EATMScreen.OTHER_AMOUNT);
			return;
		}

		if (id == BUTTON_R4 || id == KEY_CANCEL)
		{
			SetView(ELIFE_EATMScreen.HOME);
			return;
		}

		int preset = GetPreset(id);
		if (preset > 0 && IsWithinBalance(preset))
			AskQuote(preset, link);
	}

	//------------------------------------------------------------------------------------------------
	protected void PressOtherAmount(string id, notnull ELIFE_ATMCharacterComponent link)
	{
		if (id.Length() == KEY_DIGIT_LENGTH && id.StartsWith(KEY_DIGIT_PREFIX))
		{
			PressDigit(id.Substring(KEY_DIGIT_LENGTH - 1, 1));
		}
		else if (id == KEY_CLEAR)
		{
			m_sTyped = "";
			m_eNotice = ELIFE_EATMReason.NONE;
			m_iQuoteAsked = 0;
		}
		else if (id == KEY_CONFIRM || id == BUTTON_R3)
		{
			if (m_sTyped != "")
				AskQuote(m_sTyped.ToInt(), link);
		}
		else if (id == BUTTON_R4 || id == KEY_CANCEL)
		{
			SetView(ELIFE_EATMScreen.WITHDRAW);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Capped by digits only: an amount above the limit is the server's to refuse, with its reason on screen.
	protected void PressDigit(string digit)
	{
		int maxAmount = ELIFE_ATMComponent.MAX_WITHDRAWAL;
		if (m_sTyped.Length() >= maxAmount.ToString().Length() || (m_sTyped == "" && digit == "0"))
			return;

		m_sTyped += digit;
		m_eNotice = ELIFE_EATMReason.NONE;
		m_iQuoteAsked = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void PressConfirm(string id, notnull ELIFE_ATMCharacterComponent link)
	{
		if (id == BUTTON_R3 || id == KEY_CANCEL)
		{
			m_iQuoteId = 0;
			SetView(m_eConfirmOrigin);
			return;
		}

		if ((id != BUTTON_R4 && id != KEY_CONFIRM) || m_iQuoteId == 0)
			return;

		//! On a host the answer arrives inside the request, so everything it reads or overrides is set first.
		int quoteId = m_iQuoteId;
		m_iWithdrawingAmount = m_iQuoteAmount;
		m_iProcessingStartTick = System.GetTickCount();
		m_iQuoteId = 0;
		SetView(ELIFE_EATMScreen.PROCESSING);
		link.RequestConfirm(GetOwner(), quoteId);
	}

	//------------------------------------------------------------------------------------------------
	protected void PressResult(string id)
	{
		//! The menu is already closing.
		if (m_eResult == ELIFE_EATMReason.PAID)
			return;

		if (id != BUTTON_R4 && id != KEY_CANCEL)
			return;

		m_eResult = ELIFE_EATMReason.NONE;
		SetView(ELIFE_EATMScreen.HOME);
	}

	//------------------------------------------------------------------------------------------------
	//! A second request while one travels would make the first answer's quote id stale.
	protected void AskQuote(int amount, notnull ELIFE_ATMCharacterComponent link)
	{
		if (m_iQuoteAsked != 0)
			return;

		m_eNotice = ELIFE_EATMReason.NONE;
		m_iQuoteAsked = amount;
		link.RequestQuote(GetOwner(), amount);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseScreen()
	{
		ELIFE_WorldScreenMenu menu = ELIFE_WorldScreenMenu.GetOpen();
		if (menu && menu.GetBoundScreen() == m_Interact)
			menu.CloseMenu();
	}

	//------------------------------------------------------------------------------------------------
	//! Every few seconds while the menu is open, whether or not anything is pressed. Stops itself once the session ends.
	protected void SendHeartbeat()
	{
		if (!m_bSessionOpen)
		{
			GetGame().GetCallqueue().Remove(SendHeartbeat);
			return;
		}

		ELIFE_ATMCharacterComponent link = GetLink();
		if (link)
			link.RequestHeartbeat(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! A press, so it also counts as input for the server's AFK limit.
	protected void SendKeepAlive(notnull ELIFE_ATMCharacterComponent link)
	{
		int now = System.GetTickCount();
		if (m_iLastKeepAliveTick != 0 && now - m_iLastKeepAliveTick < KEEP_ALIVE_INTERVAL_MS)
			return;

		m_iLastKeepAliveTick = now;
		link.RequestKeepAlive(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected int GetPreset(string id)
	{
		if (id == BUTTON_L1)
			return PRESET_L1;

		if (id == BUTTON_L2)
			return PRESET_L2;

		if (id == BUTTON_L3)
			return PRESET_L3;

		if (id == BUTTON_L4)
			return PRESET_L4;

		if (id == BUTTON_R1)
			return PRESET_R1;

		if (id == BUTTON_R2)
			return PRESET_R2;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Only dims and blocks a preset; the server still decides on amount plus fee.
	protected bool IsWithinBalance(int amount)
	{
		return HasAccount() && amount * 100 <= ELIFE_PhoneBankingService.ToCents(m_Statement.account.balance);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasAccount()
	{
		return m_Statement && m_Statement.account.number != "";
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasNewerPage()
	{
		return m_iTransactionPage > 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasOlderPage()
	{
		return m_Statement && (m_iTransactionPage + 1) * PAGE_ROW_COUNT < m_Statement.items.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected override bool BuildContent(Widget host)
	{
		if (!super.BuildContent(host))
			return false;

		Widget contentRoot = GetContentRoot();
		if (!contentRoot)
			return true;

		m_wStackArea = contentRoot.FindAnyWidget("StackArea");
		m_wRows = contentRoot.FindAnyWidget("Rows");
		m_wAccountNumber = TextWidget.Cast(contentRoot.FindAnyWidget("AccountNumber"));
		m_wPageMark = TextWidget.Cast(contentRoot.FindAnyWidget("PageMark"));
		m_wBankName = TextWidget.Cast(contentRoot.FindAnyWidget("BankName"));
		m_wHeading = FindWrappedText(contentRoot, "Heading");
		m_wLine1 = contentRoot.FindAnyWidget("Line1");
		m_wLine1Label = TextWidget.Cast(contentRoot.FindAnyWidget("Line1Label"));
		m_wLine1Value = TextWidget.Cast(contentRoot.FindAnyWidget("Line1Value"));
		m_wLine2 = contentRoot.FindAnyWidget("Line2");
		m_wLine2Label = TextWidget.Cast(contentRoot.FindAnyWidget("Line2Label"));
		m_wLine2Value = TextWidget.Cast(contentRoot.FindAnyWidget("Line2Value"));
		m_wCaption = TextWidget.Cast(contentRoot.FindAnyWidget("Caption"));
		m_wFigure = TextWidget.Cast(contentRoot.FindAnyWidget("Figure"));
		m_wBody = FindWrappedText(contentRoot, "Body");
		m_wNotice = FindWrappedText(contentRoot, "Notice");
		m_wWideNotice = FindWrappedText(contentRoot, "WideNotice");
		m_wActivity = TextWidget.Cast(contentRoot.FindAnyWidget("Activity"));

		TextWidget mark = TextWidget.Cast(contentRoot.FindAnyWidget("BankMark"));
		if (mark)
			mark.SetText(ELIFE_PhoneStyle.CURRENCY_SYMBOL);

		TextWidget machineId = TextWidget.Cast(contentRoot.FindAnyWidget("MachineId"));
		if (machineId && m_ATM)
			machineId.SetText(m_ATM.GetMachineId());

		BuildLabels(contentRoot.FindAnyWidget("ScreenFrame"));
		BuildAccountDetail(contentRoot, "AccountDetail1", BUTTON_CENTRE_1);
		BuildAccountDetail(contentRoot, "AccountDetail2", BUTTON_CENTRE_2);
		BuildAccountDetail(contentRoot, "AccountDetail3", BUTTON_CENTRE_3);

		//! A late joiner may never have had a mirror update to roll its stand-ins from.
		if (m_ATM && !IsLocalOccupant() && m_eResult == ELIFE_EATMReason.NONE)
			ApplyMirror();

		Render();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget FindWrappedText(notnull Widget root, string name)
	{
		TextWidget text = TextWidget.Cast(root.FindAnyWidget(name));
		if (text)
			text.SetTextWrapping(true);

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildLabels(Widget frame)
	{
		m_aLabels.Clear();
		if (!frame)
			return;

		BuildLabel(frame, BUTTON_L1, false, BUTTON_CENTRE_1);
		BuildLabel(frame, BUTTON_L2, false, BUTTON_CENTRE_2);
		BuildLabel(frame, BUTTON_L3, false, BUTTON_CENTRE_3);
		BuildLabel(frame, BUTTON_L4, false, BUTTON_CENTRE_4);
		BuildLabel(frame, BUTTON_R1, true, BUTTON_CENTRE_1);
		BuildLabel(frame, BUTTON_R2, true, BUTTON_CENTRE_2);
		BuildLabel(frame, BUTTON_R3, true, BUTTON_CENTRE_3);
		BuildLabel(frame, BUTTON_R4, true, BUTTON_CENTRE_4);
	}

	//------------------------------------------------------------------------------------------------
	//! Hugs its edge and centres on the button, however wide the localized text comes out.
	protected void BuildLabel(notnull Widget frame, string id, bool rightEdge, float centreY)
	{
		TextWidget text = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(LAYOUT_SIDE_LABEL, frame));
		if (!text)
			return;

		FrameSlot.SetSizeToContent(text, true);
		if (rightEdge)
		{
			FrameSlot.SetAnchor(text, 1, 0);
			FrameSlot.SetAlignment(text, 1, 0.5);
			FrameSlot.SetPos(text, -ELIFE_PhoneStyle.SPACE_3, centreY);
		}
		else
		{
			FrameSlot.SetAnchor(text, 0, 0);
			FrameSlot.SetAlignment(text, 0, 0.5);
			FrameSlot.SetPos(text, ELIFE_PhoneStyle.SPACE_3, centreY);
		}

		ELIFE_ATMSideLabel label = new ELIFE_ATMSideLabel();
		label.m_sId = id;
		label.m_wText = text;
		m_aLabels.Insert(label);
	}

	//------------------------------------------------------------------------------------------------
	//! Hugs the right edge under its button's label, like the label itself.
	protected void BuildAccountDetail(notnull Widget root, string name, float centreY)
	{
		TextWidget detail = TextWidget.Cast(root.FindAnyWidget(name));
		if (!detail)
			return;

		FrameSlot.SetSizeToContent(detail, true);
		FrameSlot.SetAnchor(detail, 1, 0);
		FrameSlot.SetAlignment(detail, 1, 0.5);
		FrameSlot.SetPos(detail, -ELIFE_PhoneStyle.SPACE_3, centreY + ACCOUNT_DETAIL_OFFSET);
		m_aAccountDetails.Insert(detail);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnUnbound()
	{
		super.OnUnbound();

		m_wStackArea = null;
		m_wRows = null;
		m_wAccountNumber = null;
		m_wPageMark = null;
		m_wBankName = null;
		m_aAccountDetails.Clear();
		m_wHeading = null;
		m_wLine1 = null;
		m_wLine1Label = null;
		m_wLine1Value = null;
		m_wLine2 = null;
		m_wLine2Label = null;
		m_wLine2Value = null;
		m_wCaption = null;
		m_wFigure = null;
		m_wBody = null;
		m_wNotice = null;
		m_wWideNotice = null;
		m_wActivity = null;
		StopActivity();
		m_aLabels.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void Render()
	{
		if (!m_ATM)
			return;

		ELIFE_EATMScreen screen = ResolveScreen();
		SendMirror(screen);

		if (!m_wStackArea || !m_wRows)
			return;

		ResetContent();
		RenderStrip(screen);
		RenderContent(screen);
		UpdateActivity(screen);

		//! A bystander sees the occupant's labels too, but only a session has a menu to press them with.
		if (m_bSessionOpen || (!IsLocalOccupant() && m_eResult == ELIFE_EATMReason.NONE))
			RenderLabels(screen);

		PaintLabels();
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetContent()
	{
		foreach (ELIFE_ATMSideLabel label : m_aLabels)
		{
			label.m_wText.SetText("");
			label.m_bEnabled = false;
		}

		m_wHeading.SetVisible(false);
		m_wAccountNumber.SetVisible(false);
		m_wLine1.SetVisible(false);
		m_wLine2.SetVisible(false);
		m_wCaption.SetVisible(false);
		m_wFigure.SetVisible(false);
		m_wBody.SetVisible(false);
		m_wBody.SetColor(ELIFE_PhoneStyle.TextPrimary());
		m_wNotice.SetVisible(false);
		m_wWideNotice.SetVisible(false);
		m_wActivity.SetVisible(false);
		foreach (TextWidget detail : m_aAccountDetails)
		{
			detail.SetVisible(false);
		}

		m_wRows.SetVisible(false);
		ClearRows();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearRows()
	{
		Widget child = m_wRows.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RenderStrip(ELIFE_EATMScreen screen)
	{
		m_wBankName.SetText(m_ATM.GetBankName());

		if (screen == ELIFE_EATMScreen.TRANSACTIONS)
			m_wPageMark.SetTextFormat("#ELIFE-ATM_Tx_Page", m_iTransactionPage + 1, GetTransactionPageCount());
		else
			m_wPageMark.SetText("");
	}

	//------------------------------------------------------------------------------------------------
	protected int GetTransactionPageCount()
	{
		if (!m_Statement || m_Statement.items.IsEmpty())
			return 1;

		return (m_Statement.items.Count() + PAGE_ROW_COUNT - 1) / PAGE_ROW_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	//! Dots on every page that's waiting for an answer. The page keeps its text, so nothing else moves.
	protected void UpdateActivity(ELIFE_EATMScreen screen)
	{
		if (screen != ELIFE_EATMScreen.LOADING && screen != ELIFE_EATMScreen.PROCESSING && screen != ELIFE_EATMScreen.CHECKING)
		{
			StopActivity();
			return;
		}

		if (!m_bActivityRunning)
		{
			m_bActivityRunning = true;
			m_iActivityDots = 0;
			GetGame().GetCallqueue().CallLater(StepActivity, ACTIVITY_STEP_MS, true);
		}

		PaintActivity();
		m_wActivity.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopActivity()
	{
		GetGame().GetCallqueue().Remove(StepActivity);
		m_bActivityRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void StepActivity()
	{
		m_iActivityDots++;
		if (m_iActivityDots > ACTIVITY_DOTS)
			m_iActivityDots = 1;

		PaintActivity();
	}

	//------------------------------------------------------------------------------------------------
	//! Never empty, as an empty text has no height and the page would jump when the first dot appears.
	protected void PaintActivity()
	{
		if (!m_wActivity)
			return;

		string dots = ACTIVITY_GAP;
		for (int i = 0; i < m_iActivityDots; i++)
		{
			dots += ACTIVITY_DOT + ACTIVITY_GAP;
		}

		m_wActivity.SetText(dots);
	}

	//------------------------------------------------------------------------------------------------
	protected void RenderContent(ELIFE_EATMScreen screen)
	{
		switch (screen)
		{
			case ELIFE_EATMScreen.IDLE:
				PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, CONTENT_BOTTOM);
				ShowFigure(ELIFE_PhoneStyle.CURRENCY_SYMBOL, ELIFE_PhoneStyle.TextPrimary());
				ShowText(m_wBody, "#ELIFE-ATM_Ready");
				return;
			case ELIFE_EATMScreen.LOADING:
				PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, CONTENT_BOTTOM);
				ShowText(m_wHeading, "#ELIFE-ATM_Loading");
				return;
			case ELIFE_EATMScreen.NO_ACCOUNT:
				PlaceStack(CONTENT_LEFT, BELOW_R1_TOP, CONTENT_WIDTH, ABOVE_R4_BOTTOM);
				ShowText(m_wBody, "#ELIFE-ATM_NoAccount");
				return;
			case ELIFE_EATMScreen.LOAD_FAILED:
				//! Narrower when the switch label shares the row.
				if (HasSeveralAccounts())
					PlaceStack(CONTENT_LEFT, BELOW_R1_TOP, CONFIRM_WIDTH, ABOVE_R4_BOTTOM);
				else
					PlaceStack(CONTENT_LEFT, BELOW_R1_TOP, CONTENT_WIDTH, ABOVE_R4_BOTTOM);

				ShowText(m_wBody, "#ELIFE-ATM_LoadFailed");
				return;
			case ELIFE_EATMScreen.CHOOSE_ACCOUNT:
				PlaceStack(CONTENT_LEFT, BELOW_R2_TOP, ACCOUNTS_TEXT_WIDTH, ABOVE_R4_BOTTOM);
				ShowText(m_wHeading, "#ELIFE-ATM_Accounts_Title");
				return;
			case ELIFE_EATMScreen.HOME:
				PlaceStack(CONTENT_LEFT, BELOW_R1_TOP, CONTENT_WIDTH, CONTENT_BOTTOM);
				if (HasAccount())
					ShowText(m_wAccountNumber, m_Statement.account.GetDisplayNumber());

				ShowText(m_wCaption, "#ELIFE-ATM_Balance");
				ShowFigure(GetBalanceText(), ELIFE_PhoneStyle.AccentBank());
				RenderInspectHint();
				return;
			case ELIFE_EATMScreen.TRANSACTIONS:
				RenderTransactions();
				return;
			case ELIFE_EATMScreen.WITHDRAW:
				PlaceStack(WITHDRAW_LEFT, CONTENT_TOP, WITHDRAW_WIDTH, WITHDRAW_STACK_BOTTOM);
				ShowText(m_wHeading, "#ELIFE-ATM_Withdraw_Title");
				ShowLine(m_wLine1, m_wLine1Label, m_wLine1Value, "#ELIFE-ATM_Balance", GetBalanceText());
				ShowRefusal(m_eNotice, m_wWideNotice);
				return;
			case ELIFE_EATMScreen.OTHER_AMOUNT:
				RenderOtherAmount();
				return;
			case ELIFE_EATMScreen.CONFIRM:
				PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONFIRM_WIDTH, CONTENT_BOTTOM);
				ShowLine(m_wLine1, m_wLine1Label, m_wLine1Value, "#ELIFE-ATM_Amount", Whole(m_iQuoteAmount));
				ShowLine(m_wLine2, m_wLine2Label, m_wLine2Value, "#ELIFE-ATM_Fee", Money(m_iQuoteFeeCents));
				ShowText(m_wCaption, "#ELIFE-ATM_Total");
				ShowFigure(Money(m_iQuoteTotalCents), ELIFE_PhoneStyle.AccentBank());
				return;
			case ELIFE_EATMScreen.PROCESSING:
				RenderProcessing();
				return;
			case ELIFE_EATMScreen.CHECKING:
				PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, CONTENT_BOTTOM);
				ShowText(m_wBody, "#ELIFE-ATM_Checking");
				return;
			case ELIFE_EATMScreen.RESULT:
				RenderResult();
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The key does nothing without the menu, so bystanders aren't told about it.
	protected void RenderInspectHint()
	{
		if (!m_bSessionOpen || !m_Interact || !m_Interact.CanInspect())
			return;

		ShowText(m_wBody, "#ELIFE-ATM_Inspect_Hint");
		m_wBody.SetColor(ELIFE_PhoneStyle.TextSecondary());
	}

	//------------------------------------------------------------------------------------------------
	protected void RenderTransactions()
	{
		if (m_Statement.items.IsEmpty())
		{
			PlaceStack(CONTENT_LEFT, CONTENT_TOP, PAGE_ROWS_WIDTH, CONTENT_BOTTOM);
			ShowText(m_wBody, "#ELIFE-ATM_Tx_Empty");
			return;
		}

		ShowRows(m_iTransactionPage * PAGE_ROW_COUNT, PAGE_ROW_COUNT, PAGE_ROWS_TOP, PAGE_ROWS_WIDTH);
	}

	//------------------------------------------------------------------------------------------------
	protected void RenderOtherAmount()
	{
		PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONFIRM_WIDTH, ABOVE_R4_BOTTOM);
		ShowText(m_wHeading, "#ELIFE-ATM_Other_Title");

		if (m_sTyped == "")
			ShowFigure(Whole(0), ELIFE_PhoneStyle.TextTertiary());
		else
			ShowFigure(Whole(m_sTyped.ToInt()), ELIFE_PhoneStyle.TextPrimary());

		m_wBody.SetTextFormat("#ELIFE-ATM_Other_Hint", Whole(ELIFE_ATMComponent.MAX_WITHDRAWAL));
		m_wBody.SetColor(ELIFE_PhoneStyle.TextSecondary());
		m_wBody.SetVisible(true);
		ShowRefusal(m_eNotice);
	}

	//------------------------------------------------------------------------------------------------
	//! After reopening mid-withdrawal, this client no longer knows the amount.
	protected void RenderProcessing()
	{
		PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, CONTENT_BOTTOM);

		if (m_iWithdrawingAmount == 0)
		{
			ShowText(m_wHeading, "#ELIFE-ATM_Processing_Generic");
			return;
		}

		m_wHeading.SetTextFormat("#ELIFE-ATM_Processing", Whole(m_iWithdrawingAmount));
		m_wHeading.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void RenderResult()
	{
		PlaceStack(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, ABOVE_R4_BOTTOM);

		ELIFE_EATMReason result = GetShownResult();
		if (result != ELIFE_EATMReason.PAID && result != ELIFE_EATMReason.RETURNED)
		{
			ShowRefusal(result);
			return;
		}

		ShowLine(m_wLine1, m_wLine1Label, m_wLine1Value, "#ELIFE-ATM_PaidOut", Whole(m_iResultAmount));
		ShowLine(m_wLine2, m_wLine2Label, m_wLine2Value, "#ELIFE-ATM_Fee", Money(m_iResultFeeCents));
		ShowText(m_wCaption, "#ELIFE-ATM_RemainingBalance");
		ShowFigure(GetResultBalanceText(), ELIFE_PhoneStyle.AccentBank());

		if (result == ELIFE_EATMReason.PAID)
			ShowText(m_wHeading, "#ELIFE-ATM_Complete");
		else
			ShowText(m_wBody, "#ELIFE-ATM_Returned_Hint");
	}

	//------------------------------------------------------------------------------------------------
	protected void RenderLabels(ELIFE_EATMScreen screen)
	{
		switch (screen)
		{
			case ELIFE_EATMScreen.NO_ACCOUNT:
			case ELIFE_EATMScreen.LOAD_FAILED:
				SetLabel(BUTTON_R1, "#ELIFE-ATM_Action_TryAgain");
				if (HasSeveralAccounts())
					SetLabel(BUTTON_R3, "#ELIFE-ATM_Action_SwitchAccount");

				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Exit");
				return;
			case ELIFE_EATMScreen.CHOOSE_ACCOUNT:
				SetAccountLabels();
				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Exit");
				return;
			case ELIFE_EATMScreen.HOME:
				SetLabel(BUTTON_R1, "#ELIFE-ATM_Action_Withdraw");
				SetLabel(BUTTON_R2, "#ELIFE-ATM_Action_Transactions");
				if (HasSeveralAccounts())
					SetLabel(BUTTON_R3, "#ELIFE-ATM_Action_SwitchAccount");

				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Exit");
				return;
			case ELIFE_EATMScreen.TRANSACTIONS:
				if (HasNewerPage())
					SetLabel(BUTTON_R1, "#ELIFE-ATM_Action_Newer");

				if (HasOlderPage())
					SetLabel(BUTTON_R2, "#ELIFE-ATM_Action_Older");

				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Back");
				return;
			case ELIFE_EATMScreen.WITHDRAW:
				SetPresetLabel(BUTTON_L1);
				SetPresetLabel(BUTTON_L2);
				SetPresetLabel(BUTTON_L3);
				SetPresetLabel(BUTTON_L4);
				SetPresetLabel(BUTTON_R1);
				SetPresetLabel(BUTTON_R2);
				SetLabel(BUTTON_R3, "#ELIFE-ATM_Action_OtherAmount");
				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Back");
				return;
			case ELIFE_EATMScreen.OTHER_AMOUNT:
				//! Not on R4, where the next page has its Confirm: a double press must not skip the fee.
				SetLabel(BUTTON_R3, "#ELIFE-ATM_Action_Confirm", m_sTyped != "");
				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Back");
				return;
			case ELIFE_EATMScreen.CONFIRM:
				SetLabel(BUTTON_R3, "#ELIFE-ATM_Action_ChangeAmount");
				SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Confirm");
				return;
			case ELIFE_EATMScreen.RESULT:
				if (GetShownResult() != ELIFE_EATMReason.PAID)
					SetLabel(BUTTON_R4, "#ELIFE-ATM_Action_Done");

				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The number on the label, the bank and balance under it. Bystanders get stand-ins for number and balance.
	protected void SetAccountLabels()
	{
		if (!m_Accounts)
			return;

		foreach (int slot, string button : m_aAccountButtons)
		{
			int index = m_iAccountPage * ACCOUNTS_PER_PAGE + slot;
			if (index >= m_Accounts.items.Count())
				break;

			ELIFE_ATMAccountChoice account = m_Accounts.items[index];
			SetLabel(button, ELIFE_PhoneBankingService.GroupIban(account.number));

			if (!m_aAccountDetails.IsIndexValid(slot))
				continue;

			string detailText = Money(ELIFE_PhoneBankingService.ToCents(account.balance));
			if (account.bankName != "")
				detailText = account.bankName + META_SEPARATOR + detailText;

			m_aAccountDetails[slot].SetText(detailText);
			m_aAccountDetails[slot].SetVisible(true);
		}

		if (m_iAccountPage > 0)
			SetLabel(BUTTON_L1, "#ELIFE-ATM_Action_Previous");

		if (HasNextAccountPage())
			SetLabel(BUTTON_L2, "#ELIFE-ATM_Action_Next");
	}

	//------------------------------------------------------------------------------------------------
	protected void SetPresetLabel(string id)
	{
		int preset = GetPreset(id);
		SetLabel(id, Whole(preset), IsWithinBalance(preset));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLabel(string id, string text, bool enabled = true)
	{
		foreach (ELIFE_ATMSideLabel label : m_aLabels)
		{
			if (label.m_sId != id)
				continue;

			label.m_wText.SetText(text);
			label.m_bEnabled = enabled;
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintLabels()
	{
		foreach (ELIFE_ATMSideLabel label : m_aLabels)
		{
			if (!label.m_bEnabled)
				label.m_wText.SetColor(ELIFE_PhoneStyle.TextTertiary());
			else if (label.m_sId == m_sPressedButton)
				label.m_wText.SetColor(ELIFE_PhoneStyle.AccentDeepBank());
			else if (label.m_sId == m_sHoveredButton)
				label.m_wText.SetColor(ELIFE_PhoneStyle.AccentBank());
			else
				label.m_wText.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Only enabled labels count, which also rules out blank ones: SetLabel is what enables.
	override string FindButtonAtCanvas(float canvasX, float canvasY)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return "";

		//! Screen rects are in DPI-scaled pixels, so scale the point once instead of every rect.
		float x = workspace.DPIScale(canvasX);
		float y = workspace.DPIScale(canvasY);
		float padding = workspace.DPIScale(LABEL_HIT_PADDING);

		foreach (ELIFE_ATMSideLabel label : m_aLabels)
		{
			if (!label.m_bEnabled)
				continue;

			float left, top, width, height;
			label.m_wText.GetScreenPos(left, top);
			label.m_wText.GetScreenSize(width, height);

			if (x >= left - padding && x <= left + width + padding && y >= top - padding && y <= top + height + padding)
				return label.m_sId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaceStack(float left, float top, float width, float bottom)
	{
		FrameSlot.SetPos(m_wStackArea, left, top);
		FrameSlot.SetSize(m_wStackArea, width, bottom - top);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowText(notnull TextWidget widget, string text)
	{
		widget.SetText(text);
		widget.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowFigure(string text, Color color)
	{
		m_wFigure.SetColor(color);
		ShowText(m_wFigure, text);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowLine(notnull Widget line, notnull TextWidget label, notnull TextWidget value, string labelKey, string valueText)
	{
		label.SetText(labelKey);
		value.SetText(valueText);
		line.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowRefusal(ELIFE_EATMReason reason, TextWidget target = null)
	{
		string key = GetRefusalKey(reason);
		if (key == "")
			return;

		if (!target)
			target = m_wNotice;

		string amount = Whole(m_iResultAmount);
		if (reason == ELIFE_EATMReason.AMOUNT_TOO_HIGH)
			amount = Whole(ELIFE_ATMComponent.MAX_WITHDRAWAL);

		target.SetTextFormat(key, amount);
		target.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetRefusalKey(ELIFE_EATMReason reason)
	{
		switch (reason)
		{
			case ELIFE_EATMReason.IN_USE:
				return "#ELIFE-ATM_InUse";
			case ELIFE_EATMReason.OUT_OF_RANGE:
				return "#ELIFE-ATM_Reason_OutOfRange";
			case ELIFE_EATMReason.NOT_OCCUPANT:
				return "#ELIFE-ATM_Reason_NotOccupant";
			case ELIFE_EATMReason.INVALID_AMOUNT:
				return "#ELIFE-ATM_Reason_InvalidAmount";
			case ELIFE_EATMReason.AMOUNT_TOO_HIGH:
				return "#ELIFE-ATM_Reason_AmountTooHigh";
			case ELIFE_EATMReason.QUOTE_INVALID:
				return "#ELIFE-ATM_Reason_QuoteInvalid";
			case ELIFE_EATMReason.BUSY:
				return "#ELIFE-ATM_Reason_Busy";
			case ELIFE_EATMReason.NO_ACCOUNT:
				return "#ELIFE-ATM_NoAccount";
			case ELIFE_EATMReason.DATA_UNAVAILABLE:
				return "#ELIFE-ATM_Reason_DataUnavailable";
			case ELIFE_EATMReason.INSUFFICIENT_FUNDS:
				return "#ELIFE-ATM_Reason_InsufficientFunds";
			case ELIFE_EATMReason.DENIED:
				return "#ELIFE-ATM_Reason_Denied";
			case ELIFE_EATMReason.NOT_WITHDRAWN:
				return "#ELIFE-ATM_Reason_NotWithdrawn";
			case ELIFE_EATMReason.CONTACT_STAFF:
				return "#ELIFE-ATM_Reason_ContactStaff";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected string GetBalanceText()
	{
		if (!HasAccount())
			return UNKNOWN_VALUE;

		return Money(ELIFE_PhoneBankingService.ToCents(m_Statement.account.balance));
	}

	//------------------------------------------------------------------------------------------------
	//! The statement from before the withdrawal would show the old balance, so it only counts once refreshed.
	protected string GetResultBalanceText()
	{
		if (m_bResultBalanceKnown)
			return Money(m_iResultBalanceCents);

		if (m_bStatementAfterResult)
			return GetBalanceText();

		return UNKNOWN_VALUE;
	}

	//------------------------------------------------------------------------------------------------
	//! Rows arrive newest first, redacted or not.
	protected void ShowRows(int first, int count, float top, float width)
	{
		FrameSlot.SetPos(m_wRows, CONTENT_LEFT, top);
		FrameSlot.SetSizeX(m_wRows, width);

		int end = first + count;
		if (end > m_Statement.items.Count())
			end = m_Statement.items.Count();

		for (int i = first; i < end; i++)
		{
			Widget row = GetGame().GetWorkspace().CreateWidgets(LAYOUT_ROW, m_wRows);
			if (!row)
				continue;

			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
			FillRow(row, m_Statement.items[i]);
		}

		m_wRows.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillRow(notnull Widget row, ELIFE_BankAccountTransaction transaction)
	{
		TextWidget kind = TextWidget.Cast(row.FindAnyWidget("RowKind"));
		TextWidget amount = TextWidget.Cast(row.FindAnyWidget("RowAmount"));
		TextWidget meta = TextWidget.Cast(row.FindAnyWidget("RowMeta"));
		if (!transaction || !kind || !amount || !meta)
			return;

		int cents = ELIFE_PhoneBankingService.ToCents(transaction.amount);
		if (transaction.IsOutgoing())
			cents = -cents;

		string metaText = ELIFE_PhoneAppBase.FormatDayTime(transaction.occurredAt);
		int feeCents = ELIFE_PhoneBankingService.ToCents(transaction.fee);
		if (feeCents > 0)
			metaText += META_SEPARATOR + WidgetManager.Translate("#ELIFE-ATM_Tx_Fee", Money(feeCents));

		kind.SetText(GetKindText(transaction.kind));
		amount.SetText(ELIFE_PhoneBankingService.FormatSignedMoney(cents));
		if (cents > 0)
			amount.SetColor(ELIFE_PhoneStyle.Positive());
		else if (cents < 0)
			amount.SetColor(ELIFE_PhoneStyle.Negative());
		meta.SetText(metaText);
	}

	//------------------------------------------------------------------------------------------------
	//! An unknown kind shows as sent rather than blank.
	protected string GetKindText(string kind)
	{
		if (kind == ELIFE_BankAccountTransaction.KIND_DEPOSITED)
			return "#ELIFE-ATM_Tx_Deposited";

		if (kind == ELIFE_BankAccountTransaction.KIND_WITHDRAWN)
			return "#ELIFE-ATM_Tx_Withdrawn";

		if (kind == ELIFE_BankAccountTransaction.KIND_TRANSFERRED_OUT)
			return "#ELIFE-ATM_Tx_TransferredOut";

		if (kind == ELIFE_BankAccountTransaction.KIND_TRANSFERRED_IN)
			return "#ELIFE-ATM_Tx_TransferredIn";

		return kind;
	}

	//------------------------------------------------------------------------------------------------
	protected string Money(int cents)
	{
		return ELIFE_PhoneBankingService.FormatMoney(cents);
	}

	//------------------------------------------------------------------------------------------------
	protected string Whole(int tolars)
	{
		return ELIFE_PhoneBankingService.FormatWholeMoney(tolars);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_ATMScreenComponent()
	{
		GetGame().GetCallqueue().Remove(SendHeartbeat);
		GetGame().GetCallqueue().Remove(ClearLingeringResult);
		GetGame().GetCallqueue().Remove(ClearPressed);
		StopActivity();
		RemoveResultTimers();
	}
}
