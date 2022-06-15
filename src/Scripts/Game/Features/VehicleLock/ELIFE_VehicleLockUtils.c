class ELIFE_VehicleLockUtils
{
	static bool HasPlayerKeyToCar(IEntity user, string vehicleIdentifier, ResourceName keyPrefab)
	{
		// Get inventory of user
		auto inventoryManager = SCR_InventoryStorageManagerComponent.Cast(user.FindComponent(SCR_InventoryStorageManagerComponent));
		
		if (inventoryManager.GetDepositItemCountByResource(keyPrefab) == 0)
			return false;
		
		// Get inventoryItems
		array<IEntity> inventoryItems = new array<IEntity>();
		inventoryManager.GetItems(inventoryItems);
		
		// Loop through all items
		
		foreach (IEntity item : inventoryItems)
		{
			// Check if item has InventoryItemComponent
			InventoryItemComponent itemInventoryComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (!itemInventoryComponent)
				continue;
			
			// Check if item has ItemAttributeCollection
			ItemAttributeCollection itemAttributes = itemInventoryComponent.GetAttributes();
			if (!itemAttributes)
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
