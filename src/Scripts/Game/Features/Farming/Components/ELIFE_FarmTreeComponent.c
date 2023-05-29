class ELIFE_FarmTreeComponentClass: ScriptComponentClass
{
};

class ELIFE_FarmTreeComponent: ScriptComponent
{
	[Attribute("", UIWidgets.Object, category: "Farm Tree Resource")]
	//ref array<ref PointInfo> m_aResourceNodePositions; // until fixed ....
	protected ref array<vector> m_aResourceNodePositions;
	
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab what item is gathered")]
	protected ResourceName m_GatherItemPrefab;
	
	protected ref array<IEntity> resourceObjects = {};
		
	ref array<vector> GetResourceNodePositions()
	{
		return m_aResourceNodePositions;
	}
	
	//======================================== INIT ========================================\\
	//---------------------------------------- Server Init ----------------------------------------\\
	protected void InitOnServer(IEntity owner)
	{
		// Always verify the pointer. The object could be deleted by the time it gets here.
		if (owner == null)
			return;
		
		//m_FarmTreeManager.AddFarmTree(owner);
		Print("Array-Count: " + m_aResourceNodePositions.Count());
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Resource res = Resource.Load(m_GatherItemPrefab);
		
		foreach (vector pi : m_aResourceNodePositions)
		{
			vector position = GetOwner().CoordToParent(pi);
			params.Transform[3] = position;	
			if (Math.RandomInt(0, 4) > 0) {
				GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			}
		}
	}
	
	//---------------------------------------- On Init ----------------------------------------\\
	override void EOnInit(IEntity owner)
	{				
		if (SCR_Global.IsEditMode(owner))
			return;
		
		RplComponent rplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		
		if (rplComponent.Role() != RplRole.Authority)
			return;
		
		GetGame().GetCallqueue().CallLater(InitOnServer, 1, false, owner);
	}
	
	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		foreach (vector point : m_aResourceNodePositions)
		{			
			ref Shape spawnPointShape = Shape.CreateSphere(COLOR_GREEN, ShapeFlags.ONCE | ShapeFlags.NOOUTLINE, GetOwner().CoordToParent(point), 0.1);
		}
	}
	
	//---------------------------------------- Post Init ----------------------------------------\\
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}
};
