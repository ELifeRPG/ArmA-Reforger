class ELIFE_FarmTreeComponentClass: ScriptComponentClass
{
};

class ELIFE_FarmTreeComponent: ScriptComponent
{
	[Attribute("", UIWidgets.Object, category: "Farm Tree Resource")]
	protected ref array<ref PointInfo> m_aResourceNodePositions;
	
	[Attribute("", UIWidgets.EditBox, desc: "Display name of what is being gathered")]
	protected string m_GatherItemDisplayName;
	
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab what item is gathered")]
	protected ResourceName m_GatherItemPrefab;
	
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab what item is visible")]
	protected ResourceName m_GatherItemPreview;
	
	[Attribute("", UIWidgets.EditBox, desc: "Amount of time needed until you can regather something")]
	protected float m_GatherTimeout;
	
	protected ref array<ref PointInfo> resourceObjects = {};
		
	ref array<ref PointInfo> GetResourceNodePositions()
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
		
		auto actionsManagerComponent = ActionsManagerComponent.Cast( owner.FindComponent(ActionsManagerComponent) );
		array<UserActionContext> contextList = {};
		actionsManagerComponent.GetContextList(contextList);
		
		foreach (UserActionContext context: contextList) {
			array<BaseUserAction> actionList = {};
			context.GetActionsList(actionList);
			
			foreach (BaseUserAction actionBase: actionList) {
				ELIFE_GatherResourceAction action = ELIFE_GatherResourceAction.Cast(actionBase);
				
				action.m_GatherItemDisplayName = m_GatherItemDisplayName;
				action.m_GatherItemPrefab = m_GatherItemPrefab;
				action.m_GatherItemPreview = m_GatherItemPreview;
				action.m_GatherTimeout = m_GatherTimeout;
				
				vector pos[4];
				context.GetTransformationModel(pos);
				pos[3] = owner.CoordToParent(pos[3]);
				action.m_ContextPosition = pos;
				action.ShowGatherPreview();
			}
			
			//actionsManagerComponent.
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
		foreach (PointInfo point : m_aResourceNodePositions)
		{
			vector localMat[4];
			point.GetTransform(localMat);
			ref Shape spawnPointShape = Shape.CreateSphere(COLOR_GREEN, ShapeFlags.ONCE | ShapeFlags.NOOUTLINE, GetOwner().CoordToParent(localMat[3]), 0.1);
		}
		
		ActionsManagerComponent actionsManagerComponent = ActionsManagerComponent.Cast(owner.FindComponent(ActionsManagerComponent));
		
		array<UserActionContext> contextList = {};
		actionsManagerComponent.GetContextList(contextList);
		
		foreach (UserActionContext context: contextList) {
			ref Shape spawnPointShape = Shape.CreateSphere(COLOR_BLUE, ShapeFlags.ONCE | ShapeFlags.NOOUTLINE, GetOwner().CoordToParent(context.GetOrigin()), 0.1);
		}
	}
	
	//---------------------------------------- Post Init ----------------------------------------\\
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}
};
