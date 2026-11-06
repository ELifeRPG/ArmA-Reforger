//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Banking", description: "Carries ATM requests to the server and its answers back to the owning client")]
class ELIFE_ATMCharacterComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The ATM is server-owned, so its traffic rides on the character the client owns. No money logic, no API handlers.
class ELIFE_ATMCharacterComponent : ScriptComponent
{
	protected static const int REDACTED_MIN_INTERVAL_MS = 1000;

	protected int m_iLastRedactedRequestTick;

	//------------------------------------------------------------------------------------------------
	void RequestOpen(notnull IEntity atm)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Open, atmId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestClose(notnull IEntity atm)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Close, atmId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestRefresh(notnull IEntity atm)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Refresh, atmId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestKeepAlive(notnull IEntity atm)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_KeepAlive, atmId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestSelectAccount(notnull IEntity atm, string accountNumber)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_SelectAccount, atmId, accountNumber);
	}

	//------------------------------------------------------------------------------------------------
	void RequestChangeAccount(notnull IEntity atm)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_ChangeAccount, atmId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestMirror(notnull IEntity atm, int screen, int page, bool typed, int reason)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Mirror, atmId, screen, page, typed, reason);
	}

	//------------------------------------------------------------------------------------------------
	void RequestQuote(notnull IEntity atm, int amount)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Quote, atmId, amount);
	}

	//------------------------------------------------------------------------------------------------
	void RequestConfirm(notnull IEntity atm, int quoteId)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Confirm, atmId, quoteId);
	}

	//------------------------------------------------------------------------------------------------
	//! For a client whose screen bound after the last broadcast.
	void RequestRedacted(notnull IEntity atm)
	{
		RplId atmId = GetAtmId(atm);
		if (atmId.IsValid())
			Rpc(RpcAsk_Redacted, atmId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side senders: on a client-owned character these route to the client, on a host's own they call directly.
	void SendGrant(RplId atmId, bool granted, int reason)
	{
		Rpc(RpcDo_Grant, atmId, granted, reason);
	}

	//------------------------------------------------------------------------------------------------
	void SendPayload(RplId atmId, string json)
	{
		Rpc(RpcDo_Payload, atmId, json);
	}

	//------------------------------------------------------------------------------------------------
	void SendAccounts(RplId atmId, string json)
	{
		Rpc(RpcDo_Accounts, atmId, json);
	}

	//------------------------------------------------------------------------------------------------
	void SendQuote(RplId atmId, int quoteId, int amount, int feeCents, int totalCents)
	{
		Rpc(RpcDo_Quote, atmId, quoteId, amount, feeCents, totalCents);
	}

	//------------------------------------------------------------------------------------------------
	void SendQuoteRefused(RplId atmId, int reason, int amount)
	{
		Rpc(RpcDo_QuoteRefused, atmId, reason, amount);
	}

	//------------------------------------------------------------------------------------------------
	void SendResult(RplId atmId, int reason, int amount, int feeCents, int balanceCents, bool balanceKnown, int droppedAmount = 0)
	{
		Rpc(RpcDo_Result, atmId, reason, amount, feeCents, balanceCents, balanceKnown, droppedAmount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Open(RplId atmId)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "open");
		int playerId = GetPlayerId();
		if (!atm || playerId == 0)
			return;

		atm.RequestOpen(playerId, GetOwner(), this);
	}

	//------------------------------------------------------------------------------------------------
	//! Not range-checked: walking away is one of the reasons to close.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Close(RplId atmId)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "close");
		if (atm)
			atm.RequestClose(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Refresh(RplId atmId)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "refresh");
		if (atm && IsInReach(atm, "refresh"))
			atm.RequestRefresh(GetPlayerId(), GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_KeepAlive(RplId atmId)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "keep-alive");
		if (atm && IsInReach(atm, "keep-alive"))
			atm.KeepAlive(GetPlayerId(), GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SelectAccount(RplId atmId, string accountNumber)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "select account");
		if (atm && IsInReach(atm, "select account"))
			atm.RequestSelectAccount(GetPlayerId(), GetOwner(), accountNumber);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ChangeAccount(RplId atmId)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "change account");
		if (atm && IsInReach(atm, "change account"))
			atm.RequestChangeAccount(GetPlayerId(), GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! No reach check: the screen keeps being mirrored after a paid withdrawal, when the player may have walked off.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Mirror(RplId atmId, int screen, int page, bool typed, int reason)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "mirror");
		if (atm)
			atm.SetMirror(GetPlayerId(), GetOwner(), screen, page, typed, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! The ATM checks the range itself and answers with a reason.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Quote(RplId atmId, int amount)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "quote");
		if (atm)
			atm.RequestQuote(GetPlayerId(), GetOwner(), this, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! The ATM checks the range itself and answers with a reason.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Confirm(RplId atmId, int quoteId)
	{
		ELIFE_ATMComponent atm = FindATM(atmId, "confirm");
		if (atm)
			atm.RequestConfirm(GetPlayerId(), GetOwner(), this, quoteId);
	}

	//------------------------------------------------------------------------------------------------
	//! Answered from the ATM's cache, throttled per character.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Redacted(RplId atmId)
	{
		int now = System.GetTickCount();
		if (m_iLastRedactedRequestTick != 0 && now - m_iLastRedactedRequestTick < REDACTED_MIN_INTERVAL_MS)
			return;

		m_iLastRedactedRequestTick = now;

		ELIFE_ATMComponent atm = FindATM(atmId, "redacted");
		if (atm)
			Rpc(RpcDo_Redacted, atmId, atm.GetRedactedJson(), atm.GetRedactedAccountsJson());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Grant(RplId atmId, bool granted, int reason)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (screen)
			screen.OnGrant(granted, reason);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Payload(RplId atmId, string json)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (screen)
			screen.OnRealPayload(json);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Accounts(RplId atmId, string json)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (screen)
			screen.OnRealAccounts(json);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Redacted(RplId atmId, string json, string accountsJson)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (!screen)
			return;

		screen.OnRedactedAccounts(accountsJson);
		screen.OnRedactedPayload(json);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Quote(RplId atmId, int quoteId, int amount, int feeCents, int totalCents)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (screen)
			screen.OnQuote(quoteId, amount, feeCents, totalCents);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_QuoteRefused(RplId atmId, int reason, int amount)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (screen)
			screen.OnQuoteRefused(reason, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Result(RplId atmId, int reason, int amount, int feeCents, int balanceCents, bool balanceKnown, int droppedAmount)
	{
		ELIFE_ATMScreenComponent screen = FindScreen(atmId);
		if (screen)
			screen.OnResult(reason, amount, feeCents, balanceCents, balanceKnown, droppedAmount);
	}

	//------------------------------------------------------------------------------------------------
	protected RplId GetAtmId(notnull IEntity atm)
	{
		RplComponent rpl = RplComponent.Cast(atm.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! Derived from the character on the server, so a client can't speak for anyone else.
	protected int GetPlayerId()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return 0;

		return playerManager.GetPlayerIdFromControlledEntity(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindEntity(RplId atmId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(atmId));
		if (!rpl)
			return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ATMComponent FindATM(RplId atmId, string request)
	{
		IEntity entity = FindEntity(atmId);
		ELIFE_ATMComponent atm;
		if (entity)
			atm = ELIFE_ATMComponent.Cast(entity.FindComponent(ELIFE_ATMComponent));

		if (!atm)
			Print(string.Format("ELIFE_ATMCharacterComponent | %1 rejected, not an ATM", request), LogLevel.WARNING);

		return atm;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInReach(notnull ELIFE_ATMComponent atm, string request)
	{
		if (atm.IsUsableBy(GetOwner()))
			return true;

		Print(string.Format("ELIFE_ATMCharacterComponent | %1 rejected, character is dead or out of range", request), LogLevel.WARNING);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ATMScreenComponent FindScreen(RplId atmId)
	{
		IEntity entity = FindEntity(atmId);
		if (!entity)
			return null;

		return ELIFE_ATMScreenComponent.Cast(entity.FindComponent(ELIFE_ATMScreenComponent));
	}
}
