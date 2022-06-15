class ELIFE_Utils
{
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", IEntity parent = null)
	{
		EntitySpawnParams spawnParams();
		
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;
		
		if (parent != null)
		{
			spawnParams.Parent = parent;
		}
		
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
	}
}
