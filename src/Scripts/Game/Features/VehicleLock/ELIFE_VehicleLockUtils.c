class ELIFE_VehicleLockUtils
{
	private	static ref ELIFE_VehicleKeyPredicate m_VehicleKeyPredicate = new ELIFE_VehicleKeyPredicate();
	
	static bool HasPlayerKeyToCar(IEntity user, string vehicleIdentifier, ResourceName keyPrefab)
	{
		// Get inventory of user
		auto inventoryManager = SCR_InventoryStorageManagerComponent.Cast(user.FindComponent(SCR_InventoryStorageManagerComponent));
		
		array<IEntity> foundItems = {};
		
		if (inventoryManager.FindItems(foundItems, m_VehicleKeyPredicate) == 0)
			return false;
		
		// Loop through all items
		
		foreach (IEntity item : foundItems)
		{
			// Check if item has InventoryItemComponent
			InventoryItemComponent itemInventoryComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (!itemInventoryComponent)
				continue;
			
			// Get VehicleKeyComponent
			ELIFE_VehicleKeyComponent keyComponent = ELIFE_VehicleKeyComponent.Cast(item.FindComponent(ELIFE_VehicleKeyComponent));
			if (!keyComponent)
				continue;
			
			return (keyComponent.m_sVehicleIdentifier == vehicleIdentifier);
		}
		
		return false;
	}
}

class ELIFE_VehicleKeyPredicate: InventorySearchPredicate
{
	void ELIFE_VehicleKeyPredicate()
	{
		QueryComponentTypes.Insert(ELIFE_VehicleKeyComponent);
	}

	override protected bool IsMatch(BaseInventoryStorageComponent storage, IEntity item, array<GenericComponent> queriedComponents, array<BaseItemAttributeData> queriedAttributes)
	{		
		return (ELIFE_VehicleKeyComponent.Cast(queriedComponents[0]).m_sVehicleIdentifier != "");
	}
};
