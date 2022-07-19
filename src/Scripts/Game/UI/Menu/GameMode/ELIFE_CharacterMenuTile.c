class ELIFE_CharacterMenuTile : SCR_DeployMenuTile
{
	CharacterDto m_Character;
	protected ImageWidget m_wCharacterBackground;

	//------------------------------------------------------------------------------------------------
	static ELIFE_CharacterMenuTile InitializeTile(SCR_DeployMenuTileSelection parent, CharacterDto character)
	{
		Widget tile = GetGame().GetWorkspace().CreateWidgets(parent.GetTileResource());
		ELIFE_CharacterMenuTile handler = ELIFE_CharacterMenuTile.Cast(tile.FindHandler(ELIFE_CharacterMenuTile));
		SCR_GalleryComponent gallery_handler = SCR_GalleryComponent.Cast(parent.GetTileContainer().GetHandler(0));
		if (!handler)
			return null;

		handler.SetParent(parent);
		handler.SetText(string.Format("%1 %2", character.firstName, character.lastName));
		handler.m_Character = character;
		
		gallery_handler.AddItem(tile);

		return handler;
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wCharacterBackground = ImageWidget.Cast(m_wRoot.FindAnyWidget("CharacterBckg"));
	}

	//------------------------------------------------------------------------------------------------
	protected void FocusTile(Widget w)
	{
		OnMouseEnter(m_wRoot, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void UnfocusTile(Widget w)
	{
		OnMouseLeave(m_wRoot, null, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	void SetCharacterBackgroundColor(Color color)
	{
		m_wCharacterBackground.SetColor(color);
	}
};