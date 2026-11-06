//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Banking", description: "Server-authoritative ATM: occupant lock, account data, quotes and withdrawals")]
class ELIFE_ATMComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Replicated, so bystanders and late joiners see it too.
enum ELIFE_EATMState
{
	FREE,
	LOADING,
	READY,
	NO_ACCOUNT,
	LOAD_FAILED,
	PROCESSING,
	CHECKING,
	CHOOSE_ACCOUNT
}

//------------------------------------------------------------------------------------------------
enum ELIFE_EATMReason
{
	NONE,
	IN_USE,
	OUT_OF_RANGE,
	NOT_OCCUPANT,
	INVALID_AMOUNT,
	AMOUNT_TOO_HIGH,
	QUOTE_INVALID,
	BUSY,
	NO_ACCOUNT,
	DATA_UNAVAILABLE,
	INSUFFICIENT_FUNDS,
	DENIED,
	NOT_WITHDRAWN,
	PAID,
	RETURNED,
	CONTACT_STAFF
}

//------------------------------------------------------------------------------------------------
//! One line of the account choice. Built by the server rather than sent as the API's account, which has no bank name.
class ELIFE_ATMAccountChoice : JsonApiStruct
{
	string number;
	float balance;
	string bankName;

	//------------------------------------------------------------------------------------------------
	void ELIFE_ATMAccountChoice(string accountNumber = "", float accountBalance = 0, string accountBankName = "")
	{
		number = accountNumber;
		balance = accountBalance;
		bankName = accountBankName;

		RegV("number");
		RegV("balance");
		RegV("bankName");
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_ATMAccountList : JsonApiStruct
{
	ref array<ref ELIFE_ATMAccountChoice> items = {};

	//------------------------------------------------------------------------------------------------
	void ELIFE_ATMAccountList()
	{
		RegV("items");
	}
}

//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array banks response as {"items": <raw>}.
class ELIFE_BankListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_BankSummary> items = {};

	void ELIFE_BankListDto()
	{
		RegV("items");
	}
}

//------------------------------------------------------------------------------------------------
//! Everything a running withdrawal needs, fixed when it's confirmed. Lives only on the server.
class ELIFE_ATMWithdrawal
{
	string m_sMachineId;
	int m_iPlayerId;
	IEntity m_Character;
	string m_sCharacterId;
	string m_sAccountNumber;
	int m_iAmount;
	float m_fQuotedFee;
	float m_fFeeBase;
	float m_fFeeMultiplier;
	float m_fBaseline;
	string m_sBaselineAt;
	int m_iPaid;
	int m_iDropped;
	float m_fFee;
	float m_fNewBalance;
	bool m_bBalanceKnown;
	float m_fCredit;
	float m_fDeposit;

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		return string.Format("%1 player %2 character %3 account %4 amount %5 baseline %6 newest row '%7' paid %8", m_sMachineId, m_iPlayerId, m_sCharacterId, m_sAccountNumber, m_iAmount, m_fBaseline, m_sBaselineAt, m_iPaid);
	}
}

//------------------------------------------------------------------------------------------------
//! The server owns the prop, so nobody can reach it over RPC: requests arrive and answers leave through
//! ELIFE_ATMCharacterComponent. All API handlers live here, as a handler on a deleted character would be dropped.
class ELIFE_ATMComponent : ScriptComponent
{
	static const int MAX_WITHDRAWAL = 100000;
	//! The withdrawal screen stays at least this long, however fast the answer comes. Only the showing waits, not the money.
	static const int PROCESSING_MIN_MS = 800;
	//! How long the result of a paid withdrawal stays on the screen. Nobody can use the machine until then.
	static const int PAID_RESULT_MS = 8000;

	protected static const int TICK_INTERVAL_MS = 1000;
	protected static const int IDLE_TIMEOUT_MS = 60000;
	protected static const int QUOTE_LIFETIME_MS = 60000;
	protected static const int REFRESH_MIN_INTERVAL_MS = 2000;
	//! Above the 30 s total budget of the Bridge's standard resilience handler, so no retry can land after our read.
	protected static const int RECONCILE_DELAY_MS = 45000;
	protected static const float MONEY_TOLERANCE = 0.01;
	//! A few float steps: above about 130,000 a float no longer holds cents.
	protected static const float MONEY_RELATIVE_TOLERANCE = 0.000001;
	protected static const int HTTP_FORBIDDEN = 403;
	protected static const int HTTP_NOT_FOUND = 404;

	//! Every account choice is a new load of three API calls.
	protected static const int ACCOUNT_CHANGE_MIN_INTERVAL_MS = 500;

	//! One withdrawal at a time per account, across every ATM on this server. Other servers of the hive can't see it.
	protected static ref array<string> s_aBusyAccounts = {};

	[RplProp(onRplName: "OnOccupantUpdated")]
	protected int m_iOccupantPlayerId;

	[RplProp(onRplName: "OnStateUpdated")]
	protected int m_iState;

	[RplProp()]
	protected bool m_bShowingPaid;

	//! What the occupant's screen shows, for everyone else to draw with stand-in figures. Never a real amount.
	[RplProp(onRplName: "OnMirrorUpdated")]
	protected int m_iMirrorScreen;
	[RplProp(onRplName: "OnMirrorUpdated")]
	protected int m_iMirrorPage;
	[RplProp(onRplName: "OnMirrorUpdated")]
	protected bool m_bMirrorTyped;
	[RplProp(onRplName: "OnMirrorUpdated")]
	protected int m_iMirrorReason;

	protected ELIFE_WorldScreenInteractComponent m_Interact;
	protected ELIFE_ATMScreenComponent m_Screen;

	protected IEntity m_Occupant;
	protected string m_sOccupantCharacterId;
	protected int m_iLastRequestTick;
	protected int m_iLastRefreshTick;
	protected bool m_bClosePending;

	protected ref ELIFE_BaseRestCallback m_LoadCallback;
	protected bool m_bLoadInFlight;
	protected bool m_bLoadStale;
	protected string m_sAccountNumber;
	protected string m_sAccountBankId;
	protected string m_sAccountJson;
	protected bool m_bFeesKnown;
	//! From the last load, for refusing a quote early. A withdrawal still checks a fresh balance.
	protected float m_fLoadedBalance;
	protected float m_fFeeBase;
	protected float m_fFeeMultiplier;
	protected string m_sRealJson;
	protected string m_sRedactedJson;

	//! Everyone sees the header, so this is replicated. Empty until an account is chosen.
	[RplProp(onRplName: "OnStateUpdated")]
	protected string m_sBankName;

	protected ref ELIFE_BankAccountListDto m_LoadedAccounts;
	//! The personal accounts of the last load: the only numbers a client may choose from.
	protected ref array<string> m_aAccountNumbers = {};
	protected string m_sChosenAccountNumber;
	protected string m_sRealAccountsJson;
	protected string m_sRedactedAccountsJson;

	protected int m_iNextQuoteId = 1;
	protected int m_iQuoteId;
	protected int m_iLastConsumedQuoteId;
	protected int m_iQuoteAmount;
	protected float m_fQuoteFee;
	protected int m_iQuoteTick;

	protected ref ELIFE_ATMWithdrawal m_Withdrawal;
	protected ref ELIFE_BaseRestCallback m_FlowCallback;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Interact = ELIFE_WorldScreenInteractComponent.Cast(owner.FindComponent(ELIFE_WorldScreenInteractComponent));
		m_Screen = ELIFE_ATMScreenComponent.Cast(owner.FindComponent(ELIFE_ATMScreenComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Built from the placement in decimetres, so every machine and every restart gets the same one with nothing replicated.
	string GetBankName()
	{
		return m_sBankName;
	}

	//------------------------------------------------------------------------------------------------
	string GetMachineId()
	{
		vector position = GetOwner().GetOrigin();
		int x = Math.Round(position[0] * 10);
		int y = Math.Round(position[1] * 10);
		int z = Math.Round(position[2] * 10);
		return string.Format("ATM-%1-%2-%3", x, y, z);
	}

	//------------------------------------------------------------------------------------------------
	int GetOccupantPlayerId()
	{
		return m_iOccupantPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_EATMState GetState()
	{
		return m_iState;
	}

	//------------------------------------------------------------------------------------------------
	bool IsShowingPaid()
	{
		return m_bShowingPaid;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_EATMScreen GetMirrorScreen()
	{
		return m_iMirrorScreen;
	}

	//------------------------------------------------------------------------------------------------
	int GetMirrorPage()
	{
		return m_iMirrorPage;
	}

	//------------------------------------------------------------------------------------------------
	bool IsMirrorTyped()
	{
		return m_bMirrorTyped;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_EATMReason GetMirrorReason()
	{
		return m_iMirrorReason;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Display only, so it needs no reach check, but a client can't claim a page or a payout that doesn't exist.
	void SetMirror(int playerId, notnull IEntity character, int screen, int page, bool typed, int reason)
	{
		if (!IsOccupant(playerId, character))
			return;

		if (screen < ELIFE_EATMScreen.LOADING || screen > ELIFE_EATMScreen.RESULT || reason < ELIFE_EATMReason.NONE || reason > ELIFE_EATMReason.CONTACT_STAFF)
			return;

		if (reason == ELIFE_EATMReason.PAID && !m_bShowingPaid)
			return;

		m_iMirrorScreen = screen;
		m_iMirrorPage = Math.Max(page, 0);
		m_bMirrorTyped = typed;
		m_iMirrorReason = reason;
		Replication.BumpMe();

		//! onRplName doesn't fire on the authority itself.
		OnMirrorUpdated();
	}

	//------------------------------------------------------------------------------------------------
	//! True if the character is alive and within the screen's use range of its centre.
	bool IsUsableBy(IEntity character)
	{
		return m_Interact && m_Interact.IsUsableBy(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Cached, so late joiners never trigger a fetch or a second redaction.
	string GetRedactedJson()
	{
		return m_sRedactedJson;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. A repeat from the occupant re-sends the grant, so reopening during a withdrawal works.
	void RequestOpen(int playerId, notnull IEntity character, notnull ELIFE_ATMCharacterComponent link)
	{
		if (m_iOccupantPlayerId != 0 && m_iOccupantPlayerId != playerId)
		{
			link.SendGrant(GetRplId(), false, ELIFE_EATMReason.IN_USE);
			return;
		}

		//! A refusal would replace the result on the occupant's screen.
		if (m_bShowingPaid)
			return;

		if (!IsUsableBy(character))
		{
			Print(string.Format("ELIFE_ATMComponent | open refused, player %1 is dead or out of range", playerId), LogLevel.WARNING);
			link.SendGrant(GetRplId(), false, ELIFE_EATMReason.OUT_OF_RANGE);
			return;
		}

		if (m_iOccupantPlayerId == playerId)
		{
			if (character != m_Occupant)
			{
				Print(string.Format("ELIFE_ATMComponent | open refused, player %1 holds the lock with another character", playerId), LogLevel.WARNING);
				link.SendGrant(GetRplId(), false, ELIFE_EATMReason.IN_USE);
				return;
			}

			m_bClosePending = false;
			m_iLastRequestTick = System.GetTickCount();
			link.SendGrant(GetRplId(), true, ELIFE_EATMReason.NONE);
			if (m_sRealAccountsJson != "")
				link.SendAccounts(GetRplId(), m_sRealAccountsJson);

			if (m_sRealJson != "")
				link.SendPayload(GetRplId(), m_sRealJson);

			return;
		}

		string characterId = ELIFE_CharacterIdentity.GetCharacterId(playerId);
		if (characterId == "")
		{
			Print(string.Format("ELIFE_ATMComponent | open refused, player %1 has no character id", playerId), LogLevel.WARNING);
			link.SendGrant(GetRplId(), false, ELIFE_EATMReason.DATA_UNAVAILABLE);
			return;
		}

		m_iOccupantPlayerId = playerId;
		m_Occupant = character;
		m_sOccupantCharacterId = characterId;
		m_iLastRequestTick = System.GetTickCount();
		m_iLastRefreshTick = m_iLastRequestTick;
		m_iState = ELIFE_EATMState.LOADING;
		Replication.BumpMe();

		//! onRplName doesn't fire on the authority itself.
		OnOccupantUpdated();
		OnStateUpdated();

		Print(string.Format("ELIFE_ATMComponent | %1 locked by player %2", GetMachineId(), playerId), LogLevel.NORMAL);
		link.SendGrant(GetRplId(), true, ELIFE_EATMReason.NONE);

		GetGame().GetCallqueue().CallLater(Tick, TICK_INTERVAL_MS, true);
		StartLoad();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. During a withdrawal the release waits until it has finished.
	void RequestClose(int playerId)
	{
		if (playerId == 0 || playerId != m_iOccupantPlayerId || m_bShowingPaid)
			return;

		if (m_Withdrawal)
		{
			m_bClosePending = true;
			return;
		}

		Release("closed by the occupant");
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only.
	void KeepAlive(int playerId, notnull IEntity character)
	{
		if (IsOccupant(playerId, character))
			m_iLastRequestTick = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Throttled, since every load is three API calls.
	void RequestRefresh(int playerId, notnull IEntity character)
	{
		if (!IsOccupant(playerId, character))
			return;

		int now = System.GetTickCount();
		m_iLastRequestTick = now;
		//! Try again must always answer; StartLoad() already runs one load at a time.
		bool loadProblem = m_iState == ELIFE_EATMState.LOAD_FAILED || m_iState == ELIFE_EATMState.NO_ACCOUNT;
		if (!loadProblem && now - m_iLastRefreshTick < REFRESH_MIN_INTERVAL_MS)
			return;

		m_iLastRefreshTick = now;
		if (!m_Withdrawal && m_iState == ELIFE_EATMState.LOAD_FAILED)
			SetState(ELIFE_EATMState.LOADING);

		StartLoad();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. The number came from a client, so it only counts if the last load found it among this character's accounts.
	void RequestSelectAccount(int playerId, notnull IEntity character, string accountNumber)
	{
		if (m_aAccountNumbers.Contains(accountNumber))
			ChangeAccount(playerId, character, accountNumber);
		else
			Print(string.Format("ELIFE_ATMComponent | player %1 chose account %2, which is not on their list", playerId, accountNumber), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Back to the account list.
	void RequestChangeAccount(int playerId, notnull IEntity character)
	{
		ChangeAccount(playerId, character, "");
	}

	//------------------------------------------------------------------------------------------------
	//! A quote is for one account's fees, so it goes too. A dropped request is safe: the client only follows the state.
	protected void ChangeAccount(int playerId, notnull IEntity character, string accountNumber)
	{
		if (!IsOccupant(playerId, character) || m_Withdrawal || m_bShowingPaid)
			return;

		int now = System.GetTickCount();
		m_iLastRequestTick = now;
		if (now - m_iLastRefreshTick < ACCOUNT_CHANGE_MIN_INTERVAL_MS)
			return;

		m_iLastRefreshTick = now;
		m_sChosenAccountNumber = accountNumber;
		m_iQuoteId = 0;
		ClearLoadedAccount();
		SetState(ELIFE_EATMState.LOADING);
		StartLoad();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearLoadedAccount()
	{
		m_sAccountNumber = "";
		m_bFeesKnown = false;
		m_fLoadedBalance = 0;
		m_sRealJson = "";
		m_sRedactedJson = "";
		SetBankName("");
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. The fee here is a preview; the backend's own figure is the final one.
	void RequestQuote(int playerId, notnull IEntity character, notnull ELIFE_ATMCharacterComponent link, int amount)
	{
		ELIFE_EATMReason reason = ValidateQuote(playerId, character, amount);
		if (reason != ELIFE_EATMReason.NONE)
		{
			Print(string.Format("ELIFE_ATMComponent | quote refused for player %1, amount %2, reason %3", playerId, amount, typename.EnumToString(ELIFE_EATMReason, reason)), LogLevel.WARNING);
			link.SendQuoteRefused(GetRplId(), reason, amount);
			return;
		}

		m_iLastRequestTick = System.GetTickCount();
		m_iQuoteId = m_iNextQuoteId;
		m_iNextQuoteId++;
		m_iQuoteAmount = amount;
		m_fQuoteFee = m_fFeeBase + amount * m_fFeeMultiplier;
		m_iQuoteTick = m_iLastRequestTick;

		link.SendQuote(GetRplId(), m_iQuoteId, amount, ELIFE_PhoneBankingService.ToCents(m_fQuoteFee), ELIFE_PhoneBankingService.ToCents(amount + m_fQuoteFee));
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Consumes the quote before anything else can happen, so a repeat finds nothing to confirm.
	void RequestConfirm(int playerId, notnull IEntity character, notnull ELIFE_ATMCharacterComponent link, int quoteId)
	{
		if (quoteId != 0 && quoteId == m_iLastConsumedQuoteId)
		{
			Print(string.Format("ELIFE_ATMComponent | repeated confirm of quote %1 from player %2 ignored", quoteId, playerId), LogLevel.NORMAL);
			return;
		}

		ELIFE_EATMReason reason = ValidateConfirm(playerId, character, quoteId);
		if (reason != ELIFE_EATMReason.NONE)
		{
			Print(string.Format("ELIFE_ATMComponent | confirm refused for player %1, quote %2, reason %3", playerId, quoteId, typename.EnumToString(ELIFE_EATMReason, reason)), LogLevel.WARNING);
			link.SendResult(GetRplId(), reason, m_iQuoteAmount, 0, 0, false);
			return;
		}

		m_iLastRequestTick = System.GetTickCount();
		m_iLastConsumedQuoteId = quoteId;
		m_iQuoteId = 0;

		m_Withdrawal = new ELIFE_ATMWithdrawal();
		m_Withdrawal.m_sMachineId = GetMachineId();
		m_Withdrawal.m_iPlayerId = playerId;
		m_Withdrawal.m_Character = character;
		m_Withdrawal.m_sCharacterId = m_sOccupantCharacterId;
		m_Withdrawal.m_sAccountNumber = m_sAccountNumber;
		m_Withdrawal.m_iAmount = m_iQuoteAmount;
		m_Withdrawal.m_fQuotedFee = m_fQuoteFee;
		m_Withdrawal.m_fFeeBase = m_fFeeBase;
		m_Withdrawal.m_fFeeMultiplier = m_fFeeMultiplier;
		s_aBusyAccounts.Insert(m_sAccountNumber);
		SetState(ELIFE_EATMState.PROCESSING);

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			AbortWithdrawal(ELIFE_EATMReason.DATA_UNAVAILABLE, "API not available");
			return;
		}

		m_FlowCallback = api.GetBankAccount(m_sAccountNumber, this, "OnBaselineAccountFetched");
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_EATMReason ValidateQuote(int playerId, notnull IEntity character, int amount)
	{
		if (!IsOccupant(playerId, character))
			return ELIFE_EATMReason.NOT_OCCUPANT;

		if (m_Withdrawal || m_bShowingPaid)
			return ELIFE_EATMReason.BUSY;

		if (!IsUsableBy(character))
			return ELIFE_EATMReason.OUT_OF_RANGE;

		if (amount <= 0)
			return ELIFE_EATMReason.INVALID_AMOUNT;

		if (amount > MAX_WITHDRAWAL)
			return ELIFE_EATMReason.AMOUNT_TOO_HIGH;

		if (m_sAccountNumber == "")
			return ELIFE_EATMReason.NO_ACCOUNT;

		if (!m_bFeesKnown)
			return ELIFE_EATMReason.DATA_UNAVAILABLE;

		if (m_fLoadedBalance < amount + m_fFeeBase + amount * m_fFeeMultiplier)
			return ELIFE_EATMReason.INSUFFICIENT_FUNDS;

		return ELIFE_EATMReason.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_EATMReason ValidateConfirm(int playerId, notnull IEntity character, int quoteId)
	{
		if (!IsOccupant(playerId, character))
			return ELIFE_EATMReason.NOT_OCCUPANT;

		if (m_Withdrawal || m_bShowingPaid)
			return ELIFE_EATMReason.BUSY;

		if (quoteId == 0 || quoteId != m_iQuoteId || System.GetTickCount() - m_iQuoteTick > QUOTE_LIFETIME_MS)
			return ELIFE_EATMReason.QUOTE_INVALID;

		if (!IsUsableBy(character))
			return ELIFE_EATMReason.OUT_OF_RANGE;

		if (s_aBusyAccounts.Contains(m_sAccountNumber))
			return ELIFE_EATMReason.BUSY;

		return ELIFE_EATMReason.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsOccupant(int playerId, IEntity character)
	{
		return playerId != 0 && playerId == m_iOccupantPlayerId && character == m_Occupant;
	}

	//------------------------------------------------------------------------------------------------
	//! Polled, as nothing here needs a game-mode hook: a disconnect or a deleted character both show up as a changed controlled entity.
	protected void Tick()
	{
		if (m_Withdrawal)
			return;

		if (!IsOccupantConnected())
		{
			Release("the occupant disconnected or the character is gone");
			return;
		}

		//! The machine stays taken until EndPaidHold, wherever the player walks.
		if (m_bShowingPaid)
			return;

		if (!IsUsableBy(m_Occupant))
		{
			Release("the occupant died or left the range");
			return;
		}

		if (System.GetTickCount() - m_iLastRequestTick > IDLE_TIMEOUT_MS)
			Release("no request from the occupant");
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsOccupantConnected()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !m_Occupant)
			return false;

		return playerManager.IsPlayerConnected(m_iOccupantPlayerId) && playerManager.GetPlayerControlledEntity(m_iOccupantPlayerId) == m_Occupant;
	}

	//------------------------------------------------------------------------------------------------
	//! Never called during a withdrawal. A load still in flight notices the missing occupant and stops.
	protected void Release(string why)
	{
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(EndPaidHold);

		Print(string.Format("ELIFE_ATMComponent | %1 released by player %2: %3", GetMachineId(), m_iOccupantPlayerId, why), LogLevel.NORMAL);

		m_iOccupantPlayerId = 0;
		m_Occupant = null;
		m_sOccupantCharacterId = "";
		m_bClosePending = false;
		m_iQuoteId = 0;
		m_sAccountNumber = "";
		m_sAccountBankId = "";
		m_sAccountJson = "";
		m_bFeesKnown = false;
		m_fLoadedBalance = 0;
		m_sRealJson = "";
		m_sRedactedJson = "";
		m_aAccountNumbers.Clear();
		m_LoadedAccounts = null;
		m_sBankName = "";
		m_sChosenAccountNumber = "";
		m_sRealAccountsJson = "";
		m_sRedactedAccountsJson = "";
		m_bShowingPaid = false;
		m_iMirrorScreen = 0;
		m_iMirrorPage = 0;
		m_bMirrorTyped = false;
		m_iMirrorReason = 0;
		m_iState = ELIFE_EATMState.FREE;
		Replication.BumpMe();

		//! onRplName doesn't fire on the authority itself.
		OnOccupantUpdated();
		OnStateUpdated();
	}

	//------------------------------------------------------------------------------------------------
	//! Before the result goes out: on a host the answer arrives inside the send. Covers the client's wait before it shows the result.
	protected void BeginPaidHold()
	{
		m_bShowingPaid = true;
		Replication.BumpMe();

		GetGame().GetCallqueue().CallLater(EndPaidHold, PROCESSING_MIN_MS + PAID_RESULT_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void EndPaidHold()
	{
		Release("the paid result has been shown");
	}

	//------------------------------------------------------------------------------------------------
	protected void SetState(ELIFE_EATMState newState)
	{
		m_iState = newState;
		Replication.BumpMe();

		//! onRplName doesn't fire on the authority itself.
		OnStateUpdated();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOccupantUpdated()
	{
		if (m_Screen)
			m_Screen.OnOccupancyChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMirrorUpdated()
	{
		if (m_Screen)
			m_Screen.OnMirrorChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStateUpdated()
	{
		if (m_Screen)
			m_Screen.OnStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected RplId GetRplId()
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ATMCharacterComponent GetOccupantLink()
	{
		if (!m_Occupant)
			return null;

		return ELIFE_ATMCharacterComponent.Cast(m_Occupant.FindComponent(ELIFE_ATMCharacterComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected bool MoneyMatches(float a, float b)
	{
		float tolerance = Math.Max(MONEY_TOLERANCE, Math.Max(Math.AbsFloat(a), Math.AbsFloat(b)) * MONEY_RELATIVE_TOLERANCE);
		return Math.AbsFloat(a - b) <= tolerance;
	}

	//------------------------------------------------------------------------------------------------
	//! The bodies of the bare-array routes aren't parsed by the callback, so a bad one would read as an empty list.
	protected bool IsArrayBody(string body)
	{
		return body.StartsWith("[");
	}

	//------------------------------------------------------------------------------------------------
	//! One sequential load at a time: accounts, banks, transactions. A request during a load re-runs it once it lands.
	protected void StartLoad()
	{
		if (m_bLoadInFlight)
		{
			m_bLoadStale = true;
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			Print("ELIFE_ATMComponent | load failed, API not available", LogLevel.WARNING);
			EndLoad(ELIFE_EATMState.LOAD_FAILED);
			return;
		}

		m_bLoadInFlight = true;
		m_bLoadStale = false;
		m_LoadCallback = api.ListCharacterBankAccounts(m_sOccupantCharacterId, this, "OnLoadAccounts");
	}

	//------------------------------------------------------------------------------------------------
	//! A running withdrawal keeps its own state; it sets the idle one when it finishes.
	protected void EndLoad(ELIFE_EATMState resultState)
	{
		m_bLoadInFlight = false;

		if (!m_Withdrawal)
			SetState(resultState);
	}

	//------------------------------------------------------------------------------------------------
	//! True if this load's answer is no longer wanted: the occupant left, or a newer request is waiting.
	protected bool DropStaleLoad()
	{
		if (m_iOccupantPlayerId == 0)
		{
			m_bLoadInFlight = false;
			return true;
		}

		if (!m_bLoadStale)
			return false;

		m_bLoadInFlight = false;
		StartLoad();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void OnLoadAccounts(ELIFE_EApiStatusCode status, JsonApiStruct data, string characterId)
	{
		if (DropStaleLoad())
			return;

		if (status != ELIFE_EApiStatusCode.SUCCESS || !m_LoadCallback || !IsArrayBody(m_LoadCallback.GetData()))
		{
			Print(string.Format("ELIFE_ATMComponent | load failed, no account list for character %1", characterId), LogLevel.WARNING);
			EndLoad(ELIFE_EATMState.LOAD_FAILED);
			return;
		}

		ELIFE_BankAccountListDto list = new ELIFE_BankAccountListDto();
		list.ExpandFromArray(m_LoadCallback.GetData());

		//! The read has no ownership check of its own.
		m_LoadedAccounts = new ELIFE_BankAccountListDto();
		foreach (ELIFE_BankAccountSummary account : list.items)
		{
			if (account && account.characterId == characterId && account.number != "" && !account.IsCompany())
				m_LoadedAccounts.items.Insert(account.Copy());
		}

		if (m_LoadedAccounts.items.IsEmpty())
		{
			PublishAccounts(m_LoadedAccounts, null);
			ClearLoadedAccount();
			EndLoad(ELIFE_EATMState.NO_ACCOUNT);
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			Print("ELIFE_ATMComponent | load failed, API not available", LogLevel.WARNING);
			EndLoad(ELIFE_EATMState.LOAD_FAILED);
			return;
		}

		m_LoadCallback = api.ListBanks(this, "OnLoadBanks");
	}

	//------------------------------------------------------------------------------------------------
	//! A lone account needs no choosing. Null means the player has to choose.
	protected ELIFE_BankAccountSummary PickAccount(notnull ELIFE_BankAccountListDto ownAccounts)
	{
		if (ownAccounts.items.Count() == 1)
			return ownAccounts.items[0];

		foreach (ELIFE_BankAccountSummary account : ownAccounts.items)
		{
			if (account.number == m_sChosenAccountNumber)
				return account;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Bystanders get stand-in numbers and balances, and no ids. Bank names are public.
	protected void PublishAccounts(notnull ELIFE_BankAccountListDto ownAccounts, ELIFE_BankListDto banks)
	{
		m_aAccountNumbers.Clear();

		ELIFE_ATMAccountList real = new ELIFE_ATMAccountList();
		ELIFE_ATMAccountList redacted = new ELIFE_ATMAccountList();
		foreach (ELIFE_BankAccountSummary account : ownAccounts.items)
		{
			m_aAccountNumbers.Insert(account.number);

			string bankName = "";
			ELIFE_BankSummary bank = FindBank(banks, account.bankId);
			if (bank)
				bankName = bank.name;

			real.items.Insert(new ELIFE_ATMAccountChoice(account.number, account.balance, bankName));
			redacted.items.Insert(new ELIFE_ATMAccountChoice(ELIFE_DataRedactor.RedactDigits(account.number), ELIFE_DataRedactor.RedactAmount(account.balance), bankName));
		}

		real.Pack();
		m_sRealAccountsJson = real.AsString();
		redacted.Pack();
		m_sRedactedAccountsJson = redacted.AsString();

		ELIFE_ATMCharacterComponent link = GetOccupantLink();
		if (link)
			link.SendAccounts(GetRplId(), m_sRealAccountsJson);

		Rpc(RpcDo_RedactedAccounts, m_sRedactedAccountsJson);

		//! A broadcast never reaches the authority's own view, which only a host has.
		if (m_Screen && SCR_PlayerController.GetLocalPlayerId() != 0)
			m_Screen.OnRedactedAccounts(m_sRedactedAccountsJson);
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_BankSummary FindBank(ELIFE_BankListDto banks, string bankId)
	{
		if (!banks)
			return null;

		foreach (ELIFE_BankSummary bank : banks.items)
		{
			if (bank && bank.bankId == bankId)
				return bank;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The name in the screen's header. Empty until an account is chosen.
	protected void SetBankName(string bankName)
	{
		m_sBankName = bankName;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RedactedAccounts(string json)
	{
		if (m_Screen)
			m_Screen.OnRedactedAccounts(json);
	}

	//------------------------------------------------------------------------------------------------
	string GetRedactedAccountsJson()
	{
		return m_sRedactedAccountsJson;
	}

	//------------------------------------------------------------------------------------------------
	//! Without the bank list the statement still shows, but quotes are refused and the banks go unnamed.
	void OnLoadBanks(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		if (DropStaleLoad())
			return;

		ELIFE_BankListDto banks;
		if (status == ELIFE_EApiStatusCode.SUCCESS && m_LoadCallback && IsArrayBody(m_LoadCallback.GetData()))
		{
			banks = new ELIFE_BankListDto();
			banks.ExpandFromArray(m_LoadCallback.GetData());
		}

		PublishAccounts(m_LoadedAccounts, banks);

		ELIFE_BankAccountSummary picked = PickAccount(m_LoadedAccounts);
		if (!picked)
		{
			ClearLoadedAccount();
			EndLoad(ELIFE_EATMState.CHOOSE_ACCOUNT);
			return;
		}

		m_sAccountNumber = picked.number;
		m_sAccountBankId = picked.bankId;

		ELIFE_BankAccountSummary copy = picked.Copy();
		copy.Pack();
		m_sAccountJson = copy.AsString();

		m_bFeesKnown = false;
		ELIFE_BankSummary bank = FindBank(banks, m_sAccountBankId);
		if (bank)
		{
			m_fFeeBase = bank.transactionFeeBase;
			m_fFeeMultiplier = bank.transactionFeeMultiplier;
			m_bFeesKnown = true;
			SetBankName(bank.name);
		}
		else
		{
			Print(string.Format("ELIFE_ATMComponent | fee terms of bank %1 not available, quotes will be refused", m_sAccountBankId), LogLevel.WARNING);
			SetBankName("");
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			Print("ELIFE_ATMComponent | load failed, API not available", LogLevel.WARNING);
			EndLoad(ELIFE_EATMState.LOAD_FAILED);
			return;
		}

		m_LoadCallback = api.ListBankAccountTransactions(m_sAccountNumber, this, "OnLoadTransactions");
	}

	//------------------------------------------------------------------------------------------------
	void OnLoadTransactions(ELIFE_EApiStatusCode status, JsonApiStruct data, string accountNumber)
	{
		if (DropStaleLoad())
			return;

		if (status != ELIFE_EApiStatusCode.SUCCESS || !m_LoadCallback || !IsArrayBody(m_LoadCallback.GetData()))
		{
			Print(string.Format("ELIFE_ATMComponent | load failed, no transactions for account %1", accountNumber), LogLevel.WARNING);
			EndLoad(ELIFE_EATMState.LOAD_FAILED);
			return;
		}

		ELIFE_BankStatementDto statement = new ELIFE_BankStatementDto();
		statement.ExpandFromArray(m_LoadCallback.GetData(), "\"account\":" + m_sAccountJson + ",");
		PublishStatement(statement);
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the load. The statement must already hold its raw JSON.
	protected void PublishStatement(notnull ELIFE_BankStatementDto statement)
	{
		m_sRealJson = statement.GetRawJson();
		m_fLoadedBalance = statement.account.balance;

		//! Redacted once per load, so bystanders and late joiners all see the same stand-in figures.
		ELIFE_BankStatementDto redacted = statement.Redact();
		redacted.Pack();
		m_sRedactedJson = redacted.AsString();

		EndLoad(ELIFE_EATMState.READY);

		ELIFE_ATMCharacterComponent link = GetOccupantLink();
		if (link)
			link.SendPayload(GetRplId(), m_sRealJson);

		Rpc(RpcDo_RedactedPayload, m_sRedactedJson);

		//! A broadcast never reaches the authority's own view, which only a host has.
		if (m_Screen && SCR_PlayerController.GetLocalPlayerId() != 0)
			m_Screen.OnRedactedPayload(m_sRedactedJson);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RedactedPayload(string json)
	{
		if (m_Screen)
			m_Screen.OnRedactedPayload(json);
	}

	//------------------------------------------------------------------------------------------------
	void OnBaselineAccountFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string accountNumber)
	{
		if (!m_Withdrawal)
		{
			Print(string.Format("ELIFE_ATMComponent | baseline account %1 arrived with no withdrawal running", accountNumber), LogLevel.ERROR);
			return;
		}

		ELIFE_BankAccountSummary account = ELIFE_BankAccountSummary.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !account || account.number != accountNumber)
		{
			AbortWithdrawal(ELIFE_EATMReason.DATA_UNAVAILABLE, "baseline account read failed");
			return;
		}

		if (account.characterId != m_Withdrawal.m_sCharacterId)
		{
			AbortWithdrawal(ELIFE_EATMReason.DENIED, "account is not the occupant's");
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			AbortWithdrawal(ELIFE_EATMReason.DATA_UNAVAILABLE, "API not available");
			return;
		}

		m_Withdrawal.m_fBaseline = account.balance;
		m_FlowCallback = api.ListBankAccountTransactions(accountNumber, this, "OnBaselineTransactionsFetched");
	}

	//------------------------------------------------------------------------------------------------
	//! Last stop before the one PUT.
	void OnBaselineTransactionsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string accountNumber)
	{
		if (!m_Withdrawal)
		{
			Print(string.Format("ELIFE_ATMComponent | baseline rows of %1 arrived with no withdrawal running", accountNumber), LogLevel.ERROR);
			return;
		}

		if (status != ELIFE_EApiStatusCode.SUCCESS || !m_FlowCallback || !IsArrayBody(m_FlowCallback.GetData()))
		{
			AbortWithdrawal(ELIFE_EATMReason.DATA_UNAVAILABLE, "baseline transactions read failed");
			return;
		}

		ELIFE_BankStatementDto rows = new ELIFE_BankStatementDto();
		rows.ExpandFromArray(m_FlowCallback.GetData());

		if (!rows.items.IsEmpty())
		{
			m_Withdrawal.m_sBaselineAt = rows.items[0].occurredAt;
			if (m_Withdrawal.m_sBaselineAt == "")
			{
				AbortWithdrawal(ELIFE_EATMReason.DATA_UNAVAILABLE, "baseline transactions unreadable");
				return;
			}
		}

		if (m_Withdrawal.m_fBaseline < m_Withdrawal.m_iAmount + m_Withdrawal.m_fQuotedFee)
		{
			AbortWithdrawal(ELIFE_EATMReason.INSUFFICIENT_FUNDS, "balance below amount + fee");
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			AbortWithdrawal(ELIFE_EATMReason.DATA_UNAVAILABLE, "API not available");
			return;
		}

		ELIFE_WithdrawRequest request = new ELIFE_WithdrawRequest();
		request.amount = m_Withdrawal.m_iAmount;
		request.characterId = m_Withdrawal.m_sCharacterId;

		Print(string.Format("ELIFE_ATMComponent | withdrawing: %1", m_Withdrawal.Describe()), LogLevel.NORMAL);
		m_FlowCallback = api.WithdrawFromBankAccount(request, accountNumber, this, "OnWithdrawResult");
	}

	//------------------------------------------------------------------------------------------------
	//! No idempotency, and the Bridge retries PUTs: only a 200 with figures that add up, or a 403/404, is an answer.
	void OnWithdrawResult(ELIFE_EApiStatusCode status, JsonApiStruct data, string accountNumber)
	{
		if (!m_Withdrawal)
		{
			Print(string.Format("ELIFE_ATMComponent | withdraw result for %1 arrived with no withdrawal running", accountNumber), LogLevel.ERROR);
			return;
		}

		int httpCode;
		int restResult;
		if (m_FlowCallback)
		{
			httpCode = m_FlowCallback.GetHttpCode();
			restResult = m_FlowCallback.GetRestResult();
		}

		ELIFE_TransactionResult result = ELIFE_TransactionResult.Cast(data);
		if (status == ELIFE_EApiStatusCode.SUCCESS && result)
		{
			float expected = m_Withdrawal.m_fBaseline - m_Withdrawal.m_iAmount - result.fee;
			if (MoneyMatches(result.amount, m_Withdrawal.m_iAmount) && MoneyMatches(expected, result.newBalance))
			{
				m_Withdrawal.m_fNewBalance = result.newBalance;
				m_Withdrawal.m_bBalanceKnown = true;
				PayOut(result.fee, 0);
				return;
			}

			Print(string.Format("ELIFE_ATMComponent | withdraw answered 200 with unexpected figures amount %1 fee %2 new balance %3: %4", result.amount, result.fee, result.newBalance, m_Withdrawal.Describe()), LogLevel.WARNING);
			StartReconcile();
			return;
		}

		if (status != ELIFE_EApiStatusCode.SUCCESS && (httpCode == HTTP_FORBIDDEN || httpCode == HTTP_NOT_FOUND))
		{
			AbortWithdrawal(ELIFE_EATMReason.DENIED, string.Format("withdraw refused, HTTP=%1", httpCode));
			return;
		}

		Print(string.Format("ELIFE_ATMComponent | withdraw outcome unknown, RestResult=%1 HTTP=%2: %3", restResult, httpCode, m_Withdrawal.Describe()), LogLevel.WARNING);
		StartReconcile();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartReconcile()
	{
		SetState(ELIFE_EATMState.CHECKING);
		GetGame().GetCallqueue().CallLater(OnReconcileDue, RECONCILE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnReconcileDue()
	{
		if (!m_Withdrawal)
		{
			Print("ELIFE_ATMComponent | reconcile came due with no withdrawal running", LogLevel.ERROR);
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			ReportUnresolved("reconcile impossible, API not available");
			return;
		}

		m_FlowCallback = api.ListBankAccountTransactions(m_Withdrawal.m_sAccountNumber, this, "OnReconcileTransactionsFetched");
	}

	//------------------------------------------------------------------------------------------------
	//! Rows are newest first, so "newer than the baseline" is everything above the baseline row. occurredAt strings
	//! vary in length, so they are matched rather than ordered. Another server's identical withdrawal in this window would count too.
	void OnReconcileTransactionsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string accountNumber)
	{
		if (!m_Withdrawal)
		{
			Print(string.Format("ELIFE_ATMComponent | reconcile rows of %1 arrived with no withdrawal running", accountNumber), LogLevel.ERROR);
			return;
		}

		if (status != ELIFE_EApiStatusCode.SUCCESS || !m_FlowCallback || !IsArrayBody(m_FlowCallback.GetData()))
		{
			ReportUnresolved("reconcile read failed");
			return;
		}

		ELIFE_BankStatementDto rows = new ELIFE_BankStatementDto();
		rows.ExpandFromArray(m_FlowCallback.GetData());

		bool baselineSeen;
		int matches;
		float firstFee;
		float otherFees;
		foreach (ELIFE_BankAccountTransaction row : rows.items)
		{
			if (m_Withdrawal.m_sBaselineAt != "" && row.occurredAt == m_Withdrawal.m_sBaselineAt)
			{
				baselineSeen = true;
				break;
			}

			if (row.kind != ELIFE_BankAccountTransaction.KIND_WITHDRAWN || row.actingCharacterId != m_Withdrawal.m_sCharacterId)
				continue;

			if (!MoneyMatches(row.amount, m_Withdrawal.m_iAmount))
				continue;

			if (matches == 0)
				firstFee = row.fee;
			else
				otherFees += row.fee;

			matches++;
		}

		//! Also catches a parse that silently came back short, and a baseline pushed out of the 30-row window.
		if (m_Withdrawal.m_sBaselineAt != "" && !baselineSeen)
		{
			ReportUnresolved("baseline row not found in the reconcile read");
			return;
		}

		if (rows.items.IsEmpty() && m_FlowCallback.GetData() != "[]")
		{
			ReportUnresolved("reconcile read unparseable");
			return;
		}

		if (matches == 0)
		{
			AbortWithdrawal(ELIFE_EATMReason.NOT_WITHDRAWN, "no matching row in the ledger");
			return;
		}

		if (matches == 1)
		{
			Print(string.Format("ELIFE_ATMComponent | reconcile found the withdrawal, fee %1: %2", firstFee, m_Withdrawal.Describe()), LogLevel.WARNING);
			PayOut(firstFee, 0);
			return;
		}

		//! Every extra row was charged in full, so its fee goes back too: the player loses exactly the one fee.
		float extra = (matches - 1) * m_Withdrawal.m_iAmount + otherFees;
		Print(string.Format("ELIFE_ATMComponent | reconcile found %1 withdrawals, returning %2: %3", matches, extra, m_Withdrawal.Describe()), LogLevel.ERROR);
		PayOut(firstFee, extra);
	}

	//------------------------------------------------------------------------------------------------
	//! The money has left the account. A crash from here to the last SpawnCash loses it - no API can prevent that yet.
	protected void PayOut(float fee, float extraCredit)
	{
		m_Withdrawal.m_fFee = fee;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity character = m_Withdrawal.m_Character;
		bool present = character && playerManager && playerManager.IsPlayerConnected(m_Withdrawal.m_iPlayerId) && playerManager.GetPlayerControlledEntity(m_Withdrawal.m_iPlayerId) == character && IsAlive(character);

		if (present)
		{
			int dropped;
			m_Withdrawal.m_iPaid = SpawnCashStacks(m_Withdrawal.m_iAmount, character, dropped);
			m_Withdrawal.m_iDropped = dropped;
			if (m_Withdrawal.m_iPaid < m_Withdrawal.m_iAmount)
				Print(string.Format("ELIFE_ATMComponent | a cash stack failed to spawn: %1", m_Withdrawal.Describe()), LogLevel.ERROR);
		}
		else
		{
			Print(string.Format("ELIFE_ATMComponent | character gone, no cash paid: %1", m_Withdrawal.Describe()), LogLevel.ERROR);
		}

		float credit = m_Withdrawal.m_iAmount - m_Withdrawal.m_iPaid + extraCredit;
		if (credit <= 0)
		{
			Print(string.Format("ELIFE_ATMComponent | paid out, fee %1: %2", fee, m_Withdrawal.Describe()), LogLevel.NORMAL);
			SendWithdrawalResult(ELIFE_EATMReason.PAID);
			FinishWithdrawal();
			return;
		}

		DepositBack(credit);
	}

	//------------------------------------------------------------------------------------------------
	//! Carried stacks fill up first, and only the rest takes new stacks. Stops at the first stack that fails to spawn.
	//! Returns what was actually paid; dropped is the part of it on the ground.
	protected int SpawnCashStacks(int amount, notnull IEntity character, out int dropped)
	{
		dropped = 0;

		int paid = ELIFE_CashComponent.AddToCarriedStacks(character, amount);
		while (paid < amount)
		{
			int stack = amount - paid;
			if (stack > ELIFE_CashComponent.MAX_AMOUNT)
				stack = ELIFE_CashComponent.MAX_AMOUNT;

			bool onGround;
			if (!ELIFE_CashComponent.SpawnCash(stack, character, onGround))
				break;

			paid += stack;
			if (onGround)
				dropped += stack;
		}

		return paid;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAlive(notnull IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	//! A deposit is charged the fee too, so it's grossed up: the credited amount equals what's owed. One try, never retried.
	protected void DepositBack(float credit)
	{
		m_Withdrawal.m_fCredit = credit;

		float divisor = 1 - m_Withdrawal.m_fFeeMultiplier;
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (divisor <= 0 || !api)
		{
			ReportUnresolved(string.Format("deposit back of %1 impossible, fee multiplier %2 or no API", credit, m_Withdrawal.m_fFeeMultiplier));
			return;
		}

		m_Withdrawal.m_fDeposit = (credit + m_Withdrawal.m_fFeeBase) / divisor;
		Print(string.Format("ELIFE_ATMComponent | depositing back, owed %1, sending %2: %3", credit, m_Withdrawal.m_fDeposit, m_Withdrawal.Describe()), LogLevel.ERROR);

		ELIFE_DepositRequest request = new ELIFE_DepositRequest();
		request.amount = m_Withdrawal.m_fDeposit;
		m_FlowCallback = api.DepositToBankAccount(request, m_Withdrawal.m_sAccountNumber, this, "OnDepositBackResult");
	}

	//------------------------------------------------------------------------------------------------
	void OnDepositBackResult(ELIFE_EApiStatusCode status, JsonApiStruct data, string accountNumber)
	{
		if (!m_Withdrawal)
		{
			Print(string.Format("ELIFE_ATMComponent | deposit result for %1 arrived with no withdrawal running", accountNumber), LogLevel.ERROR);
			return;
		}

		ELIFE_TransactionResult result = ELIFE_TransactionResult.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !result || result.amount <= 0)
		{
			int httpCode;
			if (m_FlowCallback)
				httpCode = m_FlowCallback.GetHttpCode();

			ReportUnresolved(string.Format("deposit back of %1 (owed %2) failed, HTTP=%3", m_Withdrawal.m_fDeposit, m_Withdrawal.m_fCredit, httpCode));
			return;
		}

		Print(string.Format("ELIFE_ATMComponent | deposited back %1 with fee %2, owed %3, new balance %4: %5", result.amount, result.fee, m_Withdrawal.m_fCredit, result.newBalance, m_Withdrawal.Describe()), LogLevel.ERROR);

		m_Withdrawal.m_fNewBalance = result.newBalance;
		m_Withdrawal.m_bBalanceKnown = true;
		SendWithdrawalResult(ELIFE_EATMReason.RETURNED);
		FinishWithdrawal();
	}

	//------------------------------------------------------------------------------------------------
	//! Definite: no money moved.
	protected void AbortWithdrawal(ELIFE_EATMReason reason, string why)
	{
		Print(string.Format("ELIFE_ATMComponent | withdrawal not executed, %1: %2", why, m_Withdrawal.Describe()), LogLevel.WARNING);
		SendWithdrawalResult(reason);
		FinishWithdrawal();
	}

	//------------------------------------------------------------------------------------------------
	//! Money may be in the wrong place and the mod can't tell or fix it; only staff with this log line can.
	protected void ReportUnresolved(string why)
	{
		Print(string.Format("ELIFE_ATMComponent | UNRESOLVED, contact staff, %1: %2", why, m_Withdrawal.Describe()), LogLevel.ERROR);
		SendWithdrawalResult(ELIFE_EATMReason.CONTACT_STAFF);
		FinishWithdrawal();
	}

	//------------------------------------------------------------------------------------------------
	protected void SendWithdrawalResult(ELIFE_EATMReason reason)
	{
		ELIFE_ATMCharacterComponent link = GetOccupantLink();
		if (!link)
		{
			Print(string.Format("ELIFE_ATMComponent | result %1 not delivered, occupant gone: %2", typename.EnumToString(ELIFE_EATMReason, reason), m_Withdrawal.Describe()), LogLevel.WARNING);
			return;
		}

		int amount = m_Withdrawal.m_iPaid;
		if (reason != ELIFE_EATMReason.PAID && reason != ELIFE_EATMReason.RETURNED && reason != ELIFE_EATMReason.CONTACT_STAFF)
			amount = m_Withdrawal.m_iAmount;

		if (reason == ELIFE_EATMReason.PAID)
			BeginPaidHold();

		link.SendResult(GetRplId(), reason, amount, ELIFE_PhoneBankingService.ToCents(m_Withdrawal.m_fFee), ELIFE_PhoneBankingService.ToCents(m_Withdrawal.m_fNewBalance), m_Withdrawal.m_bBalanceKnown, m_Withdrawal.m_iDropped);
	}

	//------------------------------------------------------------------------------------------------
	//! The lock comes free through a pending close or the next tick, so the idle clock restarts here.
	protected void FinishWithdrawal()
	{
		s_aBusyAccounts.RemoveItem(m_Withdrawal.m_sAccountNumber);
		m_Withdrawal = null;
		m_FlowCallback = null;
		m_iLastRequestTick = System.GetTickCount();

		if (m_bClosePending && !m_bShowingPaid)
		{
			Release("closed by the occupant during a withdrawal");
			return;
		}

		SetState(ELIFE_EATMState.READY);
		StartLoad();
	}

	//------------------------------------------------------------------------------------------------
	//! Static prop, so this should only happen at shutdown - a server restart mid-flow can't be recovered from.
	void ~ELIFE_ATMComponent()
	{
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(EndPaidHold);
		GetGame().GetCallqueue().Remove(OnReconcileDue);

		if (!m_Withdrawal)
			return;

		s_aBusyAccounts.RemoveItem(m_Withdrawal.m_sAccountNumber);
		Print(string.Format("ELIFE_ATMComponent | ATM deleted during a withdrawal, UNRESOLVED: %1", m_Withdrawal.Describe()), LogLevel.ERROR);
	}
}
