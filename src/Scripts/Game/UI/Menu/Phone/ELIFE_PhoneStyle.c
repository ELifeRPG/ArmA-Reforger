//------------------------------------------------------------------------------------------------
//! Design tokens for the phone UI: colours, type, spacing, radii, motion and ZOrder. Call sites use
//! these rather than literals. Neutrals are tinted toward a cool slate; app accents share lightness.
class ELIFE_PhoneStyle
{
	//------------------------------------------------------------------------------------------------
	// Depth - ground, content, page chrome, sheets, phone chrome, banners, alerts
	//------------------------------------------------------------------------------------------------
	static const int ZORDER_GROUND = 0;
	static const int ZORDER_CONTENT = 10;

	//! Nav bar controls; a sheet covers them since they act on the page underneath.
	static const int ZORDER_CHROME = 20;

	//! Modal over a page, e.g. the notification hub.
	static const int ZORDER_SHEET = 30;

	//! Status bar and home pill - above sheets so there's always a way out.
	static const int ZORDER_SYSTEM = 40;

	static const int ZORDER_BANNER = 50;

	//! ScreenOff and OfflineScreen.
	static const int ZORDER_ALERT = 60;

	//------------------------------------------------------------------------------------------------
	//! Flip if the wallpaper gradient texture runs bright-at-bottom.
	static const bool GRADIENT_FLIPPED = false;

	//! False swaps every glass stack for one flat panel of the same lightness.
	static const bool GLASS_ENABLED = true;
	static const float GLASS_SPECULAR_HEIGHT = 4;

	//! Alphas are fixed per look; fade a whole bar with Opacity so its layers stay together.
	//! The heavier dark scrim is for status/nav bars over scrolling content.
	static const float GLASS_SCRIM_DARK = 0.85;
	static const float GLASS_SCRIM_DARK_CHROME = 0.95;
	static const float GLASS_ALPHA_DARK = 0.02;
	static const float GLASS_SPECULAR_DARK = 0.15;
	static const float GLASS_BLUR_DARK = 0.35;
	static const float GLASS_HAIRLINE_ALPHA_DARK = 0.64;

	//! Light glass: home cards, lock notifications, PIN keys, compose bar.
	static const float GLASS_SCRIM_LIGHT = 0.7;
	static const float GLASS_ALPHA_LIGHT = 0.14;
	static const float GLASS_SPECULAR_LIGHT = 0.2;
	static const float GLASS_BLUR_LIGHT = 0.4;
	static const float GLASS_HAIRLINE_ALPHA_LIGHT = 0.46;

	//! Accent glass mixes the scrim toward the app accent; GLOW is the optional GlassGlow layer.
	static const float GLASS_SCRIM_ACCENT = 0.9;
	static const float GLASS_ALPHA_ACCENT = 0.02;
	static const float GLASS_SPECULAR_ACCENT = 0.26;
	static const float GLASS_BLUR_ACCENT = 0.40;
	static const float GLASS_HAIRLINE_ALPHA_ACCENT = 0.62;
	static const float GLASS_GLOW_ALPHA_ACCENT = 0.25;

	//------------------------------------------------------------------------------------------------
	// Spacing - 8pt scale on the 2x canvas
	//------------------------------------------------------------------------------------------------
	static const float SPACE_1 = 8;
	static const float SPACE_2 = 16;
	static const float SPACE_3 = 24;
	static const float SPACE_4 = 32;
	static const float SPACE_5 = 40;
	static const float SPACE_6 = 48;

	//------------------------------------------------------------------------------------------------
	// Motion - ease-out only, no overshoot
	//------------------------------------------------------------------------------------------------
	static const int DURATION_STATE_MS = 180;
	static const int DURATION_PRESENT_MS = 280;
	static const int TICK_MS = 16;

	//! Delay before a spinner shows, and its minimum time on screen once it does.
	static const int SPINNER_DELAY_MS = 150;
	static const int SPINNER_MIN_VISIBLE_MS = 300;

	//------------------------------------------------------------------------------------------------
	//! Color channels are linear, so tokens are authored in sRGB and converted by Srgb(). Layouts
	//! can't call this and hardcode the converted values.
	static Color Srgb(float r, float g, float b, float a = 1)
	{
		return new Color(ToLinear(r), ToLinear(g), ToLinear(b), a);
	}

	//------------------------------------------------------------------------------------------------
	static float ToLinear(float channel)
	{
		if (channel <= 0.04045)
			return channel / 12.92;

		return Math.Pow((channel + 0.055) / 1.055, 2.4);
	}

	//------------------------------------------------------------------------------------------------
	// Neutrals
	//------------------------------------------------------------------------------------------------
	static Color Ink() { return Srgb(0.051, 0.059, 0.082); }                 //!< #0D0F15
	static Color Surface() { return Srgb(0.090, 0.102, 0.137); }             //!< #171A23
	static Color Hairline() { return Srgb(0.216, 0.239, 0.298); }            //!< #373D4C
	static Color TextPrimary() { return Srgb(0.925, 0.937, 0.965); }         //!< #ECEFF6
	static Color TextSecondary() { return Srgb(0.639, 0.667, 0.737); }       //!< #A3AABC
	static Color TextTertiary() { return Srgb(0.451, 0.478, 0.553); }        //!< #737A8D

	//! Scrim colour per glass look; accent glass uses the app's AccentDeep instead.
	static Color GlassBaseDark() { return Srgb(0.022, 0.027, 0.051); }       //!< #04070D
	static Color GlassBaseLight() { return Srgb(0.34, 0.38, 0.5); }      //!< #566181

	//! Tint and specular edge, in a dark and a light step. Accent glass takes dark tint, light specular.
	static Color GlassTintDark() { return Srgb(0.68, 0.71, 0.82); }          //!< #AEB5D1
	static Color GlassTintLight() { return Srgb(0.180, 0.220, 0.345); }      //!< #2D3858

	static Color GlassSpecularLight() { return Srgb(0.960, 0.973, 0.990); }  //!< #F5F8FC
	static Color GlassSpecularDark() { return Srgb(0.480, 0.505, 0.580); }   //!< #7A8194

	static Color GlassHairline() { return Srgb(0.020, 0.027, 0.043); }       //!< #05070B

	//------------------------------------------------------------------------------------------------
	// Per-app accents. Accent* is for text and controls; AccentDeep* is one step darker for filled
	// surfaces (tiles, avatars, outgoing bubbles) so light glyphs read on them.
	//------------------------------------------------------------------------------------------------
	static Color AccentBank() { return Srgb(0.878, 0.663, 0.290); }          //!< #E0A94A
	static Color AccentMessages() { return Srgb(0.373, 0.761, 0.494); }      //!< #5FC27E
	static Color AccentContacts() { return Srgb(0.310, 0.741, 0.769); }      //!< #4FBDC4
	static Color AccentMap() { return Srgb(0.486, 0.698, 0.933); }           //!< #7CB2EE
	static Color AccentSettings() { return Srgb(0.651, 0.694, 0.784); }      //!< #A6B1C8

	static Color AccentDeepBank() { return Srgb(0.706, 0.482, 0.071); }      //!< #B47B12
	static Color AccentDeepMessages() { return Srgb(0.137, 0.576, 0.353); }  //!< #23935A
	static Color AccentDeepContacts() { return Srgb(0.090, 0.565, 0.608); }  //!< #17909B
	static Color AccentDeepMap() { return Srgb(0.231, 0.498, 0.831); }       //!< #3B7FD4
	static Color AccentDeepSettings() { return Srgb(0.369, 0.412, 0.502); }  //!< #5E6980

	//! Money in and out.
	static Color Positive() { return AccentMessages(); }
	static Color Negative() { return Srgb(0.894, 0.576, 0.549); }            //!< #E4938C

	//------------------------------------------------------------------------------------------------
	//! No other display face ships without baking a glyph atlas, so hierarchy comes from weight and size.
	static const ResourceName FONT_REGULAR = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	static const ResourceName FONT_BOLD = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";

	//! Sizes at the 2x canvas.
	static const int TEXT_TITLE = 30;
	static const int TEXT_BODY = 22;
	static const int TEXT_CAPTION = 18;

	//! Tolar, Everon's currency. Swap to "TOL" if the glyph renders as a box.
	static const string CURRENCY_SYMBOL = "Ŧ";

	//! Use its "circle" sprite for round shapes - circleFull.edds has no real alpha.
	static const ResourceName ICON_SET_WRAPPER = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	static const ResourceName ICON_SET_CHAT = "{1872FFA1133724A2}UI/Textures/Chat/chat.imageset";

	//------------------------------------------------------------------------------------------------
	static Color AccentFor(EPhoneScreenState state)
	{
		switch (state)
		{
			case EPhoneScreenState.BANK: return AccentBank();
			case EPhoneScreenState.MESSAGES: return AccentMessages();
			case EPhoneScreenState.CONTACTS: return AccentContacts();
			case EPhoneScreenState.MAP: return AccentMap();
			case EPhoneScreenState.SETTINGS: return AccentSettings();
		}

		//! OS screens get the neutral anchor, not an app hue.
		return AccentSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Filled surfaces an app owns; one step darker than the accent so light glyphs read on it.
	static Color AccentDeepFor(EPhoneScreenState state)
	{
		switch (state)
		{
			case EPhoneScreenState.BANK: return AccentDeepBank();
			case EPhoneScreenState.MESSAGES: return AccentDeepMessages();
			case EPhoneScreenState.CONTACTS: return AccentDeepContacts();
			case EPhoneScreenState.MAP: return AccentDeepMap();
			case EPhoneScreenState.SETTINGS: return AccentDeepSettings();
		}

		return AccentDeepSettings();
	}

	//------------------------------------------------------------------------------------------------
	static Color WithAlpha(Color base, float alpha)
	{
		return new Color(base.R(), base.G(), base.B(), alpha);
	}

	//------------------------------------------------------------------------------------------------
	static Color Mix(Color from, Color to, float t)
	{
		return new Color(
			from.R() + (to.R() - from.R()) * t,
			from.G() + (to.G() - from.G()) * t,
			from.B() + (to.B() - from.B()) * t,
			from.A() + (to.A() - from.A()) * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Cubic ease-out.
	static float EaseOut(float t)
	{
		float inv = 1 - Math.Clamp(t, 0, 1);
		return 1 - inv * inv * inv;
	}

	//------------------------------------------------------------------------------------------------
	//! Paints the Glass* layers under bar. Precedence is dark > light > accent; chrome uses the heavier dark
	//! scrim. Resets layer opacity to 1, and draws a flat panel instead when GLASS_ENABLED is off.
	static void ApplyGlass(Widget bar, bool darkGlass = false, bool lightGlass = false, bool chrome = false, bool accentGlass = false, Color accentColor = null)
	{
		if (!bar)
			return;

		bool useLight = lightGlass && !darkGlass;
		bool useAccent = accentGlass && !darkGlass && !lightGlass;

		if (useAccent && !accentColor)
			accentColor = AccentSettings();

		float tintAlpha = GLASS_ALPHA_DARK;
		float specularAlpha = GLASS_SPECULAR_DARK;
		float scrimAlpha = GLASS_SCRIM_DARK;
		if (chrome)
			scrimAlpha = GLASS_SCRIM_DARK_CHROME;

		float blurIntensity = GLASS_BLUR_DARK;
		float hairlineAlpha = GLASS_HAIRLINE_ALPHA_DARK;
		float glowAlpha = 0;
		if (useLight)
		{
			tintAlpha = GLASS_ALPHA_LIGHT;
			specularAlpha = GLASS_SPECULAR_LIGHT;
			scrimAlpha = GLASS_SCRIM_LIGHT;
			blurIntensity = GLASS_BLUR_LIGHT;
			hairlineAlpha = GLASS_HAIRLINE_ALPHA_LIGHT;
		}
		else if (useAccent)
		{
			tintAlpha = GLASS_ALPHA_ACCENT;
			specularAlpha = GLASS_SPECULAR_ACCENT;
			scrimAlpha = GLASS_SCRIM_ACCENT;
			blurIntensity = GLASS_BLUR_ACCENT;
			hairlineAlpha = GLASS_HAIRLINE_ALPHA_ACCENT;
			glowAlpha = GLASS_GLOW_ALPHA_ACCENT;
		}

		BlurWidget blur = BlurWidget.Cast(bar.FindAnyWidget("GlassBlur"));
		Widget scrim = bar.FindAnyWidget("GlassScrim");
		Widget tint = bar.FindAnyWidget("GlassTint");
		Widget specular = bar.FindAnyWidget("GlassSpecular");
		Widget hairline = bar.FindAnyWidget("GlassHairline");
		Widget glow = bar.FindAnyWidget("GlassGlow");

		if (!GLASS_ENABLED)
		{
			//! On a scrimmed surface the scrim survives, being the layer already clipped to the shape.
			Color tintTowards = GlassTintDark();
			if (useLight)
				tintTowards = GlassTintLight();
			else if (useAccent)
				tintTowards = accentColor;

			Color solid = Mix(Surface(), tintTowards, tintAlpha);

			if (scrim)
			{
				scrim.SetColor(solid);

				if (tint)
					tint.SetVisible(false);
			}
			else if (tint)
			{
				tint.SetColor(solid);
			}

			if (specular)
				specular.SetVisible(false);

			if (hairline)
				hairline.SetVisible(false);

			if (glow)
				glow.SetVisible(false);

			//! No blur on the flat path - avoiding that cost is its point.
			if (blur)
				blur.SetVisible(false);

			return;
		}

		bar.SetOpacity(1);

		if (blur)
		{
			blur.SetVisible(true);
			blur.SetIntensity(blurIntensity);
			blur.SetOpacity(1);
		}

		if (scrim)
		{
			Color scrimBase = GlassBaseDark();
			if (useLight)
				scrimBase = GlassBaseLight();
			else if (useAccent)
				scrimBase = accentColor;

			scrim.SetColor(WithAlpha(scrimBase, scrimAlpha));
			scrim.SetOpacity(1);
		}

		if (tint)
		{
			//! Never the app colour, even on accent glass.
			Color tintColor = GlassTintDark();
			if (useLight)
				tintColor = GlassTintLight();

			tint.SetVisible(true);
			tint.SetColor(WithAlpha(tintColor, tintAlpha));
			tint.SetOpacity(1);
		}

		if (glow)
		{
			//! Glow sits above the icon; only accent glass lights it.
			if (useAccent)
			{
				glow.SetVisible(true);
				glow.SetColor(WithAlpha(accentColor, glowAlpha));
				glow.SetOpacity(1);
			}
			else
			{
				glow.SetVisible(false);
			}
		}

		if (specular)
		{
			Color specularBase = GlassSpecularDark();
			if (useLight || useAccent)
				specularBase = GlassSpecularLight();

			specular.SetVisible(true);
			specular.SetColor(WithAlpha(specularBase, specularAlpha));
			specular.SetOpacity(1);

			SizeLayoutWidget specularSize = SizeLayoutWidget.Cast(bar.FindAnyWidget("GlassSpecularSize"));
			if (specularSize)
			{
				specularSize.EnableHeightOverride(true);
				specularSize.SetHeightOverride(GLASS_SPECULAR_HEIGHT);
			}
		}

		if (hairline)
		{
			hairline.SetVisible(true);
			hairline.SetColor(WithAlpha(GlassHairline(), hairlineAlpha));
			hairline.SetOpacity(1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fades just the glass layers, so a header chip loses its pill while the control stays.
	static void SetGlassPresence(Widget bar, float opacity)
	{
		if (!bar)
			return;

		float presence = Math.Clamp(opacity, 0, 1);
		SetOpacityOf(bar, "GlassBlur", presence);
		SetOpacityOf(bar, "GlassScrim", presence);
		SetOpacityOf(bar, "GlassTint", presence);
		SetOpacityOf(bar, "GlassSpecular", presence);
		SetOpacityOf(bar, "GlassHairline", presence);
		SetOpacityOf(bar, "GlassGlow", presence);
	}

	//------------------------------------------------------------------------------------------------
	static void SetOpacityOf(Widget parent, string name, float opacity)
	{
		if (!parent)
			return;

		Widget widget = parent.FindAnyWidget(name);
		if (widget)
			widget.SetOpacity(opacity);
	}

	//------------------------------------------------------------------------------------------------
	//! Fits a sprite into a square box without distortion (wrapper cells aren't square). The widget
	//! must be centre-aligned, not stretched.
	static void FitIcon(ImageWidget image, float box)
	{
		if (!image)
			return;

		int sourceWidth, sourceHeight;
		image.GetImageSize(0, sourceWidth, sourceHeight);

		if (sourceWidth <= 0 || sourceHeight <= 0)
		{
			image.SetSize(box, box);
			return;
		}

		float scale = Math.Min(box / sourceWidth, box / sourceHeight);
		image.SetSize(sourceWidth * scale, sourceHeight * scale);
	}

	//------------------------------------------------------------------------------------------------
	static void SetColorOf(Widget parent, string name, Color color)
	{
		if (!parent || !color)
			return;

		Widget widget = parent.FindAnyWidget(name);
		if (widget)
			widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	static void SetTypeOf(Widget parent, string name, int size, bool bold, Color color = null)
	{
		if (!parent)
			return;

		TextWidget widget = TextWidget.Cast(parent.FindAnyWidget(name));
		if (!widget)
			return;

		widget.SetExactFontSize(size);
		widget.SetBold(bold);

		if (bold)
			widget.SetFont(FONT_BOLD);
		else
			widget.SetFont(FONT_REGULAR);

		if (color)
			widget.SetColor(color);
	}
}
