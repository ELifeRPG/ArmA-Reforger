//------------------------------------------------------------------------------------------------
//! The in-hand case around PhoneScreen. Shared by the menu and the peek, which use the same layout.
class ELIFE_PhoneCase
{
	protected ref array<Widget> m_aPieces = {};

	//------------------------------------------------------------------------------------------------
	//! Case is split into sprite pieces because ImageWidget can't round itself past the imageset art.
	void Init(notnull Widget root)
	{
		m_aPieces.Clear();

		ResourceName corners = ELIFE_PhoneStyle.PANEL_SET_ROUNDED_CORNERS;
		AddPiece(root, "BezelCornerTL", corners, ELIFE_PhoneStyle.BEZEL_FRAME_CORNER_TL);
		AddPiece(root, "BezelCornerTR", corners, ELIFE_PhoneStyle.BEZEL_FRAME_CORNER_TR);
		AddPiece(root, "BezelCornerBL", corners, ELIFE_PhoneStyle.BEZEL_FRAME_CORNER_BL);
		AddPiece(root, "BezelCornerBR", corners, ELIFE_PhoneStyle.BEZEL_FRAME_CORNER_BR);
		AddPiece(root, "BezelEdgeTop", corners, ELIFE_PhoneStyle.BEZEL_FRAME_EDGE_TOP);
		AddPiece(root, "BezelEdgeBottom", corners, ELIFE_PhoneStyle.BEZEL_FRAME_EDGE_BOTTOM);
		AddPiece(root, "BezelEdgeLeft", corners, ELIFE_PhoneStyle.BEZEL_FRAME_EDGE_LEFT);
		AddPiece(root, "BezelEdgeRight", corners, ELIFE_PhoneStyle.BEZEL_FRAME_EDGE_RIGHT);

		AddPiece(root, "BezelButtonLeft1");
		AddPiece(root, "BezelButtonLeft2");
		AddPiece(root, "BezelButtonRight");

		ResourceName outline = ELIFE_PhoneStyle.PANEL_SET_ROUNDED_OUTLINE;
		AddPiece(root, "ScreenRoundTL", outline, ELIFE_PhoneStyle.SCREEN_ROUND_CORNER_TL);
		AddPiece(root, "ScreenRoundTR", outline, ELIFE_PhoneStyle.SCREEN_ROUND_CORNER_TR);
		AddPiece(root, "ScreenRoundBL", outline, ELIFE_PhoneStyle.SCREEN_ROUND_CORNER_BL);
		AddPiece(root, "ScreenRoundBR", outline, ELIFE_PhoneStyle.SCREEN_ROUND_CORNER_BR);

		AddPiece(root, "ScreenEdgeTop");
		AddPiece(root, "ScreenEdgeBottom");
		AddPiece(root, "ScreenEdgeLeft");
		AddPiece(root, "ScreenEdgeRight");
	}

	//------------------------------------------------------------------------------------------------
	void Paint(ELIFE_PhoneGadgetComponent phone)
	{
		Color frameColor = ELIFE_PhoneStyle.Bezel();
		if (phone)
			frameColor = ELIFE_PhoneStyle.Mix(frameColor, phone.GetCaseColor(), 0.02);

		foreach (Widget piece : m_aPieces)
			piece.SetColor(frameColor);
	}

	//------------------------------------------------------------------------------------------------
	//! Pieces without a sprite stay plain colour rects.
	protected void AddPiece(Widget root, string widgetName, ResourceName imageSet = "", string spriteName = "")
	{
		Widget piece = root.FindAnyWidget(widgetName);
		if (!piece)
			return;

		ImageWidget img = ImageWidget.Cast(piece);
		if (img && spriteName != "")
			img.LoadImageFromSet(0, imageSet, spriteName);

		m_aPieces.Insert(piece);
	}
}
