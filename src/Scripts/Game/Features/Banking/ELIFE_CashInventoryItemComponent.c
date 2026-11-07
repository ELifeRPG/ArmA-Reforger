//------------------------------------------------------------------------------------------------
class ELIFE_CashInventoryItemComponentClass : SCR_BaseInventoryItemComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Lays dropped cash flat on whatever is below, as the engine's placement leaves the thin stacks hovering.
class ELIFE_CashInventoryItemComponent : SCR_BaseInventoryItemComponent
{
	//! How far in front of the feet the cash lands, and how high above them the search for the surface starts.
	protected static const float DROP_DISTANCE = 0.5;
	protected static const float TRACE_HEIGHT = 0.5;
	protected static const float TRACE_DEPTH = 3;

	//------------------------------------------------------------------------------------------------
	override bool OverridePlacementTransform(IEntity caller, out vector computedTransform[4])
	{
		if (!caller)
			return false;

		return FindDropTransform(caller, computedTransform, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! On the surface in front of the character, facing the same way and tilted to the surface. False if there's none below.
	static bool FindDropTransform(notnull IEntity character, out vector transform[4], IEntity cash = null)
	{
		vector characterTransform[4];
		character.GetWorldTransform(characterTransform);
		vector forward = characterTransform[2];
		forward[1] = 0;
		forward.Normalize();
		if (forward == vector.Zero)
			forward = vector.Forward;

		vector start = characterTransform[3] + forward * DROP_DISTANCE + vector.Up * TRACE_HEIGHT;

		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = start - vector.Up * (TRACE_HEIGHT + TRACE_DEPTH);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		array<IEntity> exclude = {character};
		if (cash)
			exclude.Insert(cash);

		trace.ExcludeArray = exclude;

		float hit = character.GetWorld().TraceMove(trace, null);
		if (hit >= 1)
			return false;

		vector up = trace.TraceNorm;
		if (up == vector.Zero)
			up = vector.Up;

		vector facing = (forward - up * vector.Dot(forward, up)).Normalized();
		if (facing == vector.Zero)
			facing = vector.Forward;

		Math3D.DirectionAndUpMatrix(facing, up, transform);
		transform[3] = start + (trace.End - start) * hit;
		return true;
	}
}
