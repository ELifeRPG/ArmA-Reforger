class ELIFE_PrefabSpawn : SCR_BasePrefabSpawner
{
	[Attribute("10", UIWidgets.EditBox, desc: "Delay for prefab to spawn upon destruction (s)",  params: "0 10000")]
	protected float respawnDelay;
	[Attribute("0", UIWidgets.EditBox, desc: "Delay for prefab vehicle to be spawned after (s)",  params: "0 10000")]
	protected float firstSpawn;
	protected float timer = 10;
	
	
	protected IEntity spawnedPrefab;
		
	protected override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		timer = firstSpawn;
	}
	
	protected override bool CanSpawn()
	{
		return !spawnedPrefab && timer <= 0;
	}
	
	protected override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		
		if (!spawnedPrefab)
			timer -= timeSlice;
		
	}
	
	protected override void OnSpawn(IEntity newEnt)
	{
		spawnedPrefab = newEnt;
		EventHandlerManagerComponent ehmc = EventHandlerManagerComponent.Cast (spawnedPrefab.FindComponent(EventHandlerManagerComponent));
		if (ehmc)
		{
			ehmc.RegisterScriptHandler("OnDestroyed", this, OnDestroyed);
		}
		timer = respawnDelay;
	}
	
	void OnDestroyed(IEntity newEnt)
	{
		spawnedPrefab = null;
	}
}
class ELIFE_PrefabSpawnClass : SCR_BasePrefabSpawnerClass
{
}