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
//! How a send landed. UNDELIVERABLE means the API committed it (200) but named recipients who'll never get it - not the same as FAILED.
enum ELIFE_EMessageSendResult
{
	SENT,
	UNDELIVERABLE,
	TOO_LONG,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! A subscriber number, client-side - mirrors just enough of the backend PhoneNumber's rule to reject an obviously malformed entry without a round trip.
class ELIFE_PhoneNumber
{
	static const int DIGIT_COUNT = 8;

	protected static const string DIGITS = "0123456789";

	protected static const string SEPARATORS = " -()/.";

	//------------------------------------------------------------------------------------------------
	//! True when raw holds exactly DIGIT_COUNT digits, ignoring an optional leading "+" and separators - says nothing about whether it's assigned to a phone.
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
//! Handheld phone gadget. Identity is this component (SPECIALIST_ITEM), never EGadgetType.GPS.
//! Provisioned against the backend on first equip. Items are not stackable.
class ELIFE_PhoneGadgetComponent : SCR_GadgetComponent
{
	protected const string BODY_SOURCE_MATERIAL = "Phone_Body_06D0DC3A5800CC7A";
	protected const string SCREEN_SOURCE_MATERIAL = "Phone_Screen_7D200FDF0E0FC494";

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

	[Attribute("0.03 0.03 0.035 1", UIWidgets.ColorPicker, "Case color tint applied to the phone menu UI bezel (should roughly match this variant's body material).", category: "Phone")]
	protected ref Color m_CaseColor;

	[RplProp(onRplName: "OnPhoneIdUpdated")]
	protected string m_sPhoneId;

	//! Not [RplProp] - a plain RplProp has no owner-only filter, so it'd leak the real number/PIN to bystanders. Pushed via PushIdentity()/ApplyIdentity() instead.
	protected string m_sNumber;
	protected string m_sPin;

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

	//! Fires with the new EPhoneScreenState whenever the screen state actually lands on this machine - both the in-hand menu and the world screen render off it.
	ref ScriptInvoker m_OnScreenStateChanged = new ScriptInvoker();

	//! Fires once this phone's provisioned identity (id/number/PIN) lands or changes - provisioning finishes after the phone is first drawn, so UI must re-read rather than sample once.
	ref ScriptInvoker m_OnIdentityChanged = new ScriptInvoker();

	//! Fires (ELIFE_EContactSaveResult) once a SaveContact() call resolves - separate from m_OnDataChanged so the form knows whether *its* submission landed.
	ref ScriptInvoker m_OnContactSaveResult = new ScriptInvoker();

	//! Fires (ELIFE_EMessageSendResult, threadId) once a SendMessage() call resolves; threadId is empty on failure, else the (possibly new) thread the message landed in.
	ref ScriptInvoker m_OnMessageSendResult = new ScriptInvoker();

	//! True once a Bridge request comes back with no real HTTP answer (curl-level failure, HTTP=0) and no later request has since gotten one - see ELIFE_PhoneScreenShell's OfflineScreen.
	[RplProp(onRplName: "OnConnectivityUpdated")]
	protected bool m_bOffline;

	ref ScriptInvoker m_OnConnectivityChanged = new ScriptInvoker();

	//! Whether the notification hub is open over the screen. Replicated for the same reason the screen
	//! state is: the world render target has to be showing what the owner is actually looking at.
	[RplProp(onRplName: "OnHubOpenUpdated")]
	protected bool m_bHubOpen;

	ref ScriptInvoker m_OnHubOpenChanged = new ScriptInvoker();

	//! threadId -> unreadCount at dismissal. A later message pushes past that watermark and the notification returns - never replicated, since this is a per-reader gesture that must not touch read state.
	protected ref map<string, int> m_mDismissedNotifications = new map<string, int>();

	//! Held rather than local so OnContactSaved() can still read GetHttpCode() off it once the shared callback base has collapsed the failure to ERROR.
	protected ref ELIFE_SaveContactCallback m_SaveContactCallback;
	protected ref ELIFE_SendMessageCallback m_SendMessageCallback;
	protected ref ELIFE_MarkThreadReadCallback m_MarkThreadReadCallback;

	protected const int DATA_CACHE_TTL_MS = 15000;

	//! Server-only. Cursor for the messages poll - the polledAt of the last updates response. Empty
	//! means "never polled", which asks for every thread whole instead of a delta.
	protected string m_sMessagesCursor;

	//! Server-only. True while the in-flight messages fetch is the background poll, not something the owner asked for - a poll failure must not flip their app into an error state.
	protected bool m_bPollInFlight;

	protected int m_iPollIntervalMs;

	//! Fast while awake (polling is the only delivery path). Slower while holstered — still frequent enough that a peek isn't a minute late.
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
	protected SoundComponent m_SoundComponent;
	protected bool m_bLiveScreenActive;
	protected ResourceName m_sAppliedScreenMaterial;
	protected EPhoneScreenState m_ePrevScreenState = EPhoneScreenState.OFF;

	//! Whether this phone was locked when its screen last went off - lets the owner power back on into LOCKED instead of always HOME.
	protected bool m_bWasLocked;

	//! Last in-phone page. Opening again lands here instead of always Home.
	protected EPhoneScreenState m_eResumeState = EPhoneScreenState.HOME;

	//! Defined in the phone's own Phone_UI.acp (reuses vanilla UI_Task_Succeded/Canceled.wav).
	protected const string SOUND_EVENT_POWER_ON = "SOUND_PHONE_POWER_ON";
	protected const string SOUND_EVENT_POWER_OFF = "SOUND_PHONE_POWER_OFF";

	//! Played on the owner's machine when the messages poll raises their unread total - see ApplyData().
	protected const string SOUND_EVENT_NOTIFICATION = "SOUND_PHONE_NOTIFICATION";

	//! Outer LOD tier (see ELIFE_PhoneScreenRenderComponent.SYNC_RANGE_METERS); starts true so a
	//! client spawning already close doesn't wait a tick for the initial state.
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
		m_SoundComponent = SoundComponent.Cast(owner.FindComponent(SoundComponent));
	}

	//------------------------------------------------------------------------------------------------
	string GetPhoneId()
	{
		return m_sPhoneId;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires on the owning client whenever m_sPhoneId actually changes - including once async
	//! provisioning finishes, since capturing it at equip time (ModeSwitch) would always be empty.
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
	//! Owner-driven, authority-applied - same shape as SetScreenState(), so the hub opens on the world
	//! screen at the same moment it opens in the menu.
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
	//! httpCode 0 means the request never got a real answer at all; any real HTTP code (even an error one) proves the Bridge is up, so that's not "offline".
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
	//! Dev-only - only the owner ever needs the PIN again (the guard chain accepts "owner OR PIN"),
	//! so it isn't reused after this - just shown in Settings.
	protected void ProvisionPhone(int playerId)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
			return;

		string characterId = ELIFE_CharacterIdentity.GetCharacterId(playerId);
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
		Replication.BumpMe();

		//! onRplName only reliably fires on remote proxies, not the machine setting the value (see
		//! RpcAsk_SetScreenState() below, which applies locally the same way) - on a listen server the
		//! equipping player IS that machine, so OnPhoneIdUpdated() alone would never fire for them.
		OnPhoneIdUpdated();

		PushIdentity(response.number, pin);

		//! Contacts/Messages guard chain requires the phone powered on - freshly provisioned phones
		//! start off, so nothing in either app works until this runs once.
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
	// Messages poll (Step 8) - the mod's delivery path, since the SignalR hub isn't reachable here
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
		//! A fetch is already on its way; marking this one as the poll would mislabel that one's
		//! failure as a background failure and swallow the owner's error state.
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
	//! Called by an app on open. Only the owner drives fetches - a bystander's screen just mirrors
	//! whatever the owner's next refresh broadcasts.
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

		//! An owner request adopts an in-flight poll as its own, so that fetch's failure is reported
		//! to them rather than stayed quiet about.
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
	//! Owner-only manual nudge from the global Offline screen - re-provisions if that never landed, else re-fetches whatever data errored.
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

	//! Owner-only. number/displayName go to the Bridge exactly as typed - the backend is the only authority on what a number is.
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

	//! Owner-only. The API's send route is a fan-out over numbers, but the phone UI only ever addresses one conversation, so this wraps a single recipient.
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
	//! One failure a player can act on (body too long) and one "try again" for everything else - same reasoning as ClassifySaveFailure().
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

		//! The unread badge comes from the messages payload, so it needs a re-fetch to clear - and a
		//! full one, since a cursor poll only reports arrivals and would never mention this thread again.
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
	//! Only place that knows a key's wire shape - route and response parsing. Everything downstream (caching, pushing, rendering) is key-agnostic. full forces a cursor-less messages fetch, for a change the poll wouldn't observe (e.g. marking a thread read moves an unread count the "arrived since" cursor would never mention again).
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
			//! Cursor-less, threads come back whole with bodies - the plain threads route omits them
			//! and would need a second call per opened thread. With a cursor the response is only
			//! what arrived since it, which OnDataFetched() merges into the cache.
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
			//! A background poll nobody asked for only warns - NoteConnectivity() above already drove
			//! the phone-wide Offline screen, and an error state here would blank an app the owner
			//! never asked to refresh.
			if (wasPoll)
			{
				Print("ELIFE_PhoneGadgetComponent | messages poll failed", LogLevel.WARNING);
				return;
			}

			Print(string.Format("ELIFE_PhoneGadgetComponent | fetch failed for '%1'", key), LogLevel.ERROR);
			PushDataError(key);
			return;
		}

		//! Re-Pack()ing data would trip the engine's "Operation called twice" warning, so the raw text it was unpacked from is read back instead - see ELIFE_PhoneJsonDto.
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

		//! A quiet poll has nothing to tell anyone. An owner-requested fetch always answers, though -
		//! their app is sitting on LOADING until something lands.
		if (changed || !wasPoll)
			PushData(key);

		//! A messages fetch that landed first used redacted numbers as titles - rebuild it now that names exist.
		if (key == DATA_CONTACTS)
			RefreshBystanderMessages();
	}

	//------------------------------------------------------------------------------------------------
	//! Folds an updates response into the cached payload, advances the cursor, and returns the merged JSON. A cursor-less response carries every thread whole so merging it is equivalent to replacing it.
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
	//! A DateTimeOffset cursor can serialize with a "+00:00" offset, and a bare "+" in a query string
	//! decodes as a space on the other side.
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
	//! Takes the cached JSON rather than the response struct: for messages those differ, since a poll
	//! response is only a delta and the bystander copy has to mirror the whole merged payload.
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
	//! Number -> saved name from the owner's real contacts cache - RedactPayload runs server-side, where only m_mServerData still has it.
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

		//! Neither RPC self-delivers to a listen server's own view (same quirk as onRplName, see
		//! OnPhoneIdUpdated()), so apply it directly here. A dedicated server has no view to render into.
		if (!SCR_PlayerController.GetLocalControlledEntity())
			return;

		if (IsLocalCharacterOwner())
			ApplyData(key, entry.m_sReal);
		else
			ApplyData(key, entry.m_sRedacted);
	}

	//------------------------------------------------------------------------------------------------
	//! Only the owner is told a fetch failed - a bystander never asked, so to them it just looks like
	//! the mirror stopped updating.
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
	//! No RplCondition filter here - its per-recipient behavior for RPCs (vs. RplProp, where it's
	//! documented) is unverified, and an entity with an owner may not broadcast at all under it.
	//! Excluding the owner is done explicitly below instead, which is correct regardless.
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

		ELIFE_PhonePeek.Show(this);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the owner's unread total goes up on this machine - counting unread rather than messages means a re-delivered message can't double-announce. The first payload never announces.
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
	//! Same owner-real/bystander-redacted split as PushData(), but for number/PIN this only runs once at provisioning - no cached copy to re-push to a late bystander. The number is masked with the
	//! fixed "unknown number" placeholder like Contacts/Messages; the PIN keeps randomized digits since nothing displays it as a name-like value.
	protected void PushIdentity(string number, string pin)
	{
		string redactedNumber = ELIFE_DataRedactor.RedactPhoneNumber();
		string redactedPin = ELIFE_DataRedactor.RedactDigits(pin);

		Rpc(RpcDo_IdentityOwner, number, pin);
		Rpc(RpcDo_IdentityBystanders, redactedNumber, redactedPin);

		//! Neither RPC self-delivers to a listen server's own view, so apply it directly here.
		if (!SCR_PlayerController.GetLocalControlledEntity())
			return;

		if (IsLocalCharacterOwner())
			ApplyIdentity(number, pin);
		else
			ApplyIdentity(redactedNumber, redactedPin);
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
		ApplyIdentity(number, pin);
	}

	//------------------------------------------------------------------------------------------------
	//! No RplCondition filter - see RpcDo_DataBystanders(); the owner is excluded explicitly below instead.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IdentityBystanders(string number, string pin)
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl && rpl.IsOwner())
			return;

		ApplyIdentity(number, pin);
	}

	//------------------------------------------------------------------------------------------------
	Color GetCaseColor()
	{
		if (!m_CaseColor)
			m_CaseColor = new Color(0.03, 0.03, 0.035, 1);

		return m_CaseColor;
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
		if (m_eScreenState == state)
			return;

		m_eScreenState = state;
		ApplyScreenState();
		Replication.BumpMe();
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
		//! Ahead of the LOD gate below: the poll cadence follows the screen being on, which has
		//! nothing to do with whether this machine is close enough to draw it. No-ops off the
		//! authority and whenever the tier hasn't actually changed.
		ReschedulePoll();

		//! The hub is a layer over the screen, so it cannot outlive the screen being awake - and a
		//! locked phone must not be holding message bodies one tap behind the lock.
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
	//! Out of live-render range, every non-Off state shows the same generic LOD material rather than
	//! per-app content - only Off gets its own distinct material.
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
	//! Lets the menu draw the right screen on open instead of sitting on black until state round-trips to the authority.
	bool WasLocked()
	{
		return m_bWasLocked;
	}

	//------------------------------------------------------------------------------------------------
	EPhoneScreenState GetResumeState()
	{
		return m_eResumeState;
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
			}

			IEntity localCharacter = SCR_PlayerController.GetLocalControlledEntity();
			if (charOwner && charOwner == localCharacter)
			{
				ELIFE_PhonePeek.TakeDoorAndHide();
				ELIFE_PhoneToggle.RememberActivePhone(this);
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
			if (m_ePrevScreenState != EPhoneScreenState.OFF && m_SoundComponent)
				m_SoundComponent.SoundEvent(SOUND_EVENT_POWER_OFF);

			m_eScreenState = EPhoneScreenState.OFF;
			m_ePrevScreenState = EPhoneScreenState.OFF;
			m_bLiveScreenActive = false;
			SetScreenMaterial(m_sScreenOffMaterial);
			StopScreenPulse();

			if (m_ScreenRenderComponent)
				m_ScreenRenderComponent.OnScreenStateChanged(EPhoneScreenState.OFF);

			//! This path sets the state field directly rather than going through ApplyScreenState(),
			//! so the poll would otherwise stay on the active cadence for a holstered phone.
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
	//! Public so a screen can tell "operating this phone" from "looking at someone else's" - the latter must not resolve numbers against a redacted contact list.
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
