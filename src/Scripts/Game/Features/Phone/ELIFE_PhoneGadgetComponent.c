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
	CONTACTS,
	STORE,
	SETUP
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
//! Why a contact save or rename didn't land, in the cases the player can act on - see ClassifySaveFailure().
enum ELIFE_EContactSaveResult
{
	SAVED,
	INVALID_NUMBER,
	DUPLICATE,
	FAILED,
	INVALID_NAME,
	NOT_FOUND
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
//! INVALID: not PIN_LENGTH digits - client-side, or the backend's 400. Also answers CompleteSetup().
enum ELIFE_EPinChangeResult
{
	CHANGED,
	INVALID,
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

	//! Who the phone is registered to. Public, unlike the number: it's the owner's RP name.
	[RplProp(onRplName: "OnIdentityUpdated")]
	protected string m_sOwnerName;

	//! Server memory only: a restart forgets it.
	[RplProp()]
	protected bool m_bSetupCompleted;

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

	//! Server-only. The PIN and owner name of the in-flight provisioning request, kept until OnPhoneProvisioned().
	protected string m_sPendingPin;
	protected string m_sPendingOwnerName;

	//! Server-only. Set once the backend confirms power-on; a phone that's off answers every app route with 409.
	protected bool m_bBackendPoweredOn;

	//! Held so handlers can read the HTTP code - the base callback collapses every failure to ERROR.
	protected ref ELIFE_ProvisionedPhoneCallback m_ProvisionCallback;
	protected ref ELIFE_PhonePowerStateCallback m_PowerOnCallback;
	protected ref ELIFE_SavedContactCallback m_SaveContactCallback;
	protected ref ELIFE_BaseRestCallback m_RenameContactCallback;
	protected string m_sPendingRenameContactId;
	protected string m_sPendingRenameName;
	protected ref ELIFE_SentMessageCallback m_SendMessageCallback;
	protected ref ELIFE_BaseRestCallback m_MarkThreadReadCallback;
	protected ref ELIFE_BaseRestCallback m_AckNotificationsCallback;
	protected ref array<string> m_aAckingNotificationIds = {};

	//! Server-only. One conversation fetch at a time; the generated callback only echoes the phoneId.
	protected ref ELIFE_MessageThreadDetailCallback m_ThreadFetchCallback;
	protected string m_sFetchingThreadId;
	protected string m_sQueuedThreadFetch;

	//! Server-only. The conversation whose full history the messages payload holds.
	protected string m_sFullThreadId;
	protected ref ELIFE_BaseRestCallback m_ChangePinCallback;
	protected ref ELIFE_BaseRestCallback m_DeleteContactCallback;
	protected ref ELIFE_BaseRestCallback m_BlockCallback;
	protected ref ELIFE_BaseRestCallback m_AppInstallCallback;
	protected int m_iOwnerPlayerId;

	//! Server-only. What the in-flight PIN change / block toggle asked for; the generated handlers only hand back the phoneId.
	protected bool m_bPinChangePending;
	protected string m_sPendingNewPin;
	//! The in-flight PIN change is setup's last step, so landing it finishes setup.
	protected bool m_bPendingSetup;
	protected string m_sPendingBlockNumber;
	protected bool m_bPendingBlock;
	protected string m_sPendingAppKey;
	protected bool m_bPendingInstall;
	//! Generated mark-read callback only echoes phoneId — serialize so we know which thread to clear.
	protected string m_sPendingReadThreadId;
	protected ref array<string> m_aMarkReadQueue = {};
	protected int m_iSystemInstallsPending;
	protected bool m_bSystemInstallLanded;
	protected int m_iDroppedUninstallsPending;
	protected bool m_bDroppedUninstallLanded;

	//! The lock screen's keypad submits at exactly this many digits, so a PIN of any other length could never be entered.
	static const int PIN_LENGTH = 4;

	static const string DATA_CONTACTS = "contacts";
	//! The inbox: the thread list, plus each thread's newest message and the open one's full history.
	static const string DATA_MESSAGES = "messages";

	//! The phone's notification queue - the only thing polled.
	static const string DATA_NOTIFICATIONS = "notifications";
	static const string DATA_BLOCKS = "blocks";
	static const string DATA_BANK_ACCOUNTS = "bank-accounts";

	//! Which Bridge apps are installed. Built-in apps aren't listed - see ELIFE_PhoneAppCatalog.
	static const string DATA_APPS = "apps";

	//! What the backend's store offers to install. It decides which downloadable apps are visible at all.
	static const string DATA_STORE_APPS = "store-apps";

	//! Followed by the bankAccountId - one statement per key.
	static const string DATA_BANK_STATEMENT_PREFIX = "bank-statement:";

	//! Server-only: the real payload per key plus its pre-built bystander copy.
	protected ref map<string, ref ELIFE_PhoneDataEntry> m_mServerData = new map<string, ref ELIFE_PhoneDataEntry>();
	protected ref map<string, bool> m_mPendingFetches = new map<string, bool>();

	//! Server-only. In-flight fetches started before a write they can't reflect - see FetchData()'s afterWrite.
	protected ref map<string, bool> m_mStaleFetches = new map<string, bool>();

	//! Server-only. The in-flight fetch per key, held for its HTTP code and, on list routes, its raw body.
	protected ref map<string, ref RestCallback> m_mFetchCallbacks = new map<string, ref RestCallback>();

	//! Server-only. bankAccountId -> packed account, waiting for its transactions to land.
	protected ref map<string, string> m_mStatementAccounts = new map<string, string>();

	//! What this machine got pushed - real payload for the owner, redacted copy for everyone else.
	protected ref map<string, string> m_mData = new map<string, string>();
	protected ref map<string, ELIFE_EPhoneDataStatus> m_mDataStatus = new map<string, ELIFE_EPhoneDataStatus>();

	//! Owner-side only; bystanders get redacted filler text.
	protected ref ELIFE_PhoneMessageFilter m_MessageFilter;

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

	//! Fires with ELIFE_EPinChangeResult when a ChangePin() or CompleteSetup() call resolves. The new PIN itself arrives as an identity change.
	ref ScriptInvoker m_OnPinChangeResult = new ScriptInvoker();

	//! Fires with (bool deleted) when a DeleteContact() call resolves.
	ref ScriptInvoker m_OnContactDeleteResult = new ScriptInvoker();

	//! Fires with (number, blocked, bool succeeded) when a SetNumberBlocked() call resolves.
	ref ScriptInvoker m_OnBlockResult = new ScriptInvoker();

	//! Fires with (appKey, installed, bool succeeded) when a SetAppInstalled() call resolves.
	ref ScriptInvoker m_OnAppInstallResult = new ScriptInvoker();

	//! True after a Bridge request got no HTTP answer at all, until one does.
	[RplProp(onRplName: "OnConnectivityUpdated")]
	protected bool m_bOffline;

	//! Set by the notification poll: that route takes no PIN, so a 403 there only means the phone is suspended.
	[RplProp(onRplName: "OnConnectivityUpdated")]
	protected bool m_bSuspended;

	ref ScriptInvoker m_OnConnectivityChanged = new ScriptInvoker();

	//! Replicated so bystanders see the hub too.
	[RplProp(onRplName: "OnHubOpenUpdated")]
	protected bool m_bHubOpen;

	ref ScriptInvoker m_OnHubOpenChanged = new ScriptInvoker();

	//! This phone's home grid (see ELIFE_PhoneHomeLayout), replicated so everyone sees the same screen.
	//! Stored on the Bridge with the wallpaper and Do Not Disturb - see SavePreferences().
	[RplProp(onRplName: "OnHomeLayoutUpdated")]
	protected string m_sHomeLayout;

	//! Owner-only. A layout sent but not yet echoed back, shown meanwhile so an edit doesn't flick back.
	protected string m_sPendingHomeLayout;

	ref ScriptInvoker m_OnHomeLayoutChanged = new ScriptInvoker();

	//! Generous for every app and card the catalog could hold.
	protected const int HOME_LAYOUT_MAX_LENGTH = 1024;

	//! Per-phone look and behaviour, replicated and stored like the home layout.
	[RplProp(onRplName: "OnPersonalizationUpdated")]
	protected string m_sWallpaper;

	[RplProp(onRplName: "OnPersonalizationUpdated")]
	protected bool m_bDoNotDisturb;

	//! Comma-separated contact ids, most recently starred last. Server memory only: a restart forgets them.
	[RplProp(onRplName: "OnPersonalizationUpdated")]
	protected string m_sFavourites;

	//! Wallpaper, Do Not Disturb or favourites changed.
	ref ScriptInvoker m_OnPersonalizationChanged = new ScriptInvoker();

	//! Server-only. A burst of edits is saved as one write, this long after the last of them.
	protected const int PREFERENCES_SAVE_DELAY_MS = 2000;

	//! Server-only. A save the Bridge never answered is retried after this.
	protected const int PREFERENCES_RETRY_MS = 30000;

	//! Server-only. The Bridge's copy has been read (or it had none) and may now be overwritten.
	protected bool m_bPreferencesLoaded;
	protected bool m_bPreferencesDirty;
	protected ref ELIFE_PhonePreferencesCallback m_PreferencesLoadCallback;
	protected ref ELIFE_PhonePreferencesCallback m_PreferencesSaveCallback;

	//! Server-only. Edited before the Bridge's copy arrived, so that copy must not overwrite them.
	protected bool m_bLayoutEditedEarly;
	protected bool m_bWallpaperEditedEarly;
	protected bool m_bDndEditedEarly;

	//! Owner-only. A notification payload that arrived while the messages it points at were still
	//! being filtered; applied right after them, so a banner never shows an older or unfiltered body.
	protected string m_sQueuedNotificationsJson;
	protected bool m_bNotificationsQueued;

	protected const int DATA_CACHE_TTL_MS = 15000;


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

		//! Last chance for a pending save; no answer can reach this phone any more, so none is awaited.
		GetGame().GetCallqueue().Remove(SavePreferences);
		if (Replication.IsServer() && m_bPreferencesDirty && m_bPreferencesLoaded)
			SendPreferences(null, "");

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

		//! A phone without a PIN has no redacted PIN to replicate, so the id is what says the identity exists.
		ApplyRedactedIdentity();
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
	bool IsSuspended()
	{
		return m_bSuspended;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetSuspended(bool suspended)
	{
		if (m_bSuspended == suspended)
			return;

		m_bSuspended = suspended;
		Replication.BumpMe();

		//! onRplName does not fire on the authority itself.
		OnConnectivityUpdated();
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
	// Home layout
	//------------------------------------------------------------------------------------------------

	string GetHomeLayout()
	{
		if (m_sPendingHomeLayout != "")
			return m_sPendingHomeLayout;

		return m_sHomeLayout;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only.
	void SetHomeLayout(string layout)
	{
		//! The server drops oversized layouts, which would leave this pending forever.
		if (!IsLocalCharacterOwner() || layout == GetHomeLayout() || layout.Length() > HOME_LAYOUT_MAX_LENGTH)
			return;

		m_sPendingHomeLayout = layout;
		Rpc(RpcAsk_SetHomeLayout, layout);
	}

	//------------------------------------------------------------------------------------------------
	//! Only bounded here: readers drop anything they can't place.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetHomeLayout(string layout)
	{
		if (layout == m_sHomeLayout || layout.Length() > HOME_LAYOUT_MAX_LENGTH)
			return;

		m_sHomeLayout = layout;

		//! onRplName doesn't fire on the authority itself - see OnPhoneIdUpdated().
		OnHomeLayoutUpdated();
		Replication.BumpMe();

		if (!m_bPreferencesLoaded)
			m_bLayoutEditedEarly = true;

		MarkPreferencesChanged();
	}

	//------------------------------------------------------------------------------------------------
	// Personalization
	//------------------------------------------------------------------------------------------------

	string GetWallpaper()
	{
		return m_sWallpaper;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDoNotDisturb()
	{
		return m_bDoNotDisturb;
	}

	//------------------------------------------------------------------------------------------------
	void GetFavourites(notnull array<string> outContactIds)
	{
		outContactIds.Clear();
		m_sFavourites.Split(",", outContactIds, true);
	}

	//------------------------------------------------------------------------------------------------
	bool IsFavourite(string contactId)
	{
		array<string> favourites = {};
		GetFavourites(favourites);
		return contactId != "" && favourites.Find(contactId) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only.
	void SetWallpaper(string wallpaperId)
	{
		if (IsLocalCharacterOwner() && wallpaperId != m_sWallpaper)
			Rpc(RpcAsk_SetWallpaper, wallpaperId);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only.
	void SetDoNotDisturb(bool enabled)
	{
		if (IsLocalCharacterOwner() && enabled != m_bDoNotDisturb)
			Rpc(RpcAsk_SetDoNotDisturb, enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only.
	void SetFavourite(string contactId, bool favourite)
	{
		if (IsLocalCharacterOwner() && contactId != "" && favourite != IsFavourite(contactId))
			Rpc(RpcAsk_SetFavourite, contactId, favourite);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetWallpaper(string wallpaperId)
	{
		if (!ELIFE_PhoneWallpapers.Find(wallpaperId) || wallpaperId == m_sWallpaper)
			return;

		m_sWallpaper = wallpaperId;
		OnPersonalizationUpdated();
		Replication.BumpMe();

		if (!m_bPreferencesLoaded)
			m_bWallpaperEditedEarly = true;

		MarkPreferencesChanged();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetDoNotDisturb(bool enabled)
	{
		if (enabled == m_bDoNotDisturb)
			return;

		m_bDoNotDisturb = enabled;
		OnPersonalizationUpdated();
		Replication.BumpMe();

		if (!m_bPreferencesLoaded)
			m_bDndEditedEarly = true;

		MarkPreferencesChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Ids only, never numbers - the list replicates to bystanders.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetFavourite(string contactId, bool favourite)
	{
		if (contactId == "" || contactId.Contains(","))
			return;

		array<string> favourites = {};
		GetFavourites(favourites);

		int index = favourites.Find(contactId);
		if (favourite == (index != -1))
			return;

		if (favourite)
			favourites.Insert(contactId);
		else
			favourites.RemoveOrdered(index);

		string joined = "";
		foreach (string id : favourites)
		{
			if (joined != "")
				joined += ",";

			joined += id;
		}

		m_sFavourites = joined;
		OnPersonalizationUpdated();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	// Preferences on the Bridge - home layout, wallpaper, Do Not Disturb
	//------------------------------------------------------------------------------------------------

	//! Server-only. Read once per phone per session, after the first power-on, so a phone the server
	//! rebuilt comes back as it was left.
	protected void LoadPreferences()
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_bPreferencesLoaded || m_PreferencesLoadCallback || m_sPhoneId == "" || !api)
			return;

		m_PreferencesLoadCallback = api.GetPhonePreferences(m_sPhoneId, this, "OnPreferencesLoaded");
	}

	//------------------------------------------------------------------------------------------------
	void OnPreferencesLoaded(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_PreferencesLoadCallback);
		m_PreferencesLoadCallback = null;
		NoteConnectivity(status, httpCode);

		ELIFE_PhonePreferences preferences = ELIFE_PhonePreferences.Cast(data);
		if (status == ELIFE_EApiStatusCode.SUCCESS && preferences)
		{
			ApplyPreferences(preferences);
		}
		else
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | preferences not loaded, HTTP=%1", httpCode), LogLevel.WARNING);

			//! The stored copy may still exist, so edits wait and the next edit or Retry reads again.
			if (httpCode == 0 || httpCode >= 500)
				return;
		}

		//! Anything else means there is nothing to read, so saving may start.
		m_bPreferencesLoaded = true;
		m_bLayoutEditedEarly = false;
		m_bWallpaperEditedEarly = false;
		m_bDndEditedEarly = false;

		if (m_bPreferencesDirty)
			ScheduleSavePreferences(PREFERENCES_SAVE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes what the Bridge stored for every field not edited since; empty values are the defaults.
	protected void ApplyPreferences(notnull ELIFE_PhonePreferences preferences)
	{
		bool layoutChanged = false;
		bool personalizationChanged = false;

		//! A newer format than this build reads would be misplaced, so the current layout stays.
		if (preferences.layoutVersion > ELIFE_PhoneHomeLayout.FORMAT_VERSION)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | stored home layout is format %1, this build reads %2", preferences.layoutVersion, ELIFE_PhoneHomeLayout.FORMAT_VERSION), LogLevel.WARNING);
		}
		else if (!m_bLayoutEditedEarly && preferences.homeLayout != m_sHomeLayout && preferences.homeLayout.Length() <= HOME_LAYOUT_MAX_LENGTH)
		{
			m_sHomeLayout = preferences.homeLayout;
			layoutChanged = true;
		}

		//! An id this build doesn't know falls back to the default, like an unset one.
		string wallpaper = preferences.wallpaper;
		if (wallpaper != "" && !ELIFE_PhoneWallpapers.Find(wallpaper))
			wallpaper = "";

		if (!m_bWallpaperEditedEarly && wallpaper != m_sWallpaper)
		{
			m_sWallpaper = wallpaper;
			personalizationChanged = true;
		}

		if (!m_bDndEditedEarly && preferences.doNotDisturb != m_bDoNotDisturb)
		{
			m_bDoNotDisturb = preferences.doNotDisturb;
			personalizationChanged = true;
		}

		if (!layoutChanged && !personalizationChanged)
			return;

		//! onRplName doesn't fire on the authority itself - see OnPhoneIdUpdated().
		if (layoutChanged)
			OnHomeLayoutUpdated();

		if (personalizationChanged)
			OnPersonalizationUpdated();

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Saving waits until the Bridge's copy has been read, so it can't overwrite it with defaults.
	protected void MarkPreferencesChanged()
	{
		m_bPreferencesDirty = true;

		if (m_bPreferencesLoaded)
			ScheduleSavePreferences(PREFERENCES_SAVE_DELAY_MS);
		else if (m_bBackendPoweredOn)
			LoadPreferences();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleSavePreferences(int delayMs)
	{
		GetGame().GetCallqueue().Remove(SavePreferences);
		GetGame().GetCallqueue().CallLater(SavePreferences, delayMs, false);
	}

	//------------------------------------------------------------------------------------------------
	//! One write at a time; an edit made while it is in flight is saved once it answers.
	protected void SavePreferences()
	{
		if (!m_bPreferencesDirty || !m_bPreferencesLoaded || m_PreferencesSaveCallback || m_sPhoneId == "")
			return;

		m_PreferencesSaveCallback = SendPreferences(this, "OnPreferencesSaved");
	}

	//------------------------------------------------------------------------------------------------
	//! Every field, every time: the generated request can't leave one out, and this server holds them all.
	//! Sent as the registered owner, so whoever is holding the phone can rearrange it.
	protected ELIFE_PhonePreferencesCallback SendPreferences(Managed instance, string functionName)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api || m_sPhoneId == "")
			return null;

		ELIFE_UpdatePhonePreferencesRequest request = new ELIFE_UpdatePhonePreferencesRequest();
		request.characterId = m_sServerOwnerCharacterId;
		request.pin = m_sServerPin;
		request.layoutVersion = ELIFE_PhoneHomeLayout.FORMAT_VERSION;
		request.homeLayout = m_sHomeLayout;
		request.wallpaper = m_sWallpaper;
		request.doNotDisturb = m_bDoNotDisturb;

		m_bPreferencesDirty = false;
		return api.UpdatePhonePreferences(request, m_sPhoneId, instance, functionName);
	}

	//------------------------------------------------------------------------------------------------
	//! The answer is the stored copy of what was sent, so there is nothing to apply.
	void OnPreferencesSaved(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_PreferencesSaveCallback);
		m_PreferencesSaveCallback = null;
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | preferences not saved, HTTP=%1", httpCode), LogLevel.WARNING);

			//! Unreachable or failing comes back later; a rejection would only be rejected again.
			if (httpCode == 0 || httpCode >= 500)
			{
				m_bPreferencesDirty = true;
				ScheduleSavePreferences(PREFERENCES_RETRY_MS);
			}

			return;
		}

		if (m_bPreferencesDirty)
			ScheduleSavePreferences(PREFERENCES_SAVE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPersonalizationUpdated()
	{
		m_OnPersonalizationChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHomeLayoutUpdated()
	{
		//! An older echo leaves a newer pending edit in place.
		if (m_sHomeLayout == m_sPendingHomeLayout)
			m_sPendingHomeLayout = "";

		m_OnHomeLayoutChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only. Clears every Messages notification from the queue, for good; reading stays untouched.
	void ClearNotifications()
	{
		if (IsLocalCharacterOwner())
			Rpc(RpcAsk_ClearNotifications);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ClearNotifications()
	{
		ELIFE_PhoneDataEntry entry;
		if (!m_mServerData.Find(DATA_NOTIFICATIONS, entry) || entry.m_sReal == "")
			return;

		ELIFE_PhoneNotificationListDto cached = new ELIFE_PhoneNotificationListDto();
		cached.ExpandFromRAW(entry.m_sReal);

		array<string> ids = {};
		foreach (ELIFE_PhoneNotification notification : cached.items)
		{
			if (notification && notification.IsMessage())
				ids.Insert(notification.notificationId);
		}

		if (!ids.IsEmpty())
			AckNotifications(ids);
	}

	//------------------------------------------------------------------------------------------------
	//! One ack at a time; a second Clear while it's out has nothing new to say.
	protected void AckNotifications(notnull array<string> ids)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api || m_sPhoneId == "" || ids.IsEmpty() || !m_aAckingNotificationIds.IsEmpty())
			return;

		ELIFE_AckPhoneNotificationsRequest request = new ELIFE_AckPhoneNotificationsRequest();
		foreach (string id : ids)
		{
			request.ids.Insert(id);
			m_aAckingNotificationIds.Insert(id);
		}

		m_AckNotificationsCallback = api.AckPhoneNotifications(request, m_sPhoneId, this, "OnNotificationsAcked");
	}

	//------------------------------------------------------------------------------------------------
	//! Removed from the cache only once the backend has them gone, and a poll still in flight is dropped,
	//! so a cleared notification can't come back and announce itself again.
	void OnNotificationsAcked(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_AckNotificationsCallback);
		NoteConnectivity(status, httpCode);

		array<string> acked = {};
		acked.Copy(m_aAckingNotificationIds);
		m_aAckingNotificationIds.Clear();

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | notifications ack failed, HTTP=%1", httpCode), LogLevel.WARNING);
			return;
		}

		ELIFE_PhoneDataEntry entry;
		if (m_mServerData.Find(DATA_NOTIFICATIONS, entry) && entry.m_sReal != "")
		{
			ELIFE_PhoneNotificationListDto cached = new ELIFE_PhoneNotificationListDto();
			cached.ExpandFromRAW(entry.m_sReal);
			StoreData(DATA_NOTIFICATIONS, cached.Without(acked).GetRawJson());
			PushData(DATA_NOTIFICATIONS);
		}

		FetchData(DATA_NOTIFICATIONS, true);
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
	//! 0 when there's no callback to ask, which NoteConnectivity() reads as no answer.
	protected int HttpCodeOf(RestCallback callback)
	{
		if (!callback)
			return 0;

		return callback.GetHttpCode();
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
	//! Who the phone is registered to, the same on every machine.
	string GetOwnerName()
	{
		return m_sOwnerName;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSetupCompleted()
	{
		return m_bSetupCompleted;
	}

	//------------------------------------------------------------------------------------------------
	//! True on the registered owner's machine only - everyone else holds the redacted stand-ins.
	bool HasRealIdentity()
	{
		return m_bHasRealIdentity;
	}

	//------------------------------------------------------------------------------------------------
	//! Digits only, exactly PIN_LENGTH of them. The backend allows longer, the lock screen doesn't.
	static bool IsValidPin(string pin)
	{
		if (pin.Length() != PIN_LENGTH)
			return false;

		for (int i = 0; i < PIN_LENGTH; i++)
		{
			if ("0123456789".IndexOf(pin.Substring(i, 1)) == -1)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// PIN change and first-launch setup
	//------------------------------------------------------------------------------------------------

	//! Owner-only. The server supplies the current PIN, so the player only types the new one. Empty removes it.
	void ChangePin(string newPin)
	{
		if (!IsLocalCharacterOwner())
			return;

		Rpc(RpcAsk_ChangePin, newPin);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ChangePin(string newPin)
	{
		RequestPinChange(newPin, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only. Setup's last step: sets the chosen PIN, or none when empty, and leaves setup once the Bridge agrees.
	void CompleteSetup(string pin)
	{
		if (!IsLocalCharacterOwner())
			return;

		Rpc(RpcAsk_CompleteSetup, pin);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CompleteSetup(string pin)
	{
		if (m_bSetupCompleted)
			return;

		//! Always sent, even for "no PIN", since the call is what marks the phone set up on the Bridge.
		RequestPinChange(pin, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Answers through NotifyPinChangeResult() either way.
	protected void RequestPinChange(string newPin, bool completesSetup)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || !api || m_bPinChangePending)
		{
			NotifyPinChangeResult(ELIFE_EPinChangeResult.FAILED);
			return;
		}

		//! Holding an unlocked phone isn't owning it - only the registered owner may lock everyone else out.
		if (ELIFE_CharacterIdentity.GetCharacterId(m_iOwnerPlayerId) != m_sServerOwnerCharacterId)
		{
			NotifyPinChangeResult(ELIFE_EPinChangeResult.FAILED);
			return;
		}

		if (newPin != "" && !IsValidPin(newPin))
		{
			NotifyPinChangeResult(ELIFE_EPinChangeResult.INVALID);
			return;
		}

		//! The owner needs no current PIN, so this also works on a phone that has none.
		ELIFE_ChangePhonePinRequest request = new ELIFE_ChangePhonePinRequest();
		request.characterId = m_sServerOwnerCharacterId;
		request.pin = m_sServerPin;
		request.newPin = newPin;

		m_bPinChangePending = true;
		m_sPendingNewPin = newPin;
		m_bPendingSetup = completesSetup;
		m_ChangePinCallback = api.ChangePhonePin(request, m_sPhoneId, this, "OnPinChanged");
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the new PIN the same way provisioning does: server copy, redacted RplProp, owner push.
	void OnPinChanged(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_ChangePinCallback);
		NoteConnectivity(status, httpCode);

		string newPin = m_sPendingNewPin;
		bool completesSetup = m_bPendingSetup;
		m_bPinChangePending = false;
		m_sPendingNewPin = "";
		m_bPendingSetup = false;

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | PIN change failed, HTTP=%1", httpCode), LogLevel.ERROR);

			if (httpCode == 400)
				NotifyPinChangeResult(ELIFE_EPinChangeResult.INVALID);
			else
				NotifyPinChangeResult(ELIFE_EPinChangeResult.FAILED);

			return;
		}

		m_sServerPin = newPin;
		m_sRedactedPin = ELIFE_DataRedactor.RedactDigits(newPin);

		if (completesSetup)
			m_bSetupCompleted = true;

		Replication.BumpMe();

		PushIdentityToOwner();
		if (SCR_PlayerController.GetLocalControlledEntity())
			ApplyRedactedIdentity();

		NotifyPinChangeResult(ELIFE_EPinChangeResult.CHANGED);

		//! Clear the setup step so it doesn't leak into the next app opened.
		if (completesSetup && m_eScreenState == EPhoneScreenState.SETUP)
		{
			RpcAsk_SetScreenSubState("");
			RpcAsk_SetScreenState(EPhoneScreenState.HOME);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Self-applies locally for the owner too - RplRcver.Owner RPCs don't self-deliver on a listen server.
	protected void NotifyPinChangeResult(ELIFE_EPinChangeResult result)
	{
		Rpc(RpcDo_PinChangeResult, result);

		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			m_OnPinChangeResult.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_PinChangeResult(ELIFE_EPinChangeResult result)
	{
		m_OnPinChangeResult.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	//! A new phone has no PIN: its owner picks one, or none, in setup.
	protected void ProvisionPhone(int playerId, string pin = "")
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
			return;

		ELIFE_ProvisionPhoneRequest request = new ELIFE_ProvisionPhoneRequest();
		request.characterId = ELIFE_CharacterIdentity.GetCharacterId(playerId);
		request.pin = pin;

		m_sServerOwnerCharacterId = request.characterId;
		m_sPendingOwnerName = ELIFE_CharacterIdentity.GetCharacterName(playerId);
		m_sPendingPin = request.pin;
		m_bBackendPoweredOn = false;
		m_ProvisionCallback = api.ProvisionPhone(request, this, "OnPhoneProvisioned");
	}

	//------------------------------------------------------------------------------------------------
	void OnPhoneProvisioned(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		int httpCode = HttpCodeOf(m_ProvisionCallback);
		NoteConnectivity(status, httpCode);

		ELIFE_ProvisionedPhone response = ELIFE_ProvisionedPhone.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !response)
		{
			//! A Bridge that still requires a PIN at provisioning gets a random one; setup replaces it.
			if (httpCode == 400 && m_sPendingPin == "")
			{
				ProvisionPhone(m_iOwnerPlayerId, string.Format("%1", Math.RandomIntInclusive(1000, 9999)));
				return;
			}

			Print("ELIFE_PhoneGadgetComponent | provisioning failed", LogLevel.ERROR);
			return;
		}

		m_sPhoneId = response.phoneId;
		m_sServerNumber = response.number;
		m_sServerPin = m_sPendingPin;
		m_sRedactedPin = ELIFE_DataRedactor.RedactDigits(m_sPendingPin);
		m_sOwnerName = m_sPendingOwnerName;
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

		//! The owner needs no PIN here, but the generated request always carries the field, so send the real one.
		ELIFE_SetPhonePowerRequest request = new ELIFE_SetPhonePowerRequest();
		request.characterId = ELIFE_CharacterIdentity.GetCharacterId(m_iOwnerPlayerId);
		request.isPoweredOn = true;
		request.pin = m_sServerPin;

		m_PowerOnCallback = api.SetPhonePower(request, m_sPhoneId, this, "OnPhonePoweredOn");
	}

	//------------------------------------------------------------------------------------------------
	void OnPhonePoweredOn(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		NoteConnectivity(status, HttpCodeOf(m_PowerOnCallback));

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print("ELIFE_PhoneGadgetComponent | power-on failed", LogLevel.ERROR);
			return;
		}

		m_bBackendPoweredOn = true;

		LoadPreferences();

		//! Before the poll: an owner fetch always answers, a poll that finds nothing new stays silent.
		RefetchFailedData();

		//! The home grid and the poll both follow what's installed, and the store decides what may stay.
		FetchData(DATA_APPS);
		FetchData(DATA_STORE_APPS);

		//! The inbox comes from the thread list; the poll only says when it changed.
		FetchData(DATA_MESSAGES);

		//! First chance to poll — don't wait a full idle interval for the opening screen to have mail.
		ReschedulePoll();
		PollNotifications();
	}

	//------------------------------------------------------------------------------------------------
	// Notification poll - the only delivery path, since the SignalR hub isn't reachable from here
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
		GetGame().GetCallqueue().Remove(PollNotifications);
		GetGame().GetCallqueue().CallLater(PollNotifications, interval, true);

		//! Screen just woke; don't wait a full interval for whatever arrived while it was off.
		if (waking)
			PollNotifications();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopPoll()
	{
		m_iPollIntervalMs = 0;
		GetGame().GetCallqueue().Remove(PollNotifications);
	}

	//------------------------------------------------------------------------------------------------
	protected void PollNotifications()
	{
		//! A fetch is already in flight; don't relabel it as the poll.
		if (m_sPhoneId == "" || m_mPendingFetches.Contains(DATA_NOTIFICATIONS))
			return;

		m_bPollInFlight = true;
		FetchData(DATA_NOTIFICATIONS);
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
		if (key == DATA_NOTIFICATIONS)
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

		//! A phone left off answers every app route with 409; OnPhonePoweredOn() re-fetches once it's on.
		if (!m_bBackendPoweredOn)
		{
			PowerOnPhone();
			return;
		}

		LoadPreferences();
		RefetchFailedData();
	}

	//------------------------------------------------------------------------------------------------
	protected void RefetchFailedData()
	{
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

		ELIFE_SavePhoneContactRequest request = new ELIFE_SavePhoneContactRequest();
		request.number = number;
		request.displayName = displayName;

		m_SaveContactCallback = api.SavePhoneContact(request, m_sPhoneId, this, "OnContactSaved");
	}

	//------------------------------------------------------------------------------------------------
	void OnContactSaved(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_SaveContactCallback);
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | contact save failed, HTTP=%1", httpCode), LogLevel.ERROR);
			NotifyContactSaveResult(ClassifySaveFailure(httpCode));
			return;
		}

		//! Bypasses the TTL cache-hit path - a cache hit here would push the stale pre-save list back.
		FetchData(DATA_CONTACTS, true);

		NotifyContactSaveResult(ELIFE_EContactSaveResult.SAVED);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only. Only the name changes - another number is another contact. Answers like SaveContact().
	void RenameContact(string contactId, string displayName)
	{
		if (!IsLocalCharacterOwner())
			return;

		Rpc(RpcAsk_RenameContact, contactId, displayName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RenameContact(string contactId, string displayName)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || contactId == "" || !api)
		{
			NotifyContactSaveResult(ELIFE_EContactSaveResult.FAILED);
			return;
		}

		ELIFE_RenamePhoneContactRequest request = new ELIFE_RenamePhoneContactRequest();
		request.displayName = displayName;

		m_sPendingRenameContactId = contactId;
		m_sPendingRenameName = displayName;
		m_RenameContactCallback = api.RenamePhoneContact(request, m_sPhoneId, contactId, this, "OnContactRenamed");
	}

	//------------------------------------------------------------------------------------------------
	void OnContactRenamed(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_RenameContactCallback);
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | contact rename failed, HTTP=%1", httpCode), LogLevel.ERROR);

			ELIFE_EContactSaveResult result = ELIFE_EContactSaveResult.FAILED;
			if (httpCode == 400)
				result = ELIFE_EContactSaveResult.INVALID_NAME;
			else if (httpCode == 404)
				result = ELIFE_EContactSaveResult.NOT_FOUND;

			NotifyContactSaveResult(result);

			//! Gone means the list is stale too.
			if (httpCode == 404)
				FetchData(DATA_CONTACTS, true);

			return;
		}

		//! Applied to the cache and pushed before the result, so the page it returns to already has the new
		//! name; the refetch after it only confirms.
		ELIFE_PhoneDataEntry entry;
		if (m_mServerData.Find(DATA_CONTACTS, entry) && entry.m_sReal != "")
		{
			ELIFE_ContactListDto cached = new ELIFE_ContactListDto();
			cached.ExpandFromRAW(entry.m_sReal);
			StoreData(DATA_CONTACTS, cached.WithName(m_sPendingRenameContactId, m_sPendingRenameName).GetRawJson());
			PushData(DATA_CONTACTS);
		}

		FetchData(DATA_CONTACTS, true);

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

		ELIFE_SendPhoneMessageRequest request = new ELIFE_SendPhoneMessageRequest();
		request.to.Insert(number);
		request.body = body;

		m_SendMessageCallback = api.SendPhoneMessage(request, m_sPhoneId, this, "OnMessageSent");
	}

	//------------------------------------------------------------------------------------------------
	void OnMessageSent(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_SendMessageCallback);
		NoteConnectivity(status, httpCode);

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | message send failed, HTTP=%1", httpCode), LogLevel.ERROR);
			NotifyMessageSendResult(ClassifySendFailure(httpCode), "");
			return;
		}

		ELIFE_SentMessage response = ELIFE_SentMessage.Cast(data);

		string threadId = "";
		ELIFE_EMessageSendResult result = ELIFE_EMessageSendResult.SENT;

		if (response)
		{
			threadId = response.threadId;

			//! One recipient per send, so any entry here means the message won't reach them.
			if (response.undeliverableRecipients.Count() > 0)
				result = ELIFE_EMessageSendResult.UNDELIVERABLE;
		}

		//! The sent message lives in its conversation; the inbox reorders around it.
		FetchThread(threadId);
		FetchData(DATA_MESSAGES, true);

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
		if (m_sPhoneId == "" || threadId == "")
			return;

		if (m_sPendingReadThreadId != "")
		{
			m_aMarkReadQueue.Insert(threadId);
			return;
		}

		BeginMarkThreadRead(threadId);
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginMarkThreadRead(string threadId)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || threadId == "" || !api)
			return;

		m_sPendingReadThreadId = threadId;
		m_MarkThreadReadCallback = api.MarkPhoneThreadRead(m_sPhoneId, threadId, this, "OnThreadMarkedRead");
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the next queued mark-read, skipping entries that can't begin so the queue never wedges.
	protected void DrainMarkReadQueue()
	{
		while (m_aMarkReadQueue.Count() > 0 && m_sPendingReadThreadId == "")
		{
			string nextId = m_aMarkReadQueue.Get(0);
			m_aMarkReadQueue.RemoveOrdered(0);
			BeginMarkThreadRead(nextId);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnThreadMarkedRead(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_MarkThreadReadCallback);
		NoteConnectivity(status, httpCode);

		string threadId = m_sPendingReadThreadId;
		m_sPendingReadThreadId = "";

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | mark thread read failed, HTTP=%1", httpCode), LogLevel.WARNING);
		}
		else
		{
			ClearCachedThreadUnread(threadId);

			//! The backend drops a thread's notifications when it's read; mirror that before the poll does.
			DropCachedThreadNotifications(threadId);
			FetchData(DATA_NOTIFICATIONS, true);
			FetchData(DATA_MESSAGES, true);
		}

		DrainMarkReadQueue();
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_EContactSaveResult ClassifySaveFailure(int httpCode)
	{
		if (httpCode == 400)
			return ELIFE_EContactSaveResult.INVALID_NUMBER;

		//! The backend also answers 409 for "phone is powered off", which isn't the player's duplicate.
		if (httpCode == 409 && m_bBackendPoweredOn)
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
	//! Owner-only.
	void DeleteContact(string contactId)
	{
		if (!IsLocalCharacterOwner() || contactId == "")
			return;

		Rpc(RpcAsk_DeleteContact, contactId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeleteContact(string contactId)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || contactId == "" || !api)
		{
			NotifyContactDeleteResult(false);
			return;
		}

		m_DeleteContactCallback = api.DeletePhoneContact(m_sPhoneId, contactId, this, "OnContactDeleted");
	}

	//------------------------------------------------------------------------------------------------
	void OnContactDeleted(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_DeleteContactCallback);
		NoteConnectivity(status, httpCode);

		bool deleted = status == ELIFE_EApiStatusCode.SUCCESS;
		if (!deleted)
			Print(string.Format("ELIFE_PhoneGadgetComponent | contact delete failed, HTTP=%1", httpCode), LogLevel.ERROR);

		//! A 404 means the list on screen is stale, so refresh on that failure too. Bypasses the TTL cache like a save.
		if (deleted || httpCode == 404)
			FetchData(DATA_CONTACTS, true);

		NotifyContactDeleteResult(deleted);
	}

	//------------------------------------------------------------------------------------------------
	//! Self-applies locally for the owner too - RplRcver.Owner RPCs don't self-deliver on a listen server.
	protected void NotifyContactDeleteResult(bool deleted)
	{
		Rpc(RpcDo_ContactDeleteResult, deleted);

		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			m_OnContactDeleteResult.Invoke(deleted);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ContactDeleteResult(bool deleted)
	{
		m_OnContactDeleteResult.Invoke(deleted);
	}

	//------------------------------------------------------------------------------------------------
	// Blocklist write path
	//------------------------------------------------------------------------------------------------

	//! Owner-only. Both routes are idempotent, so a toggle that races a stale cache still lands right.
	void SetNumberBlocked(string number, bool blocked)
	{
		if (!IsLocalCharacterOwner() || number == "")
			return;

		Rpc(RpcAsk_SetNumberBlocked, number, blocked);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetNumberBlocked(string number, bool blocked)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (m_sPhoneId == "" || number == "" || !api || m_sPendingBlockNumber != "")
		{
			NotifyBlockResult(number, blocked, false);
			return;
		}

		m_sPendingBlockNumber = number;
		m_bPendingBlock = blocked;

		if (blocked)
		{
			ELIFE_BlockPhoneNumberRequest request = new ELIFE_BlockPhoneNumberRequest();
			request.number = number;
			m_BlockCallback = api.BlockPhoneNumber(request, m_sPhoneId, this, "OnBlockChanged");
		}
		else
		{
			m_BlockCallback = api.UnblockPhoneNumber(m_sPhoneId, number, this, "OnBlockChanged");
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnBlockChanged(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_BlockCallback);
		NoteConnectivity(status, httpCode);

		string number = m_sPendingBlockNumber;
		bool blocked = m_bPendingBlock;
		m_sPendingBlockNumber = "";

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | block toggle failed, HTTP=%1", httpCode), LogLevel.ERROR);
			NotifyBlockResult(number, blocked, false);
			return;
		}

		//! Applied to the cache directly - the routes answer 204, and only GetPhone() can list the blocklist again.
		ELIFE_PhoneDataEntry entry;
		if (m_mServerData.Find(DATA_BLOCKS, entry) && entry.m_sReal != "")
		{
			ELIFE_BlockListDto cached = new ELIFE_BlockListDto();
			cached.ExpandFromRAW(entry.m_sReal);
			StoreData(DATA_BLOCKS, cached.With(number, blocked).GetRawJson());
			PushData(DATA_BLOCKS);
		}
		else
		{
			FetchData(DATA_BLOCKS);
		}

		NotifyBlockResult(number, blocked, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Self-applies locally for the owner too - RplRcver.Owner RPCs don't self-deliver on a listen server.
	protected void NotifyBlockResult(string number, bool blocked, bool succeeded)
	{
		Rpc(RpcDo_BlockResult, number, blocked, succeeded);

		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			m_OnBlockResult.Invoke(number, blocked, succeeded);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_BlockResult(string number, bool blocked, bool succeeded)
	{
		m_OnBlockResult.Invoke(number, blocked, succeeded);
	}

	//------------------------------------------------------------------------------------------------
	// Installed apps
	//------------------------------------------------------------------------------------------------

	//! Server-side cache on the authority, the pushed copy everywhere else.
	protected string InstalledAppsJson()
	{
		ELIFE_PhoneDataEntry entry;
		if (Replication.IsServer() && m_mServerData.Find(DATA_APPS, entry))
			return entry.m_sReal;

		return GetData(DATA_APPS);
	}

	//------------------------------------------------------------------------------------------------
	//! False until the first installed-apps payload lands.
	bool HasInstalledApps()
	{
		return InstalledAppsJson() != "";
	}

	//------------------------------------------------------------------------------------------------
	//! False until the list lands, so a downloadable app never shows before it's confirmed.
	bool IsAppInstalled(string appKey)
	{
		string json = InstalledAppsJson();
		if (json == "")
			return false;

		ELIFE_PhoneAppListDto apps = new ELIFE_PhoneAppListDto();
		apps.ExpandFromRAW(json);
		return apps.Has(appKey);
	}

	//------------------------------------------------------------------------------------------------
	//! False until the store list lands, so an app is never offered before the backend says so.
	bool IsAppOffered(string appKey)
	{
		string json;
		ELIFE_PhoneDataEntry entry;
		if (Replication.IsServer() && m_mServerData.Find(DATA_STORE_APPS, entry))
			json = entry.m_sReal;
		else
			json = GetData(DATA_STORE_APPS);

		if (json == "")
			return false;

		ELIFE_StoreAppListDto store = new ELIFE_StoreAppListDto();
		store.ExpandFromRAW(json);
		return store.Offers(appKey);
	}

	//------------------------------------------------------------------------------------------------
	//! OS screens and system apps always are; downloadable apps only while installed.
	bool IsAppAvailable(EPhoneScreenState state)
	{
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.Find(state);
		if (!app || app.IsSystem())
			return true;

		return IsAppInstalled(app.m_sAppKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. An app removed while open (here or from elsewhere) drops the screen to home.
	protected void LeaveUninstalledApp()
	{
		if (!IsAppAvailable(m_eScreenState))
			RpcAsk_SetScreenState(EPhoneScreenState.HOME);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only. Both routes are idempotent, so a tap that races a stale list still lands right.
	void SetAppInstalled(string appKey, bool installed)
	{
		if (!IsLocalCharacterOwner() || appKey == "")
			return;

		Rpc(RpcAsk_SetAppInstalled, appKey, installed);
	}

	//------------------------------------------------------------------------------------------------
	//! A platform command, so unlike app routes it carries the actor - same credentials as power-on.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetAppInstalled(string appKey, bool installed)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.FindByKey(appKey);
		if (m_sPhoneId == "" || !api || !app || app.IsSystem() || m_sPendingAppKey != "")
		{
			NotifyAppInstallResult(appKey, installed, false);
			return;
		}

		//! The catalog's casing, which is the backend's.
		m_sPendingAppKey = app.m_sAppKey;
		m_bPendingInstall = installed;

		string characterId = ELIFE_CharacterIdentity.GetCharacterId(m_iOwnerPlayerId);

		if (installed)
		{
			ELIFE_PhoneActorRequest request = new ELIFE_PhoneActorRequest();
			request.characterId = characterId;
			request.pin = m_sServerPin;
			m_AppInstallCallback = api.InstallPhoneApp(request, m_sPhoneId, m_sPendingAppKey, this, "OnAppInstallChanged");
		}
		else
		{
			m_AppInstallCallback = api.UninstallPhoneApp(m_sPhoneId, m_sPendingAppKey, characterId, m_sServerPin, this, "OnAppInstallChanged");
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnAppInstallChanged(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_AppInstallCallback);
		NoteConnectivity(status, httpCode);

		string appKey = m_sPendingAppKey;
		bool installed = m_bPendingInstall;
		m_sPendingAppKey = "";

		if (status != ELIFE_EApiStatusCode.SUCCESS)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | app %1 (install=%2) failed, HTTP=%3", appKey, installed, httpCode), LogLevel.ERROR);
			NotifyAppInstallResult(appKey, installed, false);
			return;
		}

		//! Applied to the cache directly - both routes answer 204, and the push is what moves home and the store.
		ELIFE_PhoneDataEntry entry;
		if (m_mServerData.Find(DATA_APPS, entry) && entry.m_sReal != "")
		{
			ELIFE_PhoneAppListDto cached = new ELIFE_PhoneAppListDto();
			cached.ExpandFromRAW(entry.m_sReal);
			StoreData(DATA_APPS, cached.With(appKey, installed).GetRawJson());
			PushData(DATA_APPS);
			LeaveUninstalledApp();
		}
		else
		{
			FetchData(DATA_APPS);
		}

		NotifyAppInstallResult(appKey, installed, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. System apps can't be removed here, but the Bridge still gates their routes on being
	//! installed, so one missing from the list (an older phone, a staff edit) is put back silently.
	protected void InstallMissingSystemApps()
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api || m_sPhoneId == "" || m_iSystemInstallsPending > 0 || !HasInstalledApps())
			return;

		ELIFE_PhoneActorRequest request = new ELIFE_PhoneActorRequest();
		request.characterId = ELIFE_CharacterIdentity.GetCharacterId(m_iOwnerPlayerId);
		request.pin = m_sServerPin;

		array<ref ELIFE_PhoneAppEntry> catalog = ELIFE_PhoneAppCatalog.GetEntries();
		foreach (ELIFE_PhoneAppEntry app : catalog)
		{
			if (!app.IsSystem() || app.m_sAppKey == "" || IsAppInstalled(app.m_sAppKey))
				continue;

			m_iSystemInstallsPending++;
			api.InstallPhoneApp(request, m_sPhoneId, app.m_sAppKey, this, "OnSystemAppInstalled");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Refetches once the last one answers. A failure isn't retried until the next list fetch.
	void OnSystemAppInstalled(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		m_iSystemInstallsPending--;

		if (status == ELIFE_EApiStatusCode.SUCCESS)
			m_bSystemInstallLanded = true;
		else
			Print("ELIFE_PhoneGadgetComponent | system app install failed", LogLevel.WARNING);

		if (m_iSystemInstallsPending > 0 || !m_bSystemInstallLanded)
			return;

		m_bSystemInstallLanded = false;
		FetchData(DATA_APPS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. The backend's store decides what may stay installed, so a downloadable app it no longer
	//! offers is removed once both lists have landed.
	protected void UninstallDroppedApps()
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api || m_sPhoneId == "" || m_iDroppedUninstallsPending > 0)
			return;

		ELIFE_PhoneDataEntry installedEntry;
		ELIFE_PhoneDataEntry storeEntry;
		if (!m_mServerData.Find(DATA_APPS, installedEntry) || installedEntry.m_sReal == "")
			return;

		if (!m_mServerData.Find(DATA_STORE_APPS, storeEntry) || storeEntry.m_sReal == "")
			return;

		ELIFE_PhoneAppListDto installed = new ELIFE_PhoneAppListDto();
		installed.ExpandFromRAW(installedEntry.m_sReal);

		ELIFE_StoreAppListDto store = new ELIFE_StoreAppListDto();
		store.ExpandFromRAW(storeEntry.m_sReal);

		foreach (ELIFE_PhoneApp app : installed.items)
		{
			if (!app || app.isSystemApp || store.Offers(app.key))
				continue;

			ELIFE_PhoneAppEntry known = ELIFE_PhoneAppCatalog.FindByKey(app.key);
			if (known && known.IsSystem())
				continue;

			m_iDroppedUninstallsPending++;
			api.UninstallPhoneApp(m_sPhoneId, app.key, m_sServerOwnerCharacterId, m_sServerPin, this, "OnDroppedAppUninstalled");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Refetches once the last one answers, so home and the Store drop the app. A failure waits for the next list.
	void OnDroppedAppUninstalled(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		m_iDroppedUninstallsPending--;

		if (status == ELIFE_EApiStatusCode.SUCCESS)
			m_bDroppedUninstallLanded = true;
		else
			Print("ELIFE_PhoneGadgetComponent | removing an app the store dropped failed", LogLevel.WARNING);

		if (m_iDroppedUninstallsPending > 0 || !m_bDroppedUninstallLanded)
			return;

		m_bDroppedUninstallLanded = false;
		FetchData(DATA_APPS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Self-applies locally for the owner too - RplRcver.Owner RPCs don't self-deliver on a listen server.
	protected void NotifyAppInstallResult(string appKey, bool installed, bool succeeded)
	{
		Rpc(RpcDo_AppInstallResult, appKey, installed, succeeded);

		if (SCR_PlayerController.GetLocalControlledEntity() && IsLocalCharacterOwner())
			m_OnAppInstallResult.Invoke(appKey, installed, succeeded);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AppInstallResult(string appKey, bool installed, bool succeeded)
	{
		m_OnAppInstallResult.Invoke(appKey, installed, succeeded);
	}

	//------------------------------------------------------------------------------------------------
	//! Owns each key's route and response parsing. afterWrite: the data just changed, so a fetch already in
	//! flight is out of date - its answer is dropped and this one runs once it lands.
	protected void FetchData(string key, bool afterWrite = false)
	{
		//! A second fetch would just overwrite the pending callback and waste a round trip.
		if (m_mPendingFetches.Contains(key))
		{
			if (afterWrite)
				m_mStaleFetches.Set(key, true);

			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			PushDataError(key);
			return;
		}

		//! Contacts/messages routes take no characterId/pin - possession is proven once at power-on.
		if (key == DATA_CONTACTS)
		{
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.ListPhoneContacts(m_sPhoneId, this, "OnContactsFetched"));
		}
		else if (key == DATA_MESSAGES)
		{
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.ListPhoneThreads(m_sPhoneId, this, "OnThreadsFetched"));
		}
		else if (key == DATA_NOTIFICATIONS)
		{
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.ListPhoneNotifications(m_sPhoneId, "", this, "OnNotificationsFetched"));
		}
		else if (key == DATA_BLOCKS)
		{
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.GetPhone(m_sPhoneId, this, "OnPhoneFetched"));
		}
		else if (key == DATA_APPS)
		{
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.ListPhoneApps(m_sPhoneId, this, "OnAppsFetched"));
		}
		else if (key == DATA_STORE_APPS)
		{
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.ListStoreApps(m_sPhoneId, this, "OnStoreAppsFetched"));
		}
		else if (key == DATA_BANK_ACCOUNTS && m_sServerOwnerCharacterId != "")
		{
			//! The registered owner's accounts - like every other app here, it shows the phone's data, not the holder's.
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.ListCharacterBankAccounts(m_sServerOwnerCharacterId, this, "OnBankAccountsFetched"));
		}
		else if (key.StartsWith(DATA_BANK_STATEMENT_PREFIX) && m_sServerOwnerCharacterId != "")
		{
			//! Account first, then its transactions - see OnStatementAccountFetched().
			string accountId = key.Substring(DATA_BANK_STATEMENT_PREFIX.Length(), key.Length() - DATA_BANK_STATEMENT_PREFIX.Length());
			m_mPendingFetches.Set(key, true);
			m_mFetchCallbacks.Set(key, api.GetBankAccount(accountId, this, "OnStatementAccountFetched"));
		}
		else
		{
			PushDataError(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnThreadsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		OnListFetched(status, DATA_MESSAGES, new ELIFE_MessageThreadSummaryListDto());
	}

	//------------------------------------------------------------------------------------------------
	void OnNotificationsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		OnListFetched(status, DATA_NOTIFICATIONS, new ELIFE_PhoneNotificationListDto());
	}

	//------------------------------------------------------------------------------------------------
	void OnContactsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		OnListFetched(status, DATA_CONTACTS, new ELIFE_ContactListDto());
	}

	//------------------------------------------------------------------------------------------------
	void OnAppsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		OnListFetched(status, DATA_APPS, new ELIFE_PhoneAppListDto());
	}

	//------------------------------------------------------------------------------------------------
	void OnStoreAppsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		OnListFetched(status, DATA_STORE_APPS, new ELIFE_StoreAppListDto());
	}

	//------------------------------------------------------------------------------------------------
	void OnBankAccountsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string characterId)
	{
		OnListFetched(status, DATA_BANK_ACCOUNTS, new ELIFE_BankAccountListDto());
	}

	//------------------------------------------------------------------------------------------------
	//! Generated list routes don't parse their bare-array body, so it's read off the callback and wrapped here.
	protected void OnListFetched(ELIFE_EApiStatusCode status, string key, notnull ELIFE_PhoneRawDto list, string head = "")
	{
		RestCallback callback;
		m_mFetchCallbacks.Find(key, callback);

		if (status != ELIFE_EApiStatusCode.SUCCESS || !callback)
		{
			OnDataFetched(ELIFE_EApiStatusCode.ERROR, null, key);
			return;
		}

		list.ExpandFromArray(callback.GetData(), head);
		OnDataFetched(status, list, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Only the blocklist is kept; the rest of the summary is platform state no app renders.
	void OnPhoneFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		ELIFE_PhoneSummary phone = ELIFE_PhoneSummary.Cast(data);

		ELIFE_BlockListDto blocks;
		if (phone)
		{
			blocks = new ELIFE_BlockListDto();
			foreach (string number : phone.blockedNumbers)
				blocks.numbers.Insert(number);

			blocks.StashPacked();
		}

		OnDataFetched(status, blocks, DATA_BLOCKS);
	}

	//------------------------------------------------------------------------------------------------
	//! Step one of a statement. The id came from a client, so it has to be one of the owner's accounts.
	void OnStatementAccountFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string bankAccountId)
	{
		string key = DATA_BANK_STATEMENT_PREFIX + bankAccountId;

		ELIFE_BankAccountSummary account = ELIFE_BankAccountSummary.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !account)
		{
			OnDataFetched(ELIFE_EApiStatusCode.ERROR, null, key);
			return;
		}

		if (account.characterId != m_sServerOwnerCharacterId)
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | bank account %1 is not the owner's", bankAccountId), LogLevel.WARNING);
			OnDataFetched(ELIFE_EApiStatusCode.ERROR, null, key);
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
		{
			OnDataFetched(ELIFE_EApiStatusCode.ERROR, null, key);
			return;
		}

		//! The account rides in front of the transactions array, so the pair lands as one payload.
		ELIFE_BankAccountSummary copy = account.Copy();
		copy.Pack();
		m_mStatementAccounts.Set(bankAccountId, copy.AsString());

		m_mFetchCallbacks.Set(key, api.ListBankAccountTransactions(bankAccountId, this, "OnStatementTransactionsFetched"));
	}

	//------------------------------------------------------------------------------------------------
	void OnStatementTransactionsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string bankAccountId)
	{
		string accountJson;
		m_mStatementAccounts.Find(bankAccountId, accountJson);
		m_mStatementAccounts.Remove(bankAccountId);

		OnListFetched(status, DATA_BANK_STATEMENT_PREFIX + bankAccountId, new ELIFE_BankStatementDto(), "\"account\":" + accountJson + ",");
	}

	//------------------------------------------------------------------------------------------------
	void OnDataFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string key)
	{
		m_mPendingFetches.Remove(key);

		if (m_mStaleFetches.Contains(key))
		{
			m_mStaleFetches.Remove(key);
			FetchData(key);
			return;
		}

		bool wasPoll = key == DATA_NOTIFICATIONS && m_bPollInFlight;
		if (key == DATA_NOTIFICATIONS)
			m_bPollInFlight = false;

		RestCallback callback;
		m_mFetchCallbacks.Find(key, callback);
		m_mFetchCallbacks.Remove(key);
		NoteConnectivity(status, HttpCodeOf(callback));

		//! The poll keeps running while suspended, so it also notices when the phone is restored.
		if (key == DATA_NOTIFICATIONS)
		{
			if (status == ELIFE_EApiStatusCode.SUCCESS)
				SetSuspended(false);
			else if (HttpCodeOf(callback) == 403)
				SetSuspended(true);
		}

		if (status != ELIFE_EApiStatusCode.SUCCESS || !data)
		{
			//! A background poll failure only warns; the Offline screen already covers connectivity.
			if (wasPoll)
			{
				Print("ELIFE_PhoneGadgetComponent | notification poll failed", LogLevel.WARNING);
				return;
			}

			Print(string.Format("ELIFE_PhoneGadgetComponent | fetch failed for '%1'", key), LogLevel.ERROR);
			PushDataError(key);
			return;
		}

		//! Re-packing would warn "Operation called twice", so payloads keep the raw text they were unpacked from.
		string real;
		ELIFE_PhoneRawDto raw = ELIFE_PhoneRawDto.Cast(data);
		if (raw)
			real = raw.GetRawJson();

		if (key == DATA_NOTIFICATIONS)
		{
			OnNotificationsLanded(ELIFE_PhoneNotificationListDto.Cast(data), real, wasPoll);
			return;
		}

		if (key == DATA_MESSAGES)
		{
			ELIFE_MessageUpdates inbox = CachedMessages().WithThreadList(ELIFE_MessageThreadSummaryListDto.Cast(data));
			inbox.Pack();
			real = inbox.AsString();
		}

		StoreData(key, real);
		PushData(key);

		//! A messages fetch that landed first used redacted numbers as titles - rebuild it now that names exist.
		if (key == DATA_CONTACTS)
			RefreshBystanderMessages();

		if (key == DATA_APPS)
		{
			LeaveUninstalledApp();
			InstallMissingSystemApps();
		}

		if (key == DATA_APPS || key == DATA_STORE_APPS)
			UninstallDroppedApps();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Caches the real payload with its bystander copy; PushData() sends both.
	protected void StoreData(string key, string realJson)
	{
		ELIFE_PhoneDataEntry entry = new ELIFE_PhoneDataEntry();
		entry.m_sReal = realJson;
		entry.m_sRedacted = RedactPayload(key, realJson);
		entry.m_iFetchTime = System.GetTickCount();
		m_mServerData.Set(key, entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Zeroed here so the UI and bystanders update at once; the thread list confirms it after.
	protected void ClearCachedThreadUnread(string threadId)
	{
		if (threadId == "")
			return;

		ELIFE_PhoneDataEntry entry;
		if (!m_mServerData.Find(DATA_MESSAGES, entry) || entry.m_sReal == "")
			return;

		ELIFE_MessageUpdates updates = new ELIFE_MessageUpdates();
		updates.ExpandFromRAW(entry.m_sReal);

		ELIFE_MessageThreadUpdate threadDto = updates.FindThread(threadId);
		if (!threadDto || threadDto.unreadCount == 0)
			return;

		//! ExpandFromRAW'd structs can't be Pack()ed — rebuild into a fresh tree.
		ELIFE_MessageUpdates rebuilt = new ELIFE_MessageUpdates();
		foreach (ELIFE_MessageThreadUpdate existing : updates.threads)
		{
			ELIFE_MessageThreadUpdate copy = existing.Copy();
			if (copy.threadId == threadId)
				copy.unreadCount = 0;

			rebuilt.threads.Insert(copy);
		}

		rebuilt.Pack();
		StoreData(DATA_MESSAGES, rebuilt.AsString());
		PushData(DATA_MESSAGES);
	}

	//------------------------------------------------------------------------------------------------
	protected void DropCachedThreadNotifications(string threadId)
	{
		ELIFE_PhoneDataEntry entry;
		if (threadId == "" || !m_mServerData.Find(DATA_NOTIFICATIONS, entry) || entry.m_sReal == "")
			return;

		ELIFE_PhoneNotificationListDto cached = new ELIFE_PhoneNotificationListDto();
		cached.ExpandFromRAW(entry.m_sReal);

		array<string> noIds = {};
		ELIFE_PhoneNotificationListDto remaining = cached.Without(noIds, threadId);
		if (remaining.items.Count() == cached.items.Count())
			return;

		StoreData(DATA_NOTIFICATIONS, remaining.GetRawJson());
		PushData(DATA_NOTIFICATIONS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. A fresh tree from the cached inbox, empty if there is none yet.
	protected ELIFE_MessageUpdates CachedMessages()
	{
		ELIFE_MessageUpdates cached = new ELIFE_MessageUpdates();

		ELIFE_PhoneDataEntry entry;
		if (m_mServerData.Find(DATA_MESSAGES, entry) && entry.m_sReal != "")
			cached.ExpandFromRAW(entry.m_sReal);

		return cached;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a freshly built tree - an ExpandFromRAW'd one can't be Pack()ed.
	protected void StoreMessages(notnull ELIFE_MessageUpdates merged)
	{
		merged.Pack();
		StoreData(DATA_MESSAGES, merged.AsString());
		PushData(DATA_MESSAGES);
	}

	//------------------------------------------------------------------------------------------------
	//! New notifications put their text into the inbox before the queue goes out, so a banner never points
	//! at a message the inbox doesn't have yet. Then the inbox and the open conversation catch up.
	protected void OnNotificationsLanded(ELIFE_PhoneNotificationListDto incoming, string real, bool wasPoll)
	{
		ELIFE_PhoneDataEntry entry;
		bool hasCache = m_mServerData.Find(DATA_NOTIFICATIONS, entry);
		if (hasCache && entry.m_sReal == real)
		{
			//! A quiet poll stays silent; an owner request always answers since its app is loading.
			if (!wasPoll)
				PushData(DATA_NOTIFICATIONS);

			return;
		}

		ELIFE_PhoneNotificationListDto previous = new ELIFE_PhoneNotificationListDto();
		if (hasCache && entry.m_sReal != "")
			previous.ExpandFromRAW(entry.m_sReal);

		array<ELIFE_PhoneNotification> fresh = {};
		bool fullThreadNotified = false;
		if (incoming)
		{
			foreach (ELIFE_PhoneNotification notification : incoming.items)
			{
				if (!notification || previous.Has(notification.notificationId))
					continue;

				fresh.Insert(notification);
				if (notification.groupKey == m_sFullThreadId)
					fullThreadNotified = true;
			}
		}

		if (!fresh.IsEmpty())
			StoreMessages(CachedMessages().WithNotificationPreviews(fresh, m_sFullThreadId));

		StoreData(DATA_NOTIFICATIONS, real);
		PushData(DATA_NOTIFICATIONS);

		if (fresh.IsEmpty())
			return;

		FetchData(DATA_MESSAGES, true);

		if (fullThreadNotified)
			FetchThread(m_sFullThreadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner-only. Opening a conversation loads its full retained history into the messages payload.
	void RequestThread(string threadId)
	{
		if (IsLocalCharacterOwner() && threadId != "")
			Rpc(RpcAsk_RequestThread, threadId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestThread(string threadId)
	{
		FetchThread(threadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. A request while one is in flight runs after it, so an answer can't predate a send.
	protected void FetchThread(string threadId)
	{
		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (threadId == "" || m_sPhoneId == "" || !api)
			return;

		if (m_ThreadFetchCallback)
		{
			m_sQueuedThreadFetch = threadId;
			return;
		}

		m_sFetchingThreadId = threadId;
		m_ThreadFetchCallback = api.GetPhoneThread(m_sPhoneId, threadId, this, "OnThreadFetched");
	}

	//------------------------------------------------------------------------------------------------
	void OnThreadFetched(ELIFE_EApiStatusCode status, JsonApiStruct data, string phoneId)
	{
		int httpCode = HttpCodeOf(m_ThreadFetchCallback);
		string threadId = m_sFetchingThreadId;
		m_ThreadFetchCallback = null;
		m_sFetchingThreadId = "";
		NoteConnectivity(status, httpCode);

		ELIFE_MessageThreadDetail detail = ELIFE_MessageThreadDetail.Cast(data);
		if (status == ELIFE_EApiStatusCode.SUCCESS && detail)
		{
			m_sFullThreadId = detail.threadId;
			StoreMessages(CachedMessages().WithThreadDetail(detail));
		}
		else
		{
			Print(string.Format("ELIFE_PhoneGadgetComponent | thread %1 not loaded, HTTP=%2", threadId, httpCode), LogLevel.WARNING);
		}

		if (m_sQueuedThreadFetch == "")
			return;

		string next = m_sQueuedThreadFetch;
		m_sQueuedThreadFetch = "";
		FetchThread(next);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the cached JSON: a poll response is only a delta, but bystanders mirror the merged payload.
	protected string RedactPayload(string key, string realJson)
	{
		if (realJson == "")
			return "";

		//! The installed set is on the home grid anyway, and the store offers the same to everyone.
		if (key == DATA_APPS || key == DATA_STORE_APPS)
			return realJson;

		JsonApiStruct redacted;

		if (key == DATA_CONTACTS)
		{
			ELIFE_ContactListDto contacts = new ELIFE_ContactListDto();
			contacts.ExpandFromRAW(realJson);
			redacted = contacts.Redact();
		}
		else if (key == DATA_MESSAGES)
		{
			ELIFE_MessageUpdates updates = new ELIFE_MessageUpdates();
			updates.ExpandFromRAW(realJson);
			redacted = updates.Redact(OwnerContactNames());
		}
		else if (key == DATA_NOTIFICATIONS)
		{
			ELIFE_PhoneNotificationListDto notifications = new ELIFE_PhoneNotificationListDto();
			notifications.ExpandFromRAW(realJson);
			redacted = notifications.Redact();
		}
		else if (key == DATA_BLOCKS)
		{
			//! Block controls are owner-only, so bystanders get an empty list rather than stand-in numbers.
			redacted = new ELIFE_BlockListDto();
		}
		else if (key == DATA_BANK_ACCOUNTS)
		{
			ELIFE_BankAccountListDto accounts = new ELIFE_BankAccountListDto();
			accounts.ExpandFromRAW(realJson);
			redacted = accounts.Redact();
		}
		else if (key.StartsWith(DATA_BANK_STATEMENT_PREFIX))
		{
			ELIFE_BankStatementDto statement = new ELIFE_BankStatementDto();
			statement.ExpandFromRAW(realJson);
			redacted = statement.Redact();
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
		foreach (ELIFE_ContactSummary contact : list.items)
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
	//! Message bodies are player-written, so the owner's copy goes through the profanity filter first.
	protected void ApplyData(string key, string json)
	{
		if (key == DATA_MESSAGES && json != "" && IsLocalCharacterOwner())
		{
			if (!m_MessageFilter)
			{
				m_MessageFilter = new ELIFE_PhoneMessageFilter();
				m_MessageFilter.m_Phone = this;
			}

			m_MessageFilter.Filter(json);
			return;
		}

		if (key == DATA_NOTIFICATIONS && IsLocalCharacterOwner() && m_MessageFilter && m_MessageFilter.IsBusy())
		{
			m_sQueuedNotificationsJson = json;
			m_bNotificationsQueued = true;
			return;
		}

		//! A newer payload supersedes one still waiting on the filter.
		if (key == DATA_NOTIFICATIONS)
			m_bNotificationsQueued = false;

		SetLocalData(key, json);
	}

	//------------------------------------------------------------------------------------------------
	void OnMessagesFiltered(string json)
	{
		// The phone may have changed hands while filtering; don't overwrite the newer redacted copy.
		if (!IsLocalCharacterOwner())
			return;

		SetLocalData(DATA_MESSAGES, json);

		if (!m_bNotificationsQueued)
			return;

		m_bNotificationsQueued = false;
		SetLocalData(DATA_NOTIFICATIONS, m_sQueuedNotificationsJson);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLocalData(string key, string json)
	{
		//! Decided before the swap - the comparison is against what this machine was already holding.
		bool announce = key == DATA_NOTIFICATIONS && HasNewNotifications(json);

		m_mData.Set(key, json);
		SetDataStatus(key, ELIFE_EPhoneDataStatus.READY);

		if (!announce)
			return;

		//! Do Not Disturb keeps the arrival quiet: still counted and listed, just not announced.
		if (m_bDoNotDisturb)
			return;

		if (m_SoundComponent)
			m_SoundComponent.SoundEvent(SOUND_EVENT_NOTIFICATION);

		//! A peek is a door into the phone, and a phone still in setup has nowhere to take it.
		if (ELIFE_PhonePrefs.IsPeekEnabled() && m_bSetupCompleted)
			ELIFE_PhonePeek.Show(this);
	}

	//------------------------------------------------------------------------------------------------
	//! The first payload is only the baseline, so opening the phone never replays a waiting notification.
	protected bool HasNewNotifications(string incomingJson)
	{
		if (!IsLocalCharacterOwner())
			return false;

		string previousJson = GetData(DATA_NOTIFICATIONS);
		if (previousJson == "" || incomingJson == "")
			return false;

		ELIFE_PhoneNotificationListDto previous = new ELIFE_PhoneNotificationListDto();
		previous.ExpandFromRAW(previousJson);

		ELIFE_PhoneNotificationListDto incoming = new ELIFE_PhoneNotificationListDto();
		incoming.ExpandFromRAW(incomingJson);

		foreach (ELIFE_PhoneNotification notification : incoming.items)
		{
			if (notification && notification.IsMessage() && !previous.Has(notification.notificationId))
				return true;
		}

		return false;
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
		if (m_bHasRealIdentity || m_sPhoneId == "")
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
		//! Only setup's last step leaves setup, so every way in lands there until then.
		if (!m_bSetupCompleted && state != EPhoneScreenState.OFF)
			state = EPhoneScreenState.SETUP;
		else if (m_bSetupCompleted && state == EPhoneScreenState.SETUP)
			state = EPhoneScreenState.HOME;

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

		//! Covers every way in - tiles, resume, peek doors, in-app hand-offs.
		if (!IsAppAvailable(state))
			state = EPhoneScreenState.HOME;

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
		if (state == EPhoneScreenState.OFF || state == EPhoneScreenState.LOCKED || state == EPhoneScreenState.SETUP)
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
