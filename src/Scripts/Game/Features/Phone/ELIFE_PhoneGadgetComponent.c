//------------------------------------------------------------------------------------------------
enum EPhoneScreenState
{
	OFF,
	LOCKED,
	HOME,
	BANK,
	MAP,
	MESSAGES,
	SETTINGS,
	CONTACTS
}

//------------------------------------------------------------------------------------------------
enum ELIFE_EPhoneDataStatus
{
	IDLE,
	LOADING,
	READY,
	ERROR
}

//------------------------------------------------------------------------------------------------
//! Why a contact save didn't land, in the two cases the player can act on - see ClassifySaveFailure().
enum ELIFE_EContactSaveResult
{
	SAVED,
	INVALID_NUMBER,
	DUPLICATE,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! UNDELIVERABLE: the API accepted it, but some recipients will never get it.
enum ELIFE_EMessageSendResult
{
	SENT,
	UNDELIVERABLE,
	TOO_LONG,
	FAILED
}

//------------------------------------------------------------------------------------------------
enum ELIFE_EUnlockResult
{
	WRONG,
	BLOCKED
}

//------------------------------------------------------------------------------------------------
//! Client-side sanity check for a subscriber number; the backend has the real rule.
class ELIFE_PhoneNumber
{
	static const int DIGIT_COUNT = 8;

	protected static const string DIGITS = "0123456789";

	protected static const string SEPARATORS = " -()/.";

	//------------------------------------------------------------------------------------------------
	//! Exactly DIGIT_COUNT digits, ignoring a leading "+" and separators.
	static bool IsWellFormed(string raw)
	{
		int length = raw.Length();
		int digits = 0;
		bool sawPlus = false;

		for (int i = 0; i < length; i++)
		{
			string character = raw.Substring(i, 1);

			if (DIGITS.IndexOf(character) != -1)
			{
				digits++;
				if (digits > DIGIT_COUNT)
					return false;

				continue;
			}

			if (character == "+")
			{
				if (sawPlus || digits > 0)
					return false;

				sawPlus = true;
				continue;
			}

			if (SEPARATORS.IndexOf(character) != -1)
				continue;

			return false;
		}

		return digits == DIGIT_COUNT;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneDataEntry
{
	string m_sReal;
	string m_sRedacted;
	int m_iFetchTime;
}

//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Gadgets", description: "Handheld phone gadget")]
class ELIFE_PhoneGadgetComponentClass : SCR_GadgetComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Handheld phone gadget (SPECIALIST_ITEM). Provisioned against the backend on first equip.
class ELIFE_PhoneGadgetComponent : SCR_GadgetComponent
{
	protected const string BODY_SOURCE_MATERIAL = "Phone_Body_Graphite";
	protected const string SCREEN_SOURCE_MATERIAL = "Phone_Screen_Live";

	[Attribute("{32874067CF8A6EB2}Assets/Items/Equipment/Radios/Radio_ANPRC68/Data/Radio_ANPRC68_01.emat", UIWidgets.ResourceNamePicker, "Body material to re-assert on every screen swap (must match this variant's MeshObject body assignment).", "emat", category: "Phone")]
	protected ResourceName m_sBodyMaterial;

	[Attribute("{B1EFD30A850D7431}Assets/Items/Equipment/Phone/Data/Phone_Screen_Off.emat", UIWidgets.ResourceNamePicker, "Screen material while not activated.", "emat", category: "Phone")]
	protected ResourceName m_sScreenOffMaterial;

	[Attribute("{454687C5EE7005C8}Assets/Items/Equipment/Phone/Data/Phone_Screen_Live.emat", UIWidgets.ResourceNamePicker, "Screen material used while ELIFE_PhoneScreenRenderComponent has a render-target bound (feeds its $rendertarget slot).", "emat", category: "Phone")]
	protected ResourceName m_sScreenLiveMaterial;

	[Attribute("{0C988E40A81DDEEF}Assets/Items/Equipment/Phone/Data/Phone_Screen_LOD.emat", UIWidgets.ResourceNamePicker, "Screen material shown for any non-Off state when out of live-render range (see GetBakedScreenMaterial).", "emat", category: "Phone")]
	protected ResourceName m_sScreenLodMaterial;

	[Attribute("2", UIWidgets.EditBox, "Intensity of the emissive pulse layered on top of the active screen material.", "0 20", category: "Phone")]
	protected float m_fScreenEmissiveIntensity;

	[RplProp(onRplName: "OnPhoneIdUpdated")]
	protected string m_sPhoneId;

	//! What this machine displays: the real identity for the registered owner, redacted for everyone else.
	protected string m_sNumber;
	protected string m_sPin;
	protected bool m_bHasRealIdentity;

	//! Safe for everyone, and an RplProp so late joiners and streamed-in phones get it too.
	[RplProp(onRplName: "ApplyRedactedIdentity")]
	protected string m_sRedactedPin;

	//! Server-only. The real identity, only ever sent to the registered owner.
	protected string m_sServerNumber;
	protected string m_sServerPin;
	protected string m_sServerOwnerCharacterId;

	//! Server-only. The server is the only one allowed to take the phone off the lock screen.
	protected bool m_bPinLocked;
	protected int m_iUnlockFailures;
	protected float m_fUnlockBlockedUntil;

	protected const int UNLOCK_MAX_FAILURES = 5;
	protected const float UNLOCK_COOLDOWN_MS = 30000;

	protected ref ELIFE_ProvisionPhoneCallback m_ProvisionCallback;
	protected ref ELIFE_BaseRestCallback m_PowerOnCallback;
	protected int m_iOwnerPlayerId;

	static const string DATA_CONTACTS = "contacts";
	static const string DATA_MESSAGES = "messages";

	//! Server-only: the real payload per key plus its pre-built bystander copy.
	protected ref map<string, ref ELIFE_PhoneDataEntry> m_mServerData = new map<string, ref ELIFE_PhoneDataEntry>();
	protected ref map<string, ref ELIFE_BaseRestCallback> m_mPendingFetches = new map<string, ref ELIFE_BaseRestCallback>();

	//! What this machine got pushed - real payload for the owner, redacted copy for everyone else.
	protected ref map<string, string> m_mData = new map<string, string>();
	protected ref map<string, ELIFE_EPhoneDataStatus> m_mDataStatus = new map<string, ELIFE_EPhoneDataStatus>();

	//! Fires with the data key whenever this machine's copy or its status changes - apps re-render off it.
	ref ScriptInvoker m_OnDataChanged = new ScriptInvoker();

	//! Fires when the screen state lands on this machine.
	ref ScriptInvoker m_OnScreenStateChanged = new ScriptInvoker();

	//! Fires when the provisioned identity lands; it usually arrives after the first draw.
	ref ScriptInvoker m_OnIdentityChanged = new ScriptInvoker();

	//! Fires with ELIFE_EContactSaveResult when a SaveContact() call resolves.
	ref ScriptInvoker m_OnContactSaveResult = new ScriptInvoker();

	//! Fires with (ELIFE_EUnlockResult, retryInMs) when the server rejects a PIN. Success arrives as a state change.
	ref ScriptInvoker m_OnUnlockRejected = new ScriptInvoker();

	//! Fires with (ELIFE_EMessageSendResult, threadId) when a send resolves; threadId is empty on failure.
	ref ScriptInvoker m_OnMessageSendResult = new ScriptInvoker();

	//! True after a Bridge request got no HTTP answer at all, until one does.
	[RplProp(onRplName: "OnConnectivityUpdated")]
	protected bool m_bOffline;

	ref ScriptInvoker m_OnConnectivityChanged = new ScriptInvoker();

	//! Replicated so bystanders see the hub too.
	[RplProp(onRplName: "OnHubOpenUpdated")]
	protected bool m_bHubOpen;

	ref ScriptInvoker m_OnHubOpenChanged = new ScriptInvoker();

	//! threadId -> unread count at dismissal; a higher count notifies again. Local only, never replicated.
	protected ref map<string, int> m_mDismissedNotifications = new map<string, int>();

	//! Kept so OnContactSaved() can read the HTTP code after the base collapses failures to ERROR.
	protected ref ELIFE_SaveContactCallback m_SaveContactCallback;
	protected ref ELIFE_SendMessageCallback m_SendMessageCallback;
	protected ref ELIFE_MarkThreadReadCallback m_MarkThreadReadCallback;

	protected const int DATA_CACHE_TTL_MS = 15000;

	//! Server-only. polledAt of the last updates response; empty fetches every thread whole.
	protected string m_sMessagesCursor;

	//! Server-only. The in-flight fetch is the background poll, whose failure mustn't error the owner's app.
	protected bool m_bPollInFlight;

	protected int m_iPollIntervalMs;

	//! Polling is the only delivery path, so it stays frequent enough while holstered for a timely peek.
	protected const int POLL_INTERVAL_ACTIVE_MS = 2000;
	protected const int POLL_INTERVAL_IDLE_MS = 15000;

	[RplProp(onRplName: "OnScreenStateUpdated")]
	protected EPhoneScreenState m_eScreenState;

	//! Generic in-app navigation sync channel - see ELIFE_PhoneAppBase.GetSubState()/ApplySubState().
	[RplProp(onRplName: "OnScreenSubStateUpdated")]
	protected string m_sScreenSubState;

	protected ParametricMaterialInstanceComponent m_ScreenEmissiveMaterial;
	protected float m_fScreenPulsePhase;
	protected ELIFE_PhoneScreenRenderComponent m_ScreenRenderComponent;
	protected ELIFE_PhoneScreenInteractComponent m_ScreenInteractComponent;
	protected SoundComponent m_SoundComponent;
	protected bool m_bLiveScreenActive;
	protected ResourceName m_sAppliedScreenMaterial;
	protected EPhoneScreenState m_ePrevScreenState = EPhoneScreenState.OFF;

	//! Whether the screen was locked when it last went off, so power-on returns to LOCKED.
	protected bool m_bWasLocked;

	//! Last in-phone page. Opening again lands here instead of always Home.
	protected EPhoneScreenState m_eResumeState = EPhoneScreenState.HOME;

	//! Defined in the phone's own Phone_UI.acp (reuses vanilla UI_Task_Succeded/Canceled.wav).
	protected const string SOUND_EVENT_POWER_ON = "SOUND_PHONE_POWER_ON";
	protected const string SOUND_EVENT_POWER_OFF = "SOUND_PHONE_POWER_OFF";

	//! Played on the owner's machine when the messages poll raises their unread total - see ApplyData().
	protected const string SOUND_EVENT_NOTIFICATION = "SOUND_PHONE_NOTIFICATION";

	//! Outer LOD tier. Starts true so a client spawning nearby gets the initial state at once.
	protected bool m_bLocallySynced = true;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	//! The poll is a repeating callqueue entry, so it has to be dropped with the phone that owns it.
	override void OnDelete(IEntity owner)
	{
		StopPoll();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		m_ScreenEmissiveMaterial = ParametricMaterialInstanceComponent.Cast(owner.FindComponent(ParametricMaterialInstanceComponent));
		m_ScreenRenderComponent = ELIFE_PhoneScreenRenderComponent.Cast(owner.FindComponent(ELIFE_PhoneScreenRenderComponent));
		m_ScreenInteractComponent = ELIFE_PhoneScreenInteractComponent.Cast(owner.FindComponent(ELIFE_PhoneScreenInteractComponent));
		m_SoundComponent = SoundComponent.Cast(owner.FindComponent(SoundComponent));
	}

	//------------------------------------------------------------------------------------------------
	string GetPhoneId()
	{
		return m_sPhoneId;
	}

	//------------------------------------------------------------------------------------------------
	//! Also fires when async provisioning finishes, which is always after equip.
	protected void OnPhoneIdUpdated()
	{
		if (IsLocalCharacterOwner())
			ELIFE_PhoneToggle.RememberActivePhone(this);

		OnIdentityUpdated();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnIdentityUpdated()
	{
		m_OnIdentityChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	bool IsOffline()
	{
		return m_bOffline;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetOffline(bool offline)
	{
		if (m_bOffline == offline)
			return;

		m_bOffline = offline;
		Replication.BumpMe();

		//! onRplName does not fire on the authority itself - same caveat as OnPhoneIdUpdated().
		OnConnectivityUpdated();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnConnectivityUpdated()
	{
		m_OnConnectivityChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	// Notification hub
	//------------------------------------------------------------------------------------------------

	bool IsHubOpen()
	{
		return m_bHubOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner asks, server applies, so every screen opens the hub together.
	void SetHubOpen(bool open)
	{
		if (m_bHubOpen == open)
			return;

		Rpc(RpcAsk_SetHubOpen, open);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetHubOpen(bool open)
	{
		if (m_bHubOpen == open)
			return;

		m_bHubOpen = open;

		//! onRplName doesn't fire on the authority itself - see OnPhoneIdUpdated().
		OnHubOpenUpdated();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHubOpenUpdated()
	{
		m_OnHubOpenChanged.Invoke(m_bHubOpen);
	}

	//------------------------------------------------------------------------------------------------
	//! True while this thread's notification has been cleared and nothing new has arrived since.
	bool IsNotificationDismissed(string threadId, int unreadCount)
	{
		int dismissedAt;
		if (!m_mDismissedNotifications.Find(threadId, dismissedAt))
			return false;

		return unreadCount <= dismissedAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Records the watermark rather than a flag, so the next message on this thread notifies again.
	void DismissNotification(string threadId, int unreadCount)
	{
		if (threadId == "")
			return;

		m_mDismissedNotifications.Set(threadId, unreadCount);
	}

	//------------------------------------------------------------------------------------------------
	//! httpCode 0 means no answer at all; any real HTTP code, even an error, means the Bridge is up.
	protected void NoteConnectivity(ELIFE_EApiStatusCode status, int httpCode)
	{
		if (status == ELIFE_EApiStatusCode.SUCCESS)
		{
			SetOffline(false);
			return;
		}

		SetOffline(httpCode == 0);
	}

	//------------------------------------------------------------------------------------------------
	string GetNumber()
	{
		return m_sNumber;
	}

	//------------------------------------------------------------------------------------------------
	string GetPin()
	{
		return m_sPin;
	}

	//------------------------------------------------------------------------------------------------
	//! Dev-only PIN generation. The server keeps it to verify unlocks; the owner sees it in Settings.
	protected void ProvisionPhone(int playerId)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
			return;

		string characterId = ELIFE_CharacterIdentity.GetCharacterId(playerId);
		m_sServerOwnerCharacterId = characterId;
		string pin = string.Format("%1", Math.RandomIntInclusive(1000, 9999));
		string body = string.Format("{\"characterId\":\"%1\",\"pin\":\"%2\"}", characterId, pin);

		m_ProvisionCallback = new ELIFE_ProvisionPhoneCallback();
		m_ProvisionCallback.SetCallback(this, "OnPhoneProvisioned", pin);
		api.GetElifeApi().POST(m_ProvisionCallback, "phones", body);
	}

	//------------------------------------------------------------------------------------------------
	void OnPhoneProvisioned(ELIFE_EApiStatusCode status, JsonApiStruct data, string pin)
	{
		int httpCode = 0;
		if (m_ProvisionCallback)
			httpCode = m_ProvisionCallback.GetHttpCode();
		NoteConnectivity(status, httpCode);

		ELIFE_ProvisionPhoneResponseDto response = ELIFE_ProvisionPhoneResponseDto.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !response)
		{
			Print("ELIFE_PhoneGadgetComponent | provisioning failed", LogLevel.ERROR);
			return;
		}

		m_sPhoneId = response.phoneId;
		m_sServerNumber = response.number;
		m_sServerPin = pin;
		m_sRedactedPin = ELIFE_DataRedactor.RedactDigits(pin);
		Replication.BumpMe();

		//! onRplName doesn't fire on the machine setting the value, so apply it here for a listen server.
		OnPhoneIdUpdated();

		PushIdentityToOwner();
		if (SCR_PlayerController.GetLocalControlledEntity())
			ApplyRedactedIdentity();

		//! Apps need the phone powered on in the backend, and a new phone starts off.
		PowerOnPhone();
	}

	//------------------------------------------------------------------------------------------------
	protected void PowerOnPhone()
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
			return;

		string characterId = ELIFE_CharacterIdentity.GetCharacterId(m_iOwnerPlayerId);
		string body = string.Format("{\"characterId\":\"%1\",\"isPoweredOn\":true}", characterId);

		m_PowerOnCallback = new ELIFE_BaseRestCallback();
		m_PowerOnCallback.SetCallback(this, "OnPhonePoweredOn");
		api.GetElifeApi().POST(m_PowerOnCallback, string.Format("phones/%1/power", m_sPhoneId), body);
	}

	//------------------------------------------------------------------------------------------------
	void OnPhonePoweredOn(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		int httpCode = 0;
		if (m_PowerOnCallback)
			httpCode = m_PowerOnCallback.GetHttpCode();
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print("ELIFE_PhoneGadgetComponent | power-on failed", LogLevel.ERROR);
			return;
		}

		//! First chance to poll — don't wait a full idle interval for the opening screen to have mail.
		ReschedulePoll();
		PollMessages();
	}

	//------------------------------------------------------------------------------------------------
	// Messages poll - the only delivery path, since the SignalR hub isn't reachable from here
	//------------------------------------------------------------------------------------------------

	//! Server-only, and re-run whenever the screen state lands, since the cadence follows it.
	protected void ReschedulePoll()
	{
		if (!Replication.IsServer() || m_sPhoneId == "")
			return;

		int interval = POLL_INTERVAL_IDLE_MS;
		if (m_eScreenState != EPhoneScreenState.OFF)
			interval = POLL_INTERVAL_ACTIVE_MS;

		if (interval == m_iPollIntervalMs)
			return;

		bool waking = interval == POLL_INTERVAL_ACTIVE_MS;

		m_iPollIntervalMs = interval;
		GetGame().GetCallqueue().Remove(PollMessages);
		GetGame().GetCallqueue().CallLater(PollMessages, interval, true);

		//! Screen just woke; don't wait a full interval for whatever arrived while it was off.
		if (waking)
			PollMessages();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopPoll()
	{
		m_iPollIntervalMs = 0;
		GetGame().GetCallqueue().Remove(PollMessages);
	}

	//------------------------------------------------------------------------------------------------
	protected void PollMessages()
	{
		//! A fetch is already in flight; don't relabel it as the poll.
		if (m_sPhoneId == "" || m_mPendingFetches.Contains(DATA_MESSAGES))
			return;

		m_bPollInFlight = true;
		FetchData(DATA_MESSAGES);
	}

	//------------------------------------------------------------------------------------------------
	//! Empty until the first push lands, so callers must re-render on m_OnDataChanged.
	string GetData(string key)
	{
		string json;
		m_mData.Find(key, json);
		return json;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_EPhoneDataStatus GetDataStatus(string key)
	{
		ELIFE_EPhoneDataStatus status;
		if (!m_mDataStatus.Find(key, status))
			return ELIFE_EPhoneDataStatus.IDLE;

		return status;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetDataStatus(string key, ELIFE_EPhoneDataStatus status)
	{
		m_mDataStatus.Set(key, status);
		m_OnDataChanged.Invoke(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by an app on open. Only the owner fetches; bystanders mirror the owner's broadcasts.
	void RequestData(string key)
	{
		if (!IsLocalCharacterOwner())
			return;

		//! Set locally, not pushed back - a round trip just to say "loading" often lands after the answer.
		SetDataStatus(key, ELIFE_EPhoneDataStatus.LOADING);

		Rpc(RpcAsk_RequestData, key);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestData(string key)
	{
		//! Provisioning hasn't finished (or failed) - answer anyway, the owner is stuck on LOADING otherwise.
		if (m_sPhoneId == "")
		{
			PushDataError(key);
			return;
		}

		//! An owner request adopts an in-flight poll so its failure gets reported.
		if (key == DATA_MESSAGES)
			m_bPollInFlight = false;

		ELIFE_PhoneDataEntry entry;
		if (m_mServerData.Find(key, entry) && System.GetTickCount() - entry.m_iFetchTime < DATA_CACHE_TTL_MS)
		{
			PushData(key);
			return;
		}

		FetchData(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only retry from the Offline screen: re-provision if needed, else re-fetch what errored.
	void RetryConnectivity()
	{
		if (!IsLocalCharacterOwner())
			return;

		Rpc(RpcAsk_RetryConnectivity);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RetryConnectivity()
	{
		if (m_sPhoneId == "")
		{
			ProvisionPhone(m_iOwnerPlayerId);
			return;
		}

		foreach (string key, ELIFE_EPhoneDataStatus status : m_mDataStatus)
		{
			if (status == ELIFE_EPhoneDataStatus.ERROR)
				FetchData(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Contacts write path
	//------------------------------------------------------------------------------------------------

	//! Owner-only. Sent as typed; the backend validates the number.
	void SaveContact(string number, string displayName)
	{
		if (!IsLocalCharacterOwner())
			return;

		Rpc(RpcAsk_SaveContact, number, displayName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SaveContact(string number, string displayName)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || !api)
		{
			NotifyContactSaveResult(ELIFE_EContactSaveResult.FAILED);
			return;
		}

		//! displayName is player-typed free text, so it goes through ELIFE_Json.EscapeString().
		string body = string.Format("{\"number\":\"%1\",\"displayName\":\"%2\"}",
			ELIFE_Json.EscapeString(number), ELIFE_Json.EscapeString(displayName));

		m_SaveContactCallback = new ELIFE_SaveContactCallback();
		m_SaveContactCallback.SetCallback(this, "OnContactSaved");
		api.GetElifeApi().POST(m_SaveContactCallback, string.Format("phones/%1/apps/contacts/entries", m_sPhoneId), body);
	}

	//------------------------------------------------------------------------------------------------
	void OnContactSaved(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		int httpCode = 0;
		if (m_SaveContactCallback)
			httpCode = m_SaveContactCallback.GetHttpCode();
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | contact save failed, HTTP=%1", httpCode), LogLevel.ERROR);
			NotifyContactSaveResult(ClassifySaveFailure(httpCode));
			return;
		}

		//! Bypasses the TTL cache-hit path - a cache hit here would push the stale pre-save list back.
		FetchData(DATA_CONTACTS);

		NotifyContactSaveResult(ELIFE_EContactSaveResult.SAVED);
	}

	//------------------------------------------------------------------------------------------------
	//! Self-applies locally for the owner too - RplRcver.Owner RPCs don't self-deliver on a listen server.
	protected void NotifyContactSaveResult(ELIFE_EContactSaveResult result)
	{
		Rpc(RpcDo_ContactSaveResult, result);

		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			m_OnContactSaveResult.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	// Messages write path
	//------------------------------------------------------------------------------------------------

	//! Owner-only. Wraps the fan-out send route for a single recipient.
	void SendMessage(string number, string body)
	{
		if (!IsLocalCharacterOwner())
			return;

		Rpc(RpcAsk_SendMessage, number, body);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SendMessage(string number, string body)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || !api)
		{
			NotifyMessageSendResult(ELIFE_EMessageSendResult.FAILED, "");
			return;
		}

		//! body is player-typed free text, escaped the same way SaveContact()'s displayName is.
		string requestBody = string.Format("{\"to\":[\"%1\"],\"body\":\"%2\"}",
			ELIFE_Json.EscapeString(number), ELIFE_Json.EscapeString(body));

		m_SendMessageCallback = new ELIFE_SendMessageCallback();
		m_SendMessageCallback.SetCallback(this, "OnMessageSent");
		api.GetElifeApi().POST(m_SendMessageCallback, string.Format("phones/%1/apps/messages/send", m_sPhoneId), requestBody);
	}

	//------------------------------------------------------------------------------------------------
	void OnMessageSent(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		int httpCode = 0;
		if (m_SendMessageCallback)
			httpCode = m_SendMessageCallback.GetHttpCode();
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | message send failed, HTTP=%1", httpCode), LogLevel.ERROR);
			NotifyMessageSendResult(ClassifySendFailure(httpCode), "");
			return;
		}

		ELIFE_SendMessageResponseDto response = ELIFE_SendMessageResponseDto.Cast(data);

		string threadId = "";
		ELIFE_EMessageSendResult result = ELIFE_EMessageSendResult.SENT;

		if (response)
		{
			threadId = response.threadId;

			//! One recipient per send, so any entry here means the message won't reach them.
			if (response.undeliverableRecipients.Count() > 0)
				result = ELIFE_EMessageSendResult.UNDELIVERABLE;
		}

		//! Bypasses the TTL cache-hit path - a cache hit would push the pre-send conversation back.
		FetchData(DATA_MESSAGES);

		NotifyMessageSendResult(result, threadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Self-applies locally for the owner too - RplRcver.Owner RPCs don't self-deliver on a listen server.
	protected void NotifyMessageSendResult(ELIFE_EMessageSendResult result, string threadId)
	{
		Rpc(RpcDo_MessageSendResult, result, threadId);

		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			m_OnMessageSendResult.Invoke(result, threadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Body too long is actionable; everything else is "try again".
	protected ELIFE_EMessageSendResult ClassifySendFailure(int httpCode)
	{
		if (httpCode == 400 || httpCode == 413)
			return ELIFE_EMessageSendResult.TOO_LONG;

		return ELIFE_EMessageSendResult.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MessageSendResult(ELIFE_EMessageSendResult result, string threadId)
	{
		m_OnMessageSendResult.Invoke(result, threadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only, fire-and-forget: clears a thread's unread count server-side. Nothing on screen waits on the answer.
	void MarkThreadRead(string threadId)
	{
		if (!IsLocalCharacterOwner() || threadId == "")
			return;

		Rpc(RpcAsk_MarkThreadRead, threadId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MarkThreadRead(string threadId)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || threadId == "" || !api)
			return;

		m_MarkThreadReadCallback = new ELIFE_MarkThreadReadCallback();
		m_MarkThreadReadCallback.SetCallback(this, "OnThreadMarkedRead");

		//! Empty object, not empty string - RestContext still needs something to send with a POST.
		api.GetElifeApi().POST(m_MarkThreadReadCallback, string.Format("phones/%1/apps/messages/threads/%2/read", m_sPhoneId, threadId), "{}");
	}

	//------------------------------------------------------------------------------------------------
	void OnThreadMarkedRead(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		int httpCode = 0;
		if (m_MarkThreadReadCallback)
			httpCode = m_MarkThreadReadCallback.GetHttpCode();
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | mark thread read failed, HTTP=%1", httpCode), LogLevel.WARNING);
			return;
		}

		//! Unread counts come from the payload, so re-fetch fully - a cursor poll wouldn't report this thread again.
		FetchData(DATA_MESSAGES, true);
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_EContactSaveResult ClassifySaveFailure(int httpCode)
	{
		if (httpCode == 400)
			return ELIFE_EContactSaveResult.INVALID_NUMBER;

		if (httpCode == 409)
			return ELIFE_EContactSaveResult.DUPLICATE;

		return ELIFE_EContactSaveResult.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ContactSaveResult(ELIFE_EContactSaveResult result)
	{
		m_OnContactSaveResult.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Owns each key's route and response parsing. full forces a cursor-less messages fetch.
	protected void FetchData(string key, bool full = false)
	{
		//! A second fetch would just overwrite the pending callback and waste a round trip.
		if (m_mPendingFetches.Contains(key))
			return;

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			PushDataError(key);
			return;
		}

		string route;
		ELIFE_BaseRestCallback callback;

		if (key == DATA_CONTACTS)
		{
			route = "contacts/entries";
			callback = new ELIFE_ContactsFetchCallback();
		}
		else if (key == DATA_MESSAGES)
		{
			//! Cursor-less returns every thread with bodies; with a cursor only what arrived since, merged in OnDataFetched().
			if (full || m_sMessagesCursor == "")
				route = "messages/updates";
			else
				route = string.Format("messages/updates?since=%1", EncodeCursor(m_sMessagesCursor));

			callback = new ELIFE_MessageUpdatesFetchCallback();
		}
		else
		{
			PushDataError(key);
			return;
		}

		callback.SetCallback(this, "OnDataFetched", key);
		m_mPendingFetches.Set(key, callback);

		//! Contacts/messages routes take no characterId/pin - possession is proven once at power-on.
		api.GetElifeApi().GET(callback, string.Format("phones/%1/apps/%2", m_sPhoneId, route));
	}

	//------------------------------------------------------------------------------------------------
	void OnDataFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string key)
	{
		ELIFE_BaseRestCallback callback;
		m_mPendingFetches.Find(key, callback);
		m_mPendingFetches.Remove(key);

		bool wasPoll = key == DATA_MESSAGES && m_bPollInFlight;
		if (key == DATA_MESSAGES)
			m_bPollInFlight = false;

		int httpCode = 0;
		if (callback)
			httpCode = callback.GetHttpCode();
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS || !data)
		{
			//! A background poll failure only warns; the Offline screen already covers connectivity.
			if (wasPoll)
			{
				Print("ELIFE_PhoneGadgetComponent | messages poll failed", LogLevel.WARNING);
				return;
			}

			Print(string.Format("ELIFE_PhoneGadgetComponent | fetch failed for '%1'", key), LogLevel.ERROR);
			PushDataError(key);
			return;
		}

		//! Re-packing would warn "Operation called twice", so use the raw text it was unpacked from.
		ELIFE_PhoneDataEntry entry = new ELIFE_PhoneDataEntry();
		ELIFE_PhoneJsonDto jsonDto = ELIFE_PhoneJsonDto.Cast(data);
		if (jsonDto)
			entry.m_sReal = jsonDto.GetRawJson();

		bool changed = true;
		if (key == DATA_MESSAGES)
			entry.m_sReal = MergeMessages(ELIFE_MessageUpdatesDto.Cast(data), changed);

		entry.m_sRedacted = RedactPayload(key, entry.m_sReal);
		entry.m_iFetchTime = System.GetTickCount();
		m_mServerData.Set(key, entry);

		//! A quiet poll stays silent; an owner request always answers since its app is loading.
		if (changed || !wasPoll)
			PushData(key);

		//! A messages fetch that landed first used redacted numbers as titles - rebuild it now that names exist.
		if (key == DATA_CONTACTS)
			RefreshBystanderMessages();
	}

	//------------------------------------------------------------------------------------------------
	//! Merges an updates response into the cache, advances the cursor, returns the merged JSON.
	protected string MergeMessages(ELIFE_MessageUpdatesDto response, out bool changed)
	{
		changed = true;

		if (!response)
			return "";

		m_sMessagesCursor = response.polledAt;

		ELIFE_PhoneDataEntry cached;
		if (!m_mServerData.Find(DATA_MESSAGES, cached) || cached.m_sReal == "")
			return response.GetRawJson();

		ELIFE_MessageUpdatesDto current = new ELIFE_MessageUpdatesDto();
		current.ExpandFromRAW(cached.m_sReal);

		ELIFE_MessageUpdatesDto merged = current.MergedWith(response, changed);

		//! Built fresh by MergedWith(), so this one may be Pack()ed - see ELIFE_PhoneJsonDto.
		merged.Pack();
		return merged.AsString();
	}

	//------------------------------------------------------------------------------------------------
	//! Encode "+" in the cursor offset, or it decodes as a space.
	protected string EncodeCursor(string cursor)
	{
		string encoded = "";

		int length = cursor.Length();
		for (int i = 0; i < length; i++)
		{
			string character = cursor.Substring(i, 1);
			if (character == "+")
				encoded += "%2B";
			else
				encoded += character;
		}

		return encoded;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the cached JSON: a poll response is only a delta, but bystanders mirror the merged payload.
	protected string RedactPayload(string key, string realJson)
	{
		if (realJson == "")
			return "";

		JsonApiStruct redacted;

		if (key == DATA_CONTACTS)
		{
			ELIFE_ContactListDto contacts = new ELIFE_ContactListDto();
			contacts.ExpandFromRAW(realJson);
			redacted = contacts.Redact();
		}
		else if (key == DATA_MESSAGES)
		{
			ELIFE_MessageUpdatesDto updates = new ELIFE_MessageUpdatesDto();
			updates.ExpandFromRAW(realJson);
			redacted = updates.Redact(OwnerContactNames());
		}

		if (!redacted)
			return "";

		redacted.Pack();
		return redacted.AsString();
	}

	//------------------------------------------------------------------------------------------------
	//! Number -> name from the owner's contacts; only the server-side cache still has them here.
	protected map<string, string> OwnerContactNames()
	{
		ELIFE_PhoneDataEntry entry;
		if (!m_mServerData.Find(DATA_CONTACTS, entry) || entry.m_sReal == "")
			return null;

		ELIFE_ContactListDto list = new ELIFE_ContactListDto();
		list.ExpandFromRAW(entry.m_sReal);

		map<string, string> names = new map<string, string>();
		foreach (ELIFE_ContactDto contact : list.items)
		{
			if (contact && contact.number != "")
				names.Set(contact.number, contact.displayName);
		}

		return names;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshBystanderMessages()
	{
		ELIFE_PhoneDataEntry entry;
		if (!m_mServerData.Find(DATA_MESSAGES, entry) || entry.m_sReal == "")
			return;

		entry.m_sRedacted = RedactPayload(DATA_MESSAGES, entry.m_sReal);
		PushData(DATA_MESSAGES);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner gets the real payload, everyone with the phone streamed in gets the redacted copy.
	protected void PushData(string key)
	{
		ELIFE_PhoneDataEntry entry;
		if (!m_mServerData.Find(key, entry))
			return;

		Rpc(RpcDo_DataOwner, key, entry.m_sReal);
		Rpc(RpcDo_DataBystanders, key, entry.m_sRedacted);

		//! Neither RPC reaches a listen server's own view, so apply it here.
		if (!SCR_PlayerController.GetLocalControlledEntity())
			return;

		if (IsLocalCharacterOwner())
			ApplyData(key, entry.m_sReal);
		else
			ApplyData(key, entry.m_sRedacted);
	}

	//------------------------------------------------------------------------------------------------
	//! Only the owner hears about failures; bystanders just stop getting updates.
	protected void PushDataError(string key)
	{
		Rpc(RpcDo_DataError, key);

		//! Same listen-server self-delivery caveat as PushData().
		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			SetDataStatus(key, ELIFE_EPhoneDataStatus.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DataError(string key)
	{
		SetDataStatus(key, ELIFE_EPhoneDataStatus.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! RPCs only marshal primitives, so payloads cross as packed JSON rather than as DTOs.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DataOwner(string key, string json)
	{
		ApplyData(key, json);
	}

	//------------------------------------------------------------------------------------------------
	//! No RplCondition - its behaviour on RPCs is unverified. The owner is skipped explicitly below.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_DataBystanders(string key, string json)
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl && rpl.IsOwner())
			return;

		ApplyData(key, json);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyData(string key, string json)
	{
		//! Decided before the swap - the comparison is against what this machine was already holding.
		bool announce = key == DATA_MESSAGES && ShouldAnnounce(json);

		m_mData.Set(key, json);
		SetDataStatus(key, ELIFE_EPhoneDataStatus.READY);

		if (!announce)
			return;

		if (m_SoundComponent)
			m_SoundComponent.SoundEvent(SOUND_EVENT_NOTIFICATION);

		if (ELIFE_PhonePrefs.IsPeekEnabled())
			ELIFE_PhonePeek.Show(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Announce when the owner's unread total rises. Counting unread avoids double-announcing re-deliveries.
	protected bool ShouldAnnounce(string incomingJson)
	{
		if (!IsLocalCharacterOwner())
			return false;

		string previous = GetData(DATA_MESSAGES);
		if (previous == "" || incomingJson == "")
			return false;

		return TotalUnread(incomingJson) > TotalUnread(previous);
	}

	//------------------------------------------------------------------------------------------------
	protected int TotalUnread(string json)
	{
		ELIFE_MessageUpdatesDto updates = new ELIFE_MessageUpdatesDto();
		updates.ExpandFromRAW(json);

		int total = 0;
		foreach (ELIFE_ThreadDto threadDto : updates.threads)
			total += threadDto.unreadCount;

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Called at provisioning and whenever the registered owner takes the phone in hand.
	protected void PushIdentityToOwner()
	{
		if (m_sServerNumber == "")
			return;

		Rpc(RpcDo_IdentityOwner, m_sServerNumber, m_sServerPin);

		//! Owner RPCs don't reach a listen server's own view.
		if (IsLocalCharacterOwner())
			RpcDo_IdentityOwner(m_sServerNumber, m_sServerPin);
	}

	//------------------------------------------------------------------------------------------------
	//! Number is the localised "unknown" placeholder, so it's built on each client rather than replicated.
	protected void ApplyRedactedIdentity()
	{
		if (m_bHasRealIdentity || m_sRedactedPin == "")
			return;

		ApplyIdentity(ELIFE_DataRedactor.RedactPhoneNumber(), m_sRedactedPin);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyIdentity(string number, string pin)
	{
		m_sNumber = number;
		m_sPin = pin;
		OnIdentityUpdated();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_IdentityOwner(string number, string pin)
	{
		m_bHasRealIdentity = true;
		ApplyIdentity(number, pin);
	}

	//------------------------------------------------------------------------------------------------
	override void OnToggleActive(bool state)
	{
		m_bActivated = state;

		if (!state)
		{
			SetScreenState(EPhoneScreenState.OFF);
			return;
		}

		//! Power-on from Off only — don't clobber a page that's already showing.
		if (m_eScreenState != EPhoneScreenState.OFF)
			return;

		if (m_bWasLocked)
			SetScreenState(EPhoneScreenState.LOCKED);
		else
			SetScreenState(m_eResumeState);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the authority (server) to change state - only it may set an [RplProp] for it to replicate.
	void SetScreenState(EPhoneScreenState state)
	{
		if (m_eScreenState == state)
			return;

		RememberResume(state);
		Rpc(RpcAsk_SetScreenState, state);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetScreenState(EPhoneScreenState state)
	{
		//! Only RpcAsk_Unlock leaves the lock screen, so a locked phone also powers on locked.
		if (m_bPinLocked && state != EPhoneScreenState.OFF)
			state = EPhoneScreenState.LOCKED;

		//! Without a known PIN the lock could never be undone, so a power-on lands on home instead.
		if (state == EPhoneScreenState.LOCKED && m_sServerPin == "")
		{
			if (m_eScreenState != EPhoneScreenState.OFF)
				return;

			state = EPhoneScreenState.HOME;
		}

		if (state == EPhoneScreenState.LOCKED)
			m_bPinLocked = true;

		if (m_eScreenState == state)
			return;

		m_eScreenState = state;
		ApplyScreenState();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	void Unlock(string pin)
	{
		RememberResume(EPhoneScreenState.HOME);
		Rpc(RpcAsk_Unlock, pin);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Unlock(string pin)
	{
		if (!m_bPinLocked)
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (now < m_fUnlockBlockedUntil)
		{
			SendUnlockRejected(ELIFE_EUnlockResult.BLOCKED, m_fUnlockBlockedUntil - now);
			return;
		}

		if (pin != m_sServerPin)
		{
			m_iUnlockFailures++;
			if (m_iUnlockFailures < UNLOCK_MAX_FAILURES)
			{
				SendUnlockRejected(ELIFE_EUnlockResult.WRONG, 0);
				return;
			}

			m_iUnlockFailures = 0;
			m_fUnlockBlockedUntil = now + UNLOCK_COOLDOWN_MS;
			SendUnlockRejected(ELIFE_EUnlockResult.BLOCKED, UNLOCK_COOLDOWN_MS);
			return;
		}

		m_iUnlockFailures = 0;
		m_bPinLocked = false;
		RpcAsk_SetScreenState(EPhoneScreenState.HOME);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendUnlockRejected(ELIFE_EUnlockResult result, float retryInMs)
	{
		Rpc(RpcDo_UnlockRejected, result, (int)retryInMs);

		//! Owner RPCs don't reach a listen server's own view.
		if (IsLocalCharacterOwner())
			m_OnUnlockRejected.Invoke(result, (int)retryInMs);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_UnlockRejected(int result, int retryInMs)
	{
		m_OnUnlockRejected.Invoke(result, retryInMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Fired on proxies whenever m_eScreenState updates, incl. initial sync for late joiners.
	protected void OnScreenStateUpdated()
	{
		ApplyScreenState();
	}

	//------------------------------------------------------------------------------------------------
	//! Generic counterpart to SetScreenState() for in-app navigation - see ELIFE_PhoneAppBase.
	void SetScreenSubState(string subState)
	{
		if (m_sScreenSubState == subState)
			return;

		Rpc(RpcAsk_SetScreenSubState, subState);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetScreenSubState(string subState)
	{
		if (m_sScreenSubState == subState)
			return;

		m_sScreenSubState = subState;
		ApplyScreenSubState();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnScreenSubStateUpdated()
	{
		ApplyScreenSubState();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyScreenSubState()
	{
		if (m_ScreenRenderComponent)
			m_ScreenRenderComponent.OnScreenSubStateChanged(m_sScreenSubState);
	}

	//------------------------------------------------------------------------------------------------
	string GetScreenSubState()
	{
		return m_sScreenSubState;
	}

	//------------------------------------------------------------------------------------------------
	//! Outer LOD tier gate - entering re-applies current state, leaving freezes on the Off material.
	void SetLocallySynced(bool synced)
	{
		if (m_bLocallySynced == synced)
			return;

		m_bLocallySynced = synced;

		if (m_bLocallySynced)
		{
			ApplyScreenState();
			return;
		}

		StopScreenPulse();
		SetScreenMaterial(m_sScreenOffMaterial);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyScreenState()
	{
		//! Before the LOD gate: poll cadence follows the screen being on, regardless of distance.
		ReschedulePoll();

		//! Close the hub when the screen sleeps or locks, so message bodies aren't one tap behind the lock.
		if (Replication.IsServer() && m_bHubOpen && (m_eScreenState == EPhoneScreenState.OFF || m_eScreenState == EPhoneScreenState.LOCKED))
		{
			m_bHubOpen = false;
			OnHubOpenUpdated();
			Replication.BumpMe();
		}

		if (!m_bLocallySynced)
			return;

		if (m_bLiveScreenActive)
			SetScreenMaterial(m_sScreenLiveMaterial);
		else
			SetScreenMaterial(GetBakedScreenMaterial());

		bool poweringOn = m_ePrevScreenState == EPhoneScreenState.OFF && m_eScreenState != EPhoneScreenState.OFF;
		bool poweringOff = m_ePrevScreenState != EPhoneScreenState.OFF && m_eScreenState == EPhoneScreenState.OFF;
		m_ePrevScreenState = m_eScreenState;

		//! Any state other than Off answers the question, so the flag survives the screen going dark.
		if (m_eScreenState != EPhoneScreenState.OFF)
			m_bWasLocked = m_eScreenState == EPhoneScreenState.LOCKED;

		RememberResume(m_eScreenState);

		if (m_eScreenState == EPhoneScreenState.OFF)
		{
			StopScreenPulse();
			if (poweringOff && m_SoundComponent)
				m_SoundComponent.SoundEvent(SOUND_EVENT_POWER_OFF);
		}
		else
		{
			if (poweringOn && m_SoundComponent)
				m_SoundComponent.SoundEvent(SOUND_EVENT_POWER_ON);

			StartScreenPulse();
		}

		if (m_ScreenRenderComponent)
			m_ScreenRenderComponent.OnScreenStateChanged(m_eScreenState);

		m_OnScreenStateChanged.Invoke(m_eScreenState);
	}

	//------------------------------------------------------------------------------------------------
	//! Out of live range every non-Off state shares one LOD material.
	protected ResourceName GetBakedScreenMaterial()
	{
		if (m_eScreenState == EPhoneScreenState.OFF)
			return m_sScreenOffMaterial;

		return m_sScreenLodMaterial;
	}

	//------------------------------------------------------------------------------------------------
	EPhoneScreenState GetScreenState()
	{
		return m_eScreenState;
	}

	//------------------------------------------------------------------------------------------------
	//! Known before the power-on state round-trips, unlike GetScreenState().
	bool WasLocked()
	{
		return m_bWasLocked;
	}

	//------------------------------------------------------------------------------------------------
	//! A hand-off that leaves the phone is not a resume target.
	protected void RememberResume(EPhoneScreenState state)
	{
		if (state == EPhoneScreenState.OFF || state == EPhoneScreenState.LOCKED)
			return;

		if (state == EPhoneScreenState.MAP)
			m_eResumeState = EPhoneScreenState.HOME;
		else
			m_eResumeState = state;
	}

	//------------------------------------------------------------------------------------------------
	void SetLiveScreenMaterial(bool enable)
	{
		if (m_bLiveScreenActive == enable)
			return;

		m_bLiveScreenActive = enable;

		if (m_bLiveScreenActive)
			SetScreenMaterial(m_sScreenLiveMaterial);
		else
			SetScreenMaterial(GetBakedScreenMaterial());
	}

	//------------------------------------------------------------------------------------------------
	override EGadgetType GetType()
	{
		return EGadgetType.SPECIALIST_ITEM;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeHeld()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeRaised()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void ModeSwitch(EGadgetMode mode, IEntity charOwner)
	{
		super.ModeSwitch(mode, charOwner);

		if (mode == EGadgetMode.IN_HAND)
		{
			if (Replication.IsServer() && charOwner)
			{
				RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));

				PlayerManager playerManager = GetGame().GetPlayerManager();
				int playerId = 0;
				if (playerManager)
					playerId = playerManager.GetPlayerIdFromControlledEntity(charOwner);

				PlayerController pc;
				if (playerManager && playerId != 0)
					pc = playerManager.GetPlayerController(playerId);

				if (rpl && pc)
					rpl.Give(pc.GetRplIdentity());

				if (playerId != 0)
					m_iOwnerPlayerId = playerId;

				if (m_sPhoneId == "")
					ProvisionPhone(playerId);
				else if (m_sServerOwnerCharacterId != "" && ELIFE_CharacterIdentity.GetCharacterId(playerId) == m_sServerOwnerCharacterId)
					PushIdentityToOwner();
			}

			IEntity localCharacter = SCR_PlayerController.GetLocalControlledEntity();
			if (charOwner && charOwner == localCharacter)
			{
				ELIFE_PhonePeek.TakeDoorAndHide();
				ELIFE_PhoneToggle.RememberActivePhone(this);

				if (m_ScreenInteractComponent)
					m_ScreenInteractComponent.SetActive(true);
			}
		}

		if (mode != EGadgetMode.IN_HAND)
		{
			ClosePhoneMenu();

			//! Direct call since ToggleActive requires m_CharacterOwner, which may already be cleared by this point.
			if (m_bActivated)
				OnToggleActive(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void ModeClear(EGadgetMode mode)
	{
		super.ModeClear(mode);

		if (mode == EGadgetMode.IN_HAND)
		{
			if (m_ScreenInteractComponent)
				m_ScreenInteractComponent.SetActive(false);

			if (m_ePrevScreenState != EPhoneScreenState.OFF && m_SoundComponent)
				m_SoundComponent.SoundEvent(SOUND_EVENT_POWER_OFF);

			m_eScreenState = EPhoneScreenState.OFF;
			m_ePrevScreenState = EPhoneScreenState.OFF;
			m_bLiveScreenActive = false;
			SetScreenMaterial(m_sScreenOffMaterial);
			StopScreenPulse();

			if (m_ScreenRenderComponent)
				m_ScreenRenderComponent.OnScreenStateChanged(EPhoneScreenState.OFF);

			//! Sets the state directly, bypassing ApplyScreenState(), so reschedule the poll here.
			ReschedulePoll();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void ToggleFocused(bool enable)
	{
		super.ToggleFocused(enable);

		if (!IsLocalCharacterOwner())
			return;

		if (enable)
			OpenPhoneMenu();
		else
			ClosePhoneMenu();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetScreenMaterial(ResourceName material)
	{
		if (material == ResourceName.Empty)
			return;

		//! Skip redundant remaps - re-issuing the same material tears down an already-bound $rendertarget.
		if (material == m_sAppliedScreenMaterial)
			return;

		IEntity owner = GetOwner();
		VObject obj = owner.GetVObject();
		if (!obj)
		{
			Print("ELIFE_Phone: SetScreenMaterial aborted - GetVObject() returned null", LogLevel.WARNING);
			return;
		}

		string remap = string.Format("$remap '%1' '%2'; $remap '%3' '%4';",
			BODY_SOURCE_MATERIAL, m_sBodyMaterial, SCREEN_SOURCE_MATERIAL, material);
		owner.SetObject(obj, remap);
		m_sAppliedScreenMaterial = material;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetScreenLit(bool lit)
	{
		if (!m_ScreenEmissiveMaterial)
			return;

		if (lit)
			m_ScreenEmissiveMaterial.SetEmissiveMultiplier(m_fScreenEmissiveIntensity);
		else
			m_ScreenEmissiveMaterial.SetEmissiveMultiplier(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartScreenPulse()
	{
		if (!m_ScreenEmissiveMaterial)
			return;

		m_fScreenPulsePhase = 0;
		GetGame().GetCallqueue().Remove(TickScreenPulse);
		GetGame().GetCallqueue().CallLater(TickScreenPulse, 200, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopScreenPulse()
	{
		GetGame().GetCallqueue().Remove(TickScreenPulse);
		SetScreenLit(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickScreenPulse()
	{
		if (!m_ScreenEmissiveMaterial)
			return;

		m_fScreenPulsePhase = m_fScreenPulsePhase + Math.RandomFloatInclusive(0.04, 0.09);
		float t = 0.75 + 0.25 * Math.Sin(m_fScreenPulsePhase);
		t += Math.RandomFloatInclusive(-0.01, 0.01);
		t = Math.Clamp(t, 0.95, 1);
		m_ScreenEmissiveMaterial.SetEmissiveMultiplier(m_fScreenEmissiveIntensity * t);
	}

	//------------------------------------------------------------------------------------------------
	void OpenPhoneMenu()
	{
		if (!IsLocalCharacterOwner())
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		ELIFE_PhoneToggle.RememberActivePhone(this);

		ELIFE_PhonePeek.TakeDoorAndHide();
		string doorThreadId = ELIFE_PhonePeek.ConsumeHandoff();

		//! Power-on first so resume lands, then a peek door can overwrite it.
		ToggleActive(true, SCR_EUseContext.FROM_ACTION);

		ELIFE_PhoneMenu phoneMenu = ELIFE_PhoneMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.ELIFE_PhoneMenu));
		if (!phoneMenu)
			phoneMenu = ELIFE_PhoneMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.ELIFE_PhoneMenu));

		if (phoneMenu)
			phoneMenu.BindPhone(this, doorThreadId);
	}

	//------------------------------------------------------------------------------------------------
	void ClosePhoneMenu()
	{
		if (!IsLocalCharacterOwner())
			return;

		ToggleActive(false, SCR_EUseContext.FROM_ACTION);

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		ELIFE_PhoneMenu phoneMenu = ELIFE_PhoneMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.ELIFE_PhoneMenu));
		if (!phoneMenu)
			return;

		phoneMenu.CloseWithoutHolster();
	}

	//------------------------------------------------------------------------------------------------
	void Holster()
	{
		ClosePhoneMenu();

		if (GetMode() != EGadgetMode.IN_HAND)
			return;

		ChimeraCharacter characterOwner = GetCharacterOwner();
		if (!characterOwner)
			characterOwner = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());

		if (!characterOwner)
			return;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(characterOwner);
		if (gadgetManager)
			gadgetManager.SetGadgetMode(GetOwner(), EGadgetMode.IN_STORAGE);
	}

	//------------------------------------------------------------------------------------------------
	//! Distinguishes operating this phone from looking at someone else's.
	bool IsLocalCharacterOwner()
	{
		ChimeraCharacter characterOwner = GetCharacterOwner();
		if (!characterOwner)
			return false;

		return characterOwner == SCR_PlayerController.GetLocalControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		writer.WriteBool(m_bActivated);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		reader.ReadBool(m_bActivated);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_ProvisionPhoneCallback : ELIFE_BaseRestCallback
{
	//------------------------------------------------------------------------------------------------
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		ELIFE_ProvisionPhoneResponseDto dto = new ELIFE_ProvisionPhoneResponseDto();
		dto.ExpandFromRAW(data);
		dto.StashRawJson(data);
		resultData = dto;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_ContactsFetchCallback : ELIFE_BaseRestCallback
{
	//------------------------------------------------------------------------------------------------
	//! Bridge returns a bare JSON array - wrap it so the object-rooted JsonApiStruct parser can read it.
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		ELIFE_ContactListDto dto = new ELIFE_ContactListDto();
		string wrapped = string.Format("{\"items\":%1}", data);
		dto.ExpandFromRAW(wrapped);
		dto.StashRawJson(wrapped);
		resultData = dto;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_SaveContactCallback : ELIFE_BaseRestCallback
{
	//------------------------------------------------------------------------------------------------
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		ELIFE_SaveContactResponseDto dto = new ELIFE_SaveContactResponseDto();
		dto.ExpandFromRAW(data);
		dto.StashRawJson(data);
		resultData = dto;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_SendMessageCallback : ELIFE_BaseRestCallback
{
	//------------------------------------------------------------------------------------------------
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		ELIFE_SendMessageResponseDto dto = new ELIFE_SendMessageResponseDto();
		dto.ExpandFromRAW(data);
		dto.StashRawJson(data);
		resultData = dto;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}

//------------------------------------------------------------------------------------------------
//! The read route answers 204 with no body - only the status code matters, which the base class already carries.
class ELIFE_MarkThreadReadCallback : ELIFE_BaseRestCallback
{
	//------------------------------------------------------------------------------------------------
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		resultData = null;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_MessageUpdatesFetchCallback : ELIFE_BaseRestCallback
{
	//------------------------------------------------------------------------------------------------
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		ELIFE_MessageUpdatesDto dto = new ELIFE_MessageUpdatesDto();
		dto.ExpandFromRAW(data);
		dto.StashRawJson(data);
		resultData = dto;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}
