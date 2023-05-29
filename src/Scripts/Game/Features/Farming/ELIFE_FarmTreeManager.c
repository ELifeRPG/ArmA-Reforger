class ELIFE_FarmTreeManagerClass : ScriptComponentClass
{
	// prefab properties here
}

//------------------------------------------------------------------------------------------------
/*!
	Class generated via ScriptWizard.
*/
class ELIFE_FarmTreeManager : ScriptComponent
{
    // List of farm trees
    protected ref array<IEntity> m_FarmTrees = new array<IEntity>();
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		// server only
        if (!Replication.IsServer())
            return;
	}

	//------------------------------------------------------------------------------------------------
    bool AddFarmTree(IEntity e)
    { 		
		Print("Add FarmTree to array: " + e.GetPrefabData().GetPrefabName());
		m_FarmTrees.Insert(e);
		
		e.SetScale(0.1);
		
        return true;
    }
	
	//---------------------------------------- Post Init ----------------------------------------\\
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void ELIFE_FarmTreeManager(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_FarmTreeManager()
	{
	}
}
