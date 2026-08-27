//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Signs", description: "Live-renders 2D content onto a custom sign's decal material slot via a render-target.")]
class ELIFE_SignContentRenderComponentClass : ScriptComponentClass
{
	[Attribute("SIGN NAME", UIWidgets.EditBox, "Custom text displayed on this sign")]
	protected string m_sText;

	//------------------------------------------------------------------------------------------------
	string GetText()
	{
		return m_sText;
	}
}

//------------------------------------------------------------------------------------------------
//! Init is deferred 250ms past OnPostInit and gated on actual play mode - both confirmed
//! necessary in practice (not just per API docs) by a public reference implementation's own
//! diagnostic self-test component, which hit and documented the same "binds but shows nothing"
//! failure this component originally had: workspace/entity hierarchy aren't reliably settled
//! yet at OnPostInit time, and SetRenderTarget silently does nothing outside actual play mode
//! (e.g. in the World Editor's static viewport).
class ELIFE_SignContentRenderComponent : ScriptComponent
{
	protected const ResourceName CONTENT_LAYOUT = "{F0DA0E490E0B36F7}UI/layouts/Menus/Sign/SignContent.layout";
	protected const int INIT_DELAY_MS = 250;

	protected Widget m_wRoot;
	protected RTTextureWidget m_RT;
	protected TextWidget m_wLabel;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(InitRT, INIT_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void InitRT()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wRoot = workspace.CreateWidgets(CONTENT_LAYOUT);
		if (!m_wRoot)
			return;

		m_RT = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("ContentRT"));
		if (!m_RT)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return;
		}

		m_wLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("ContentLabel"));

		IEntity owner = GetOwner();
		if (!owner)
			return;

		ELIFE_SignContentRenderComponentClass data = ELIFE_SignContentRenderComponentClass.Cast(GetComponentData(owner));
		if (m_wLabel && data)
			m_wLabel.SetText(data.GetText());

		m_RT.SetRenderTarget(owner);
		m_RT.SetEnabled(true);
		m_wRoot.Update();
		m_RT.Update();
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_SignContentRenderComponent()
	{
		if (m_RT)
			m_RT.RemoveRenderTarget(GetOwner());

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}
}
