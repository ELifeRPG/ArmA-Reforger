//------------------------------------------------------------------------------------------------
enum EPhoneScreenState
{
	OFF,
	LOCKED,
	HOME,
	BANK,
	MAP,
	MESSAGES,
	SETTINGS
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

	[RplProp()]
	protected string m_sNumber;

	[RplProp()]
	protected string m_sPin;

	protected ref ELIFE_ProvisionPhoneCallback m_ProvisionCallback;
	protected ref ELIFE_BaseRestCallback m_PowerOnCallback;
	protected int m_iOwnerPlayerId;

	//! Server: last fetched contacts (real) - client: whichever variant this machine's RPC channel delivered.
	protected ref array<ref ELIFE_ContactDto> m_aContacts = {};
	protected int m_iContactsCacheTime;
	protected ref ELIFE_ContactsFetchCallback m_ContactsFetchCallback;
	protected const int CONTACTS_CACHE_TTL_MS = 15000;

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

	//! Defined in the phone's own Phone_UI.acp (reuses vanilla UI_Task_Succeded/Canceled.wav).
	protected const string SOUND_EVENT_POWER_ON = "SOUND_PHONE_POWER_ON";
	protected const string SOUND_EVENT_POWER_OFF = "SOUND_PHONE_POWER_OFF";

	//! Reserved for a future notification feature (see Phone_UI.acp) - not triggered anywhere yet.
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
		ELIFE_ProvisionPhoneResponseDto response = ELIFE_ProvisionPhoneResponseDto.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !response)
		{
			Print("ELIFE_PhoneGadgetComponent | provisioning failed", LogLevel.ERROR);
			return;
		}

		m_sPhoneId = response.phoneId;
		m_sNumber = response.number;
		m_sPin = pin;
		Replication.BumpMe();

		//! onRplName only reliably fires on remote proxies, not the machine setting the value (see
		//! RpcAsk_SetScreenState() below, which applies locally the same way) - on a listen server the
		//! equipping player IS that machine, so OnPhoneIdUpdated() alone would never fire for them.
		OnPhoneIdUpdated();

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
		if (status != ELIFE_EApiStatusCode.SUCCESS)
			Print("ELIFE_PhoneGadgetComponent | power-on failed", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	array<ref ELIFE_ContactDto> GetContacts()
	{
		return m_aContacts;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the Contacts app on open - refreshes from the Bridge only if the cache is stale.
	void RequestContacts()
	{
		Rpc(RpcAsk_RequestContacts);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestContacts()
	{
		if (m_sPhoneId == "")
			return;

		if (System.GetTickCount() - m_iContactsCacheTime < CONTACTS_CACHE_TTL_MS)
		{
			PushContacts();
			return;
		}

		ELIFE_Api api = ELIFE_Api.GetInstance();
		if (!api)
			return;

		//! Contacts/messages routes take no characterId/pin - possession is proven once at power-on.
		string request = string.Format("phones/%1/apps/contacts/entries", m_sPhoneId);

		m_ContactsFetchCallback = new ELIFE_ContactsFetchCallback();
		m_ContactsFetchCallback.SetCallback(this, "OnContactsFetched");
		api.GetElifeApi().GET(m_ContactsFetchCallback, request);
	}

	//------------------------------------------------------------------------------------------------
	void OnContactsFetched(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		ELIFE_ContactListDto list = ELIFE_ContactListDto.Cast(data);
		if (status != ELIFE_EApiStatusCode.SUCCESS || !list)
		{
			Print("ELIFE_PhoneGadgetComponent | contacts fetch failed", LogLevel.ERROR);
			return;
		}

		m_aContacts = list.items;
		m_iContactsCacheTime = System.GetTickCount();
		PushContacts();
	}

	//------------------------------------------------------------------------------------------------
	//! Owner gets the real cache, everyone else with the phone streamed gets a redacted copy
	protected void PushContacts()
	{
		ELIFE_ContactListDto real = new ELIFE_ContactListDto();
		real.items = m_aContacts;
		real.Pack();
		Rpc(RpcDo_ContactsOwner, real.AsString());

		//! Broadcast doesn't self-deliver to a listen server's own local view (same quirk as
		//! onRplName - see OnPhoneIdUpdated()) - the UI needs to self-apply the redacted
		//! copy here too when Replication.IsServer() and the local player isn't the owner.
		ELIFE_ContactListDto redacted = new ELIFE_ContactListDto();
		foreach (ELIFE_ContactDto contact : m_aContacts)
			redacted.items.Insert(contact.Redact());
		redacted.Pack();
		Rpc(RpcDo_ContactsBystanders, redacted.AsString());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ContactsOwner(string json)
	{
		ApplyContacts(json);
	}

	//------------------------------------------------------------------------------------------------
	//! No RplCondition filter here - its per-recipient behavior for RPCs (vs. RplProp, where it's
	//! documented) is unverified, and an entity with an owner may not broadcast at all under it.
	//! Excluding the owner is done explicitly below instead, which is correct regardless.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ContactsBystanders(string json)
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl && rpl.IsOwner())
			return;

		ApplyContacts(json);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyContacts(string json)
	{
		ELIFE_ContactListDto list = new ELIFE_ContactListDto();
		list.ExpandFromRAW(json);
		m_aContacts = list.items;
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

		if (state)
			SetScreenState(EPhoneScreenState.HOME);
		else
			SetScreenState(EPhoneScreenState.OFF);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the authority (server) to change state - only it may set an [RplProp] for it to replicate.
	void SetScreenState(EPhoneScreenState state)
	{
		if (m_eScreenState == state)
			return;

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
		//! Outside the outer LOD tier, skip material/pulse/RT work entirely until back in range.
		if (!m_bLocallySynced)
			return;

		if (m_bLiveScreenActive)
			SetScreenMaterial(m_sScreenLiveMaterial);
		else
			SetScreenMaterial(GetBakedScreenMaterial());

		bool poweringOn = m_ePrevScreenState == EPhoneScreenState.OFF && m_eScreenState != EPhoneScreenState.OFF;
		bool poweringOff = m_ePrevScreenState != EPhoneScreenState.OFF && m_eScreenState == EPhoneScreenState.OFF;
		m_ePrevScreenState = m_eScreenState;

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
				ELIFE_PhoneToggle.RememberActivePhone(this);
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

		ELIFE_PhoneMenu phoneMenu = ELIFE_PhoneMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.ELIFE_PhoneMenu));
		if (!phoneMenu)
			phoneMenu = ELIFE_PhoneMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.ELIFE_PhoneMenu));

		if (phoneMenu)
			phoneMenu.BindPhone(this);

		ToggleActive(true, SCR_EUseContext.FROM_ACTION);
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
	protected bool IsLocalCharacterOwner()
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
		dto.ExpandFromRAW(string.Format("{\"items\":%1}", data));
		resultData = dto;
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}
