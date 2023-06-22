class ELIFE_GatherResourceAction : ScriptedUserAction
{
	string m_GatherItemDisplayName;
	ResourceName m_GatherItemPrefab;
	ResourceName m_GatherItemPreview;
	float m_GatherTimeout;
	
	vector m_ContextPosition[4];
	IEntity m_GatherItem;
	
	protected float m_RestockTimestamp;
	protected bool m_PrefabVisible;
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{	

	}
	
    //------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Play sound
		//Todo: Replace SCR_InventoryStorageManagerComponent RPL setup with own function or better synced sound alternative
		auto inventoryManager = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		inventoryManager.PlayItemSound(pOwnerEntity, "SOUND_PICK_UP");
		
		//Spawn item
		inventoryManager.TrySpawnPrefabToStorage(m_GatherItemPrefab);
		
		m_RestockTimestamp = Replication.Time() + (m_GatherTimeout * 1000);
		
		if (m_GatherItem) {				
			m_GatherItem.ClearFlags(EntityFlags.VISIBLE);
			m_PrefabVisible = false;
		}
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
		if(m_RestockTimestamp > Replication.Time())
		{
			int secondsLeft = (m_RestockTimestamp - Replication.Time()) / 1000;
			if (secondsLeft < 59) {
				SetCannotPerformReason(string.Format("Please wait %1 seconds", secondsLeft + 1));
			}
			else {
				SetCannotPerformReason(string.Format("Please wait %1 minutes", (secondsLeft + 1) / 60));
			}
			return false;
		}
		
		auto inventoryManager = SCR_InventoryStorageManagerComponent.Cast( user.FindComponent( SCR_InventoryStorageManagerComponent ) );
		
		if (!m_PrefabVisible && m_GatherItem) {
			m_PrefabVisible = true;
			m_GatherItem.SetFlags(EntityFlags.VISIBLE);
		}
		
		return true;
 	}
    
    //------------------------------------------------------------------------------------------------
    override bool HasLocalEffectOnlyScript()
    {
        return true;
    } 
	
	//------------------------------------------------------------------------------------------------
	void ShowGatherPreview() {
		if (!m_PrefabVisible) {
			EntitySpawnParams params = EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = m_ContextPosition[3];
			
			Resource res = Resource.Load(m_GatherItemPreview);
			m_GatherItem = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			
			m_PrefabVisible = true;
		}
	}
}