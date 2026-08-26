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
	[Attribute("", UIWidgets.EditBox, "Leave empty to auto-assign a UUID when the phone spawns.")]
	protected string m_sDebugPhoneId;

	protected const string BODY_SOURCE_MATERIAL = "Phone_Body_06D0DC3A5800CC7A";
	protected const string SCREEN_SOURCE_MATERIAL = "Phone_Screen_7D200FDF0E0FC494";

	[Attribute("{32874067CF8A6EB2}Assets/Items/Equipment/Radios/Radio_ANPRC68/Data/Radio_ANPRC68_01.emat", UIWidgets.ResourceNamePicker, "Body material to re-assert on every screen swap (must match this variant's MeshObject body assignment).", "emat", category: "Phone")]
	protected ResourceName m_sBodyMaterial;

	[Attribute("{B1EFD30A850D7431}Assets/Items/Equipment/Phone/Data/Phone_Screen_Off.emat", UIWidgets.ResourceNamePicker, "Screen material while not activated.", "emat", category: "Phone")]
	protected ResourceName m_sScreenOffMaterial;

	[Attribute("{C2F0E41B961E8542}Assets/Items/Equipment/Phone/Data/Phone_Screen_Locked.emat", UIWidgets.ResourceNamePicker, "Screen material for the locked-screen state (not currently reachable from the menu).", "emat", category: "Phone")]
	protected ResourceName m_sScreenLockedMaterial;

	[Attribute("{8AD586FC9172AA4C}Assets/Items/Equipment/Phone/Data/Phone_Screen_Home.emat", UIWidgets.ResourceNamePicker, "Screen material for the home screen.", "emat", category: "Phone")]
	protected ResourceName m_sScreenHomeMaterial;

	[Attribute("{C9D0F15EBDE84136}Assets/Items/Equipment/Phone/Data/Phone_Screen_Bank.emat", UIWidgets.ResourceNamePicker, "Screen material for the bank app.", "emat", category: "Phone")]
	protected ResourceName m_sScreenBankMaterial;

	[Attribute("{93D6FB5B77090462}Assets/Items/Equipment/Phone/Data/Phone_Screen_Map.emat", UIWidgets.ResourceNamePicker, "Screen material for the map app.", "emat", category: "Phone")]
	protected ResourceName m_sScreenMapMaterial;

	[Attribute("{DD81EAC2D9C509DA}Assets/Items/Equipment/Phone/Data/Phone_Screen_Messages.emat", UIWidgets.ResourceNamePicker, "Screen material for the messages app.", "emat", category: "Phone")]
	protected ResourceName m_sScreenMessagesMaterial;

	[Attribute("{3704D5BBFA59010B}Assets/Items/Equipment/Phone/Data/Phone_Screen_Settings.emat", UIWidgets.ResourceNamePicker, "Screen material for the settings app.", "emat", category: "Phone")]
	protected ResourceName m_sScreenSettingsMaterial;

	[Attribute("2", UIWidgets.EditBox, "Intensity of the emissive pulse layered on top of the active screen material.", "0 20", category: "Phone")]
	protected float m_fScreenEmissiveIntensity;

	[Attribute("0.03 0.03 0.035 1", UIWidgets.ColorPicker, "Case color tint applied to the phone menu UI bezel (should roughly match this variant's body material).", category: "Phone")]
	protected ref Color m_CaseColor;

	[RplProp()]
	protected string m_sPhoneId;

	[RplProp(onRplName: "OnScreenStateUpdated")]
	protected EPhoneScreenState m_eScreenState;

	protected ParametricMaterialInstanceComponent m_ScreenEmissiveMaterial;
	protected float m_fScreenPulsePhase;

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

		if (!Replication.IsServer())
			return;

		if (m_sPhoneId != "")
			return;

		if (m_sDebugPhoneId != "")
			m_sPhoneId = m_sDebugPhoneId;
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
	//! Asks the authority (server) to change the state - see RpcAsk_SetScreenState. The authority
	//! is the only side allowed to set an [RplProp] value for it to actually replicate;
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
	//! Fired by Replication on proxies (not the authority) whenever m_eScreenState updates -
	//! including the initial sync for players who join after the state was already set.
	protected void OnScreenStateUpdated()
	{
		ApplyScreenState();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyScreenState()
	{
		switch (m_eScreenState)
		{
			case EPhoneScreenState.LOCKED:
				SetScreenMaterial(m_sScreenLockedMaterial);
				break;
			case EPhoneScreenState.HOME:
				SetScreenMaterial(m_sScreenHomeMaterial);
				break;
			case EPhoneScreenState.BANK:
				SetScreenMaterial(m_sScreenBankMaterial);
				break;
			case EPhoneScreenState.MAP:
				SetScreenMaterial(m_sScreenMapMaterial);
				break;
			case EPhoneScreenState.MESSAGES:
				SetScreenMaterial(m_sScreenMessagesMaterial);
				break;
			case EPhoneScreenState.SETTINGS:
				SetScreenMaterial(m_sScreenSettingsMaterial);
				break;
			default:
				SetScreenMaterial(m_sScreenOffMaterial);
				break;
		}

		if (m_eScreenState == EPhoneScreenState.OFF)
			StopScreenPulse();
		else
			StartScreenPulse();
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
			SetScreenMaterial(m_sScreenOffMaterial);
			StopScreenPulse();
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
