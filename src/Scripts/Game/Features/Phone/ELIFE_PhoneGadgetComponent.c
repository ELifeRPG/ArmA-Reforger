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
//! Each spawned phone gets a unique UUID (backend key). Items are not stackable.
class ELIFE_PhoneGadgetComponent : SCR_GadgetComponent
{
	[Attribute("", UIWidgets.EditBox, "Fixed device ID assigned at manufacture. Leave empty to auto-assign a UUID when the phone spawns.")]
	protected string m_sManufactureDeviceId;

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

	[RplProp()]
	protected string m_sPhoneId;

	[RplProp(onRplName: "OnScreenStateUpdated")]
	protected EPhoneScreenState m_eScreenState;

	//! Generic in-app navigation sync channel - see ELIFE_PhoneAppBase.GetSubState()/ApplySubState().
	[RplProp(onRplName: "OnScreenSubStateUpdated")]
	protected string m_sScreenSubState;

	protected ParametricMaterialInstanceComponent m_ScreenEmissiveMaterial;
	protected float m_fScreenPulsePhase;
	protected ELIFE_PhoneScreenRenderComponent m_ScreenRenderComponent;
	protected bool m_bLiveScreenActive;
	protected ResourceName m_sAppliedScreenMaterial;

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

		if (!Replication.IsServer())
			return;

		if (m_sPhoneId != "")
			return;

		if (m_sManufactureDeviceId != "")
			m_sPhoneId = m_sManufactureDeviceId;
		else
			m_sPhoneId = UUID.GenV4();

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	string GetPhoneId()
	{
		return m_sPhoneId;
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

		if (m_eScreenState == EPhoneScreenState.OFF)
			StopScreenPulse();
		else
			StartScreenPulse();

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
			m_eScreenState = EPhoneScreenState.OFF;
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
