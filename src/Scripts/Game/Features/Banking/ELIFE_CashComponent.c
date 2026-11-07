//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Banking", description: "Stack of cash carrying a whole-tolar amount")]
class ELIFE_CashComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Server-authoritative amount. No RPCs here: clients don't reliably own items, so requests go through ELIFE_CashCharacterComponent.
class ELIFE_CashComponent : ScriptComponent
{
	//! One realistic bundle: 100 notes of Ŧ100, about 1 cm high.
	static const int MAX_AMOUNT = 10000;

	protected static const ResourceName CASH_PREFAB = "{4B8E888EFE59EE44}Prefabs/Items/Cash/Cash.et";

	//! Five looks by amount: a few Ŧ20, Ŧ50 or Ŧ100 notes, then a stack, then a banded brick of Ŧ100 notes.
	protected static const int NOTE_50_FROM = 100;
	protected static const int NOTE_100_FROM = 500;
	protected static const int MEDIUM_FROM = 1000;
	protected static const int LARGE_FROM = 5000;

	//! The models' note material, swapped for the smaller notes. The Ŧ100 is the models' own.
	protected static const string NOTE_MATERIAL_SLOT = "Cash_Note";
	protected static const ResourceName NOTE_20_MATERIAL = "{FC681CB84C4EB339}Assets/Items/Cash/Data/Cash_Note_20.emat";
	protected static const ResourceName NOTE_50_MATERIAL = "{6CED029AAD3513AA}Assets/Items/Cash/Data/Cash_Note_50.emat";

	//! What inventory icons show per look. The icon is drawn from a prefab, which knows no amount.
	protected static const ResourceName PREVIEW_SMALL_20 = "{B7DC46441B70A23F}Prefabs/Items/Cash/Cash_Preview_Small20.et";
	protected static const ResourceName PREVIEW_SMALL_50 = "{FA378EB4C982469F}Prefabs/Items/Cash/Cash_Preview_Small50.et";
	protected static const ResourceName PREVIEW_SMALL = "{587BDC078ECAE116}Prefabs/Items/Cash/Cash_Preview_Small.et";
	protected static const ResourceName PREVIEW_MEDIUM = "{5F35427F16B5FC97}Prefabs/Items/Cash/Cash_Preview_Medium.et";
	protected static const ResourceName PREVIEW_LARGE = "{68B9C7C7AC98843E}Prefabs/Items/Cash/Cash_Preview_Large.et";

	[Attribute("{9787463F40E1971B}Assets/Items/Cash/Cash_Small.xob", UIWidgets.ResourceNamePicker, "Model below the medium amount.", "xob")]
	protected ResourceName m_sSmallModel;

	[Attribute("{1ED96F215A5D455E}Assets/Items/Cash/Cash_Medium.xob", UIWidgets.ResourceNamePicker, "Model from the medium amount.", "xob")]
	protected ResourceName m_sMediumModel;

	[Attribute("{65F94B076D9BEC29}Assets/Items/Cash/Cash_Large.xob", UIWidgets.ResourceNamePicker, "Model from the large amount up to the cap.", "xob")]
	protected ResourceName m_sLargeModel;

	//! Model plus note material last applied, so an amount change that keeps the look does nothing.
	protected string m_sShownLook;

	//! A new ground stack is listed by hand for at most VICINITY_POLL_MS * VICINITY_POLL_COUNT.
	protected static const int VICINITY_POLL_MS = 200;
	protected static const int VICINITY_POLL_COUNT = 25;
	protected int m_iVicinityPollsLeft;

	//! Weak references, so deleted stacks turn null.
	protected static ref array<ELIFE_CashComponent> s_aNewOnGround = {};

	[Attribute("0", UIWidgets.EditBox, "Tolars the stack starts with. Designer/DevWorld use only, the shipped prefab stays at 0.", "0 10000")]
	protected int m_iInitialAmount;

	[RplProp(onRplName: "OnAmountUpdated")]
	protected int m_iAmount;

	protected ref ScriptInvokerVoid m_OnAmountChanged;

	//------------------------------------------------------------------------------------------------
	void ELIFE_CashComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_iAmount = m_iInitialAmount;
	}

	//------------------------------------------------------------------------------------------------
	//! A frame later, so a joining client has the replicated amount before the model is chosen.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(ShowModelForAmount, 0);
		GetGame().GetCallqueue().CallLater(ShowInVicinity, 0);

		InventoryItemComponent item = InventoryItemComponent.Cast(owner.FindComponent(InventoryItemComponent));
		if (item)
			item.m_OnParentSlotChangedInvoker.Insert(OnParentSlotChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! The engine's vicinity scan finds new ground items late, so the open vicinity list adds them by hand until then.
	protected void ShowInVicinity()
	{
		if (!SCR_InventoryMenuUI.GetInventoryMenu() || !IsLyingInWorld())
			return;

		s_aNewOnGround.Insert(this);
		RefreshVicinity();

		m_iVicinityPollsLeft = VICINITY_POLL_COUNT;
		GetGame().GetCallqueue().CallLater(WatchVicinity, VICINITY_POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listing the stack by hand once the engine has found it, it's gone from the ground, or the wait is over.
	protected void WatchVicinity()
	{
		m_iVicinityPollsLeft--;
		if (m_iVicinityPollsLeft > 0 && IsLyingInWorld() && !IsInLocalVicinity())
			return;

		GetGame().GetCallqueue().Remove(WatchVicinity);
		s_aNewOnGround.RemoveItem(this);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInLocalVicinity()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return false;

		CharacterVicinityComponent vicinity = CharacterVicinityComponent.Cast(player.FindComponent(CharacterVicinityComponent));
		if (!vicinity)
			return false;

		array<IEntity> items = {};
		vicinity.GetAvailableItems(items);
		return items.Contains(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the new stacks on the ground within the radius that the engine's vicinity hasn't listed yet.
	static void AddNewOnGround(notnull IEntity character, float radius, notnull array<IEntity> items)
	{
		foreach (ELIFE_CashComponent cash : s_aNewOnGround)
		{
			if (!cash || !cash.IsLyingInWorld())
				continue;

			IEntity owner = cash.GetOwner();
			if (!items.Contains(owner) && vector.Distance(owner.GetOrigin(), character.GetOrigin()) <= radius)
				items.Insert(owner);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshVicinity()
	{
		if (!IsLyingInWorld())
			return;

		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		if (menu)
			menu.RefreshLootUIListener();
	}

	//------------------------------------------------------------------------------------------------
	//! In the game world and not in any storage; inventory icon copies live in their own world.
	protected bool IsLyingInWorld()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.GetWorld() != GetGame().GetWorld())
			return false;

		InventoryItemComponent item = InventoryItemComponent.Cast(owner.FindComponent(InventoryItemComponent));
		return item && !item.GetParentSlot();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);

		if (GetGame().InPlayMode())
			RefreshVicinity();
	}

	//------------------------------------------------------------------------------------------------
	//! A stack resized in a storage keeps the collider it had there, so it floats once dropped.
	protected void OnParentSlotChanged(InventoryStorageSlot oldSlot, InventoryStorageSlot newSlot)
	{
		if (!newSlot)
			GetGame().GetCallqueue().CallLater(UpdateCollider, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCollider()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		Physics physics = owner.GetPhysics();
		if (physics)
			physics.UpdateGeometries();
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}

	//------------------------------------------------------------------------------------------------
	void SetAmount(int amount)
	{
		if (!Replication.IsServer())
			return;

		m_iAmount = amount;

		//! onRplName doesn't fire on the authority itself.
		OnAmountUpdated();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Fires on every machine once the stack shows its new amount.
	ScriptInvokerVoid GetOnAmountChanged()
	{
		if (!m_OnAmountChanged)
			m_OnAmountChanged = new ScriptInvokerVoid();

		return m_OnAmountChanged;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAmountUpdated()
	{
		ShowModelForAmount();

		if (m_OnAmountChanged)
			m_OnAmountChanged.Invoke();

		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		if (menu)
			menu.ELIFE_RefreshItemInfo(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetPreviewPrefab()
	{
		if (m_iAmount >= LARGE_FROM)
			return PREVIEW_LARGE;

		if (m_iAmount >= MEDIUM_FROM)
			return PREVIEW_MEDIUM;

		if (m_iAmount >= NOTE_100_FROM)
			return PREVIEW_SMALL;

		if (m_iAmount >= NOTE_50_FROM)
			return PREVIEW_SMALL_50;

		return PREVIEW_SMALL_20;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on every machine, as the amount is replicated. Only swaps when the size actually changes.
	//! Inventory icon copies live in their own world and keep their prefab's model.
	protected void ShowModelForAmount()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.GetWorld() != GetGame().GetWorld())
			return;

		ResourceName model;
		ResourceName noteMaterial;
		GetLook(model, noteMaterial);

		string look = model + noteMaterial;
		if (look == m_sShownLook || !ApplyLook(owner, model, noteMaterial))
			return;

		m_sShownLook = look;

		//! The collider follows the model, but only for a stack that's simulated, not one inside a storage.
		Physics physics = owner.GetPhysics();
		if (physics && physics.GetSimulationState() != SimulationState.NONE)
			physics.UpdateGeometries();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives another entity this stack's current look, e.g. the inspection preview's copy, which has no amount.
	bool ShowLookOn(notnull IEntity target)
	{
		ResourceName model;
		ResourceName noteMaterial;
		GetLook(model, noteMaterial);
		return ApplyLook(target, model, noteMaterial);
	}

	//------------------------------------------------------------------------------------------------
	protected void GetLook(out ResourceName model, out ResourceName noteMaterial)
	{
		model = m_sSmallModel;
		noteMaterial = ResourceName.Empty;
		if (m_iAmount >= LARGE_FROM)
			model = m_sLargeModel;
		else if (m_iAmount >= MEDIUM_FROM)
			model = m_sMediumModel;
		else if (m_iAmount < NOTE_50_FROM)
			noteMaterial = NOTE_20_MATERIAL;
		else if (m_iAmount < NOTE_100_FROM)
			noteMaterial = NOTE_50_MATERIAL;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyLook(notnull IEntity target, ResourceName model, ResourceName noteMaterial)
	{
		Resource resource = Resource.Load(model);
		if (!resource || !resource.IsValid())
			return false;

		VObject asset = resource.GetResource().ToVObject();
		if (!asset)
			return false;

		string remap;
		if (noteMaterial != ResourceName.Empty)
			remap = string.Format("$remap '%1' '%2';", NOTE_MATERIAL_SLOT, noteMaterial);

		target.SetObject(asset, remap);
		target.Update();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_CashComponent()
	{
		if (s_aNewOnGround)
			s_aNewOnGround.RemoveItem(this);

		if (!GetGame())
			return;

		ScriptCallQueue callqueue = GetGame().GetCallqueue();
		callqueue.Remove(ShowModelForAmount);
		callqueue.Remove(UpdateCollider);
		callqueue.Remove(WatchVicinity);
		callqueue.Remove(ShowInVicinity);
	}

	//------------------------------------------------------------------------------------------------
	//! The fullest carried stack that can take the whole amount, so money gathers on one stack.
	static ELIFE_CashComponent FindStackWithRoom(notnull IEntity character, int amount)
	{
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);

		ELIFE_CashComponent best;
		foreach (IEntity item : items)
		{
			ELIFE_CashComponent cash = ELIFE_CashComponent.Cast(item.FindComponent(ELIFE_CashComponent));
			//! Compared as a difference so the check itself can't overflow.
			if (!cash || amount > MAX_AMOUNT - cash.GetAmount())
				continue;

			if (!best || cash.GetAmount() > best.GetAmount())
				best = cash;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Tops up carried stacks, fullest first, and returns how much of the amount they took.
	static int AddToCarriedStacks(notnull IEntity character, int amount)
	{
		if (!Replication.IsServer())
			return 0;

		int added;
		while (added < amount)
		{
			ELIFE_CashComponent stack = FindStackWithRoom(character, 1);
			if (!stack)
				break;

			int take = MAX_AMOUNT - stack.GetAmount();
			if (take > amount - added)
				take = amount - added;

			stack.SetAmount(stack.GetAmount() + take);
			added += take;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Puts a new stack into the given storage, else anywhere in the inventory, else on the ground, which onGround reports.
	static IEntity SpawnCash(int amount, notnull IEntity character, out bool onGround, BaseInventoryStorageComponent storage = null, bool toGround = false)
	{
		onGround = false;

		if (!Replication.IsServer() || amount <= 0 || amount > MAX_AMOUNT)
			return null;

		Resource resource = Resource.Load(CASH_PREFAB);
		if (!resource.IsValid())
			return null;

		//! Spawned straight onto the ground in case it ends up there, as the engine's placement leaves it hovering.
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		bool placed = ELIFE_CashInventoryItemComponent.FindDropTransform(character, params.Transform);
		if (!placed)
			character.GetWorldTransform(params.Transform);

		IEntity cash = GetGame().SpawnEntityPrefab(resource, character.GetWorld(), params);
		if (!cash)
			return null;

		ELIFE_CashComponent cashComponent = ELIFE_CashComponent.Cast(cash.FindComponent(ELIFE_CashComponent));
		if (!cashComponent)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(cash);
			return null;
		}

		//! Same frame as the spawn, so the first replicated state already carries the amount.
		cashComponent.SetAmount(amount);

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (inventory && !toGround)
		{
			//! The storage may hold sub-storages (vest pouches), so let the inventory pick one.
			BaseInventoryStorageComponent target;
			if (storage)
				target = inventory.FindStorageForInsert(cash, storage, EStoragePurpose.PURPOSE_ANY);

			if (target && inventory.TryInsertItemInStorage(cash, target))
				return cash;

			if (inventory.TryInsertItem(cash))
				return cash;
		}

		InventoryItemComponent item = InventoryItemComponent.Cast(cash.FindComponent(InventoryItemComponent));
		if (item && !placed)
			item.PlaceOnGround(character);

		onGround = true;
		return cash;
	}
}
