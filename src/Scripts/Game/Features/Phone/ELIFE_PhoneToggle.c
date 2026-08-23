//------------------------------------------------------------------------------------------------
//! Tap-toggle for the handheld phone (default N). Identity is ELIFE_PhoneGadgetComponent.
//! Several phones may sit in inventory. N toggles the remembered equipped phone (in hand or last used).
//! Equip a different phone from inventory to switch. A single owned phone is treated as equipped.
//! Holster returns the item to pocket storage — do not use EGadgetMode.IN_SLOT (that is vest/belt gadget mounts).
class ELIFE_PhoneToggle
{
	protected static bool s_bRegistered;
	protected static InputManager s_RegisteredInput;

	//------------------------------------------------------------------------------------------------
	static void Register()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		if (s_bRegistered && s_RegisteredInput == inputManager)
			return;

		inputManager.RemoveActionListener("ELIFE_PhoneToggle", EActionTrigger.DOWN, OnToggle);
		inputManager.AddActionListener("ELIFE_PhoneToggle", EActionTrigger.DOWN, OnToggle);
		s_RegisteredInput = inputManager;
		s_bRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	static void OnToggle()
	{
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (!character)
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager && menuManager.FindMenuByPreset(ChimeraMenuPreset.ELIFE_PhoneMenu))
		{
			HolsterOwnedPhone(character);
			return;
		}

		ELIFE_PhoneGadgetComponent phone = FindOwnedPhone(character);
		if (!phone)
		{
			array<ELIFE_PhoneGadgetComponent> carriedPhones = {};
			CollectOwnedPhones(character, carriedPhones);
			if (carriedPhones.Count() > 0)
				SCR_HintManagerComponent.ShowCustomHint("#ELIFE-Hint_Phone_Unequipped", "#ELIFE-Item_Phone_Name", 3.0);
			else
				SCR_HintManagerComponent.ShowCustomHint("#ELIFE-Hint_Phone_Missing", "#ELIFE-Item_Phone_Name", 3.0);
			return;
		}

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgetManager && phone.GetMode() != EGadgetMode.IN_HAND)
			gadgetManager.SetGadgetMode(phone.GetOwner(), EGadgetMode.IN_HAND);

		phone.OpenPhoneMenu();
	}

	//------------------------------------------------------------------------------------------------
	static void HolsterOwnedPhone(IEntity character)
	{
		if (!character)
			character = SCR_PlayerController.GetLocalControlledEntity();

		ELIFE_PhoneGadgetComponent phone = GetHeldPhone(character);
		if (!phone)
			phone = FindOwnedPhone(character);

		if (phone)
		{
			phone.Holster();
			return;
		}

		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.ELIFE_PhoneMenu);
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_PhoneGadgetComponent GetHeldPhone(IEntity character)
	{
		if (!character)
			return null;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (!gadgetManager)
			return null;

		return ELIFE_PhoneGadgetComponent.Cast(gadgetManager.GetHeldGadgetComponent());
	}

	//------------------------------------------------------------------------------------------------
	//! Equipped phone: in hand, last used still owned, or the only phone carried. Not IN_SLOT (vest/belt mounts).
	static ELIFE_PhoneGadgetComponent FindOwnedPhone(IEntity character)
	{
		if (!character)
			return null;

		ELIFE_PhoneGadgetComponent heldPhone = GetHeldPhone(character);
		if (heldPhone)
			return heldPhone;

		array<ELIFE_PhoneGadgetComponent> phones = {};
		CollectOwnedPhones(character, phones);
		if (phones.Count() == 0)
			return null;

		string activeId = GetRememberedPhoneId();
		if (activeId != "")
		{
			foreach (ELIFE_PhoneGadgetComponent phone : phones)
			{
				if (phone && phone.GetPhoneId() == activeId)
					return phone;
			}
		}

		if (phones.Count() == 1)
			return phones[0];

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static void CollectOwnedPhones(IEntity character, out notnull array<ELIFE_PhoneGadgetComponent> phones)
	{
		phones.Clear();
		if (!character)
			return;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgetManager)
		{
			array<SCR_GadgetComponent> gadgets = gadgetManager.GetGadgetsByType(EGadgetType.SPECIALIST_ITEM);
			if (gadgets)
			{
				foreach (SCR_GadgetComponent gadget : gadgets)
				{
					ELIFE_PhoneGadgetComponent phone = ELIFE_PhoneGadgetComponent.Cast(gadget);
					if (phone)
						phones.Insert(phone);
				}
			}
		}

		if (phones.Count() > 0)
			return;

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory)
			return;

		array<IEntity> items = {};
		inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);
		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			ELIFE_PhoneGadgetComponent phone = ELIFE_PhoneGadgetComponent.Cast(item.FindComponent(ELIFE_PhoneGadgetComponent));
			if (phone)
				phones.Insert(phone);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void RememberActivePhone(ELIFE_PhoneGadgetComponent phone)
	{
		if (!phone)
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
			playerController.ELIFE_SetActivePhoneId(phone.GetPhoneId());
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetRememberedPhoneId()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController)
			return "";

		return playerController.ELIFE_GetActivePhoneId();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected string m_sELIFEActivePhoneId;

	//------------------------------------------------------------------------------------------------
	void ELIFE_SetActivePhoneId(string phoneId)
	{
		m_sELIFEActivePhoneId = phoneId;
	}

	//------------------------------------------------------------------------------------------------
	string ELIFE_GetActivePhoneId()
	{
		return m_sELIFEActivePhoneId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		if (m_bIsLocalPlayerController)
			ELIFE_PhoneToggle.Register();
	}
}
