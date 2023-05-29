class ELIFE_GatherResourceAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.EditBox, desc: "Display name of what is being gathered")]
	protected string m_GatherItemDisplayName;
	
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab what item is gathered")]
	protected ResourceName m_GatherItemPrefab;
	
    //------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Play sound		
		//Todo: Replace SCR_InventoryStorageManagerComponent RPL setup with own function or better synced sound alternative
		auto inventoryManager = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		inventoryManager.PlayItemSound(pOwnerEntity, "SOUND_PICK_UP");
		
		//Spawn item
		inventoryManager.TrySpawnPrefabToStorage(m_GatherItemPrefab);
		
		// Delete resourceObject from tree
		delete pOwnerEntity;
	}
	
    //------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = string.Format("Gather %1", m_GatherItemDisplayName);
		return true;
	}
	
    //------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
 	{
		return true;
 	}
    
    //------------------------------------------------------------------------------------------------
    override bool HasLocalEffectOnlyScript()
    {
        return true;
    } 
}