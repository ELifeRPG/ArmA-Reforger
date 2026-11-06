class ELIFE_VehicleKeyComponentClass : GameComponentClass
{
	// prefab properties here
}

//------------------------------------------------------------------------------------------------
/*!
	Class generated via ScriptWizard.
*/
class ELIFE_VehicleKeyComponent : GameComponent
{
	[Attribute("")]
	string m_sDebugIdentifier;
	
	[RplProp()]
	string m_sVehicleIdentifier;

	//------------------------------------------------------------------------------------------------
	void ELIFE_VehicleKeyComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		if(m_sDebugIdentifier != "")
		{
			m_sVehicleIdentifier = m_sDebugIdentifier;
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_VehicleKeyComponent()
	{
	}

}
