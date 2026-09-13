//------------------------------------------------------------------------------------------------
//! Single source of truth for every value the phone OS draws with: colours, type sizes, spacing,
//! radii, durations, ZOrder levels and the per-app accent palette.
//!
//! Rule: no colour, size or duration is invented at a call site. If a screen needs a value that
//! isn't here, the token gets added here first.
//!
//! Colours are OKLCH-derived and converted to Enfusion's 0..1 RGBA. The neutrals are all tinted
//! toward the OS anchor hue (a cool slate, ~225 deg) - there is no pure black or pure white in the
//! phone UI. The five app accents share one lightness (L* ~ 72) and a similar chroma; only the hue
//! rotates, which is what makes them read as one family instead of five themes.
class ELIFE_PhoneStyle
{
	//------------------------------------------------------------------------------------------------
	// Depth model
	//
	// Wallpaper/ground -> content -> chrome (status bar, nav bar, bottom bar) -> sheets -> alerts.
	// Stacking only. Glass looks are a separate three-style system (dark / light / accent), not an altitude.
	//------------------------------------------------------------------------------------------------
	static const int ZORDER_GROUND = 0;
	static const int ZORDER_CONTENT = 10;
	static const int ZORDER_CHROME = 20;
	static const int ZORDER_SHEET = 30;
	static const int ZORDER_ALERT = 40;

	//------------------------------------------------------------------------------------------------
	//! The lock/home wallpaper uses one shipped dithered gradient texture (a flat ramp bands across 500px of near-black). Flip if the shipped texture runs bright-at-bottom.
	static const bool GRADIENT_FLIPPED = false;

	//! Flat fallback path. Set false to swap every glass stack (tint + specular edge + hairline) for
	//! one solid panel at the same lightness - one boolean, both canvases, no layout edits.
	static const bool GLASS_ENABLED = true;
	static const float GLASS_SPECULAR_HEIGHT = 2;

	//! Alpha is the material (fixed per look, never per instance); Opacity is presence (applied to the whole bar so tint/specular/hairline composite as one layer - editing the three alphas separately would detach the specular line).
	//! Dark glass's scrim has two strengths: GLASS_SCRIM_DARK for inline cards on a flat page ground, GLASS_SCRIM_DARK_CHROME for status/nav bars covering scrolling content. Same recipe otherwise.
	static const float GLASS_SCRIM_DARK = 0.75;
	static const float GLASS_SCRIM_DARK_CHROME = 0.95;
	static const float GLASS_ALPHA_DARK = 0.01;
	static const float GLASS_SPECULAR_DARK = 0.3;
	static const float GLASS_BLUR_DARK = 0.35;
	static const float GLASS_HAIRLINE_ALPHA_DARK = 0.14;

	//! Light glass - the brighter look (home cards, lock notifications, PIN keys, compose bar, trailing nav action at rest).
	static const float GLASS_SCRIM_LIGHT = 0.4;
	static const float GLASS_ALPHA_LIGHT = 0.05;
	static const float GLASS_SPECULAR_LIGHT = 0.07;
	static const float GLASS_BLUR_LIGHT = 0.25;
	static const float GLASS_HAIRLINE_ALPHA_LIGHT = 0.26;

	//! Accent glass - same structure as dark/light, but the scrim mixes toward the app's own accent instead of a fixed neutral. GLASS_GLOW_ALPHA_ACCENT is the optional GlassGlow layer's own strength (see ApplyGlass).
	static const float GLASS_SCRIM_ACCENT = 0.9;
	static const float GLASS_ALPHA_ACCENT = 0.02;
	static const float GLASS_SPECULAR_ACCENT = 0.26;
	static const float GLASS_BLUR_ACCENT = 0.40;
	static const float GLASS_HAIRLINE_ALPHA_ACCENT = 0.42;
	static const float GLASS_GLOW_ALPHA_ACCENT = 0.25;

	//------------------------------------------------------------------------------------------------
	// Spacing - 4pt scale at this canvas (the 8pt scale of a 568-wide phone, halved for 236)
	//------------------------------------------------------------------------------------------------
	static const float SPACE_1 = 4;
	static const float SPACE_2 = 8;
	static const float SPACE_3 = 12;
	static const float SPACE_4 = 16;
	static const float SPACE_5 = 20;
	static const float SPACE_6 = 24;

	//! Leading inset for list content, and where an inset separator starts when the row has an avatar.
	static const float INSET_LEADING = 12;
	static const float INSET_SEPARATOR_AVATAR = 44;

	static const float ROW_HEIGHT_SINGLE = 30;
	static const float ROW_HEIGHT_DOUBLE = 34;

	//! A row whose value sits *under* its label rather than beside it - a long identifier that will
	//! not fit a value column (Settings' device id) or an editable field (the contact form).
	static const float ROW_HEIGHT_STACKED = 46;

	static const float AVATAR_SIZE = 28;

	//------------------------------------------------------------------------------------------------
	// Motion - ease-out translations, no springs, no overshoot. One entrance per screen.
	//------------------------------------------------------------------------------------------------
	static const int DURATION_STATE_MS = 180;
	static const int DURATION_PRESENT_MS = 280;
	static const int DURATION_SHEET_MS = 320;
	static const int TICK_MS = 16;

	//! Spinner discipline: never flash. Delay before showing, minimum time on screen once shown.
	static const int SPINNER_DELAY_MS = 150;
	static const int SPINNER_MIN_VISIBLE_MS = 300;

	//------------------------------------------------------------------------------------------------
	//! Enfusion's Color channels are linear light, not sRGB - a raw sRGB hex typed straight in renders too bright and desaturated, so the palette below is authored in sRGB and Srgb() converts it. Layout files hardcode the already-converted linear values for these same tokens since they can't call this.
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
	// Neutrals - anchor hue ~225 deg. sRGB design values in the comments.
	//------------------------------------------------------------------------------------------------
	static Color Ink() { return Srgb(0.051, 0.059, 0.082); }                 //!< #0D0F15

	//! A step blacker than Ink(), still hue-tinted rather than pure #000. Dark glass's scrim mixes toward this so it reads as genuinely black glass, not just another near-black surface.
	static Color InkDeep() { return Srgb(0.006, 0.007, 0.010); }             //!< #020203

	//! Light glass's counterpart to InkDeep() - a light hue-tinted neutral its scrim mixes toward instead.
	static Color LightBase() { return Srgb(0.320, 0.340, 0.370); }           //!< #51575E
	static Color Surface() { return Srgb(0.090, 0.102, 0.137); }             //!< #171A23
	static Color SurfaceRaised() { return Srgb(0.133, 0.149, 0.196); }       //!< #222632
	static Color Hairline() { return Srgb(0.216, 0.239, 0.298); }            //!< #373D4C
	static Color TextPrimary() { return Srgb(0.925, 0.937, 0.965); }         //!< #ECEFF6
	static Color TextSecondary() { return Srgb(0.639, 0.667, 0.737); }       //!< #A3AABC
	static Color TextTertiary() { return Srgb(0.451, 0.478, 0.553); }        //!< #737A8D

	//! The glass tint is a near-white pulled toward the anchor hue. The colour of a glass surface
	//! comes through from the content below it - the tint never carries a hue of its own.
	static Color GlassTint() { return Srgb(0.788, 0.824, 0.910); }           //!< #C9D2E8
	static Color GlassSpecular() { return Srgb(0.900, 0.920, 0.970); }       //!< #E5EBF7
	static Color GlassHairline() { return Srgb(0.020, 0.027, 0.043); }       //!< #05070B

	//! Dark glass's own specular colour - the same near-white line dimmed down to a mid grey, so it
	//! reads as a faint edge catching light rather than a bright stroke laid over a near-black card.
	static Color GlassSpecularDark() { return Srgb(0.370, 0.390, 0.450); }   //!< #5E6373

	//! The phone case as drawn in the in-hand bezel. Kept plain on purpose.
	static Color Bezel() { return Srgb(0.071, 0.078, 0.102); }               //!< #12141A

	//------------------------------------------------------------------------------------------------
	// Per-app accents - one family, hue rotating, Settings the most desaturated.
	//
	// Two steps per app, both declared here so they stay a family:
	//   Accent*     - L* ~72, for text, badges, the nav bar's controls and the app's primary action.
	//   AccentDeep* - L* ~52, same hue and chroma family, for the filled surfaces an app owns: its
	//                 home tile, its avatars, its outbound message bubbles.
	// The two-step split is what lets a tile be genuinely saturated and still carry near-white glyphs.
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

	//! Money moves in two directions and nothing else does, so these are the only semantic pair.
	//! Both sit at the same lightness as the accents so a statement doesn't shout.
	static Color Positive() { return AccentMessages(); }
	static Color Negative() { return Srgb(0.894, 0.576, 0.549); }            //!< #E4938C

	//------------------------------------------------------------------------------------------------
	//! RobotoCondensed at two weights - Reforger ships no other display face without baking a new glyph atlas, so display/body is carried by weight+size instead (Bold for clock/titles/balances, Regular for body/subhead/caption).
	static const ResourceName FONT_REGULAR = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	static const ResourceName FONT_BOLD = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";

	//! Sizes at the 236x443 screen. Anything below TEXT_FLOOR is not allowed to carry meaning.
	static const int TEXT_DISPLAY = 46;
	static const int TEXT_HERO = 26;
	static const int TEXT_TITLE_LARGE = 20;
	static const int TEXT_TITLE = 15;
	static const int TEXT_BODY = 11;
	static const int TEXT_SUBHEAD = 10;
	static const int TEXT_CAPTION = 9;
	static const int TEXT_FLOOR = 9;

	//! Home clock only - its own hero readout, distinct from lock's TEXT_DISPLAY (46) so the two screens don't fight for the same size.
	static const int TEXT_CLOCK = 34;

	//! The tolar is Everon's own in-game currency (modelled into the fuel station price display); Ŧ (U+0166) is our chosen mark since none is attested in game data. If it renders as a blank box, swap this to "TOL".
	static const string CURRENCY_SYMBOL = "Ŧ";

	//! Reforger's own icon sets. Public so both ELIFE_PhoneScreenShell (app tiles/home cards) and ELIFE_PhoneAppBase (nav actions) can load sprites without duplicating the GUID.
	static const ResourceName ICON_SET_WRAPPER = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	static const ResourceName ICON_SET_CHAT = "{1872FFA1133724A2}UI/Textures/Chat/chat.imageset";

	//! Round shapes must come from the wrapper set's "circle" sprite via LoadImageFromSet() - circleFull.edds and RadialMenuMaskInverse.edds look like filled circles but have no real alpha and draw as opaque squares.

	//------------------------------------------------------------------------------------------------
	//! Reforger ships only rounded_2px/rounded_6px/rounded_faded_16 (SmartPanelWidget has no radius setter, a wider one needs a baked atlas), so 6px is a hard ceiling. rounded_2px is for small inner details a 6px arc would swallow.
	static const string STYLE_RADIUS_SCREEN = "rounded_6px";
	static const string STYLE_RADIUS_DETAIL = "rounded_2px";
	static const string STYLE_RADIUS_ELEMENT = "rounded_6px";

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

		//! Home, lock and off are OS-owned, so they get the anchor neutral rather than an app hue.
		return AccentSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! The filled surface an app owns - its home tile, its avatars, its outbound bubbles. Saturated
	//! at full strength, one lightness step down from the accent so a near-white glyph reads on it.
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
	//! Cubic ease-out. The only easing curve the phone uses - no overshoot anywhere.
	static float EaseOut(float t)
	{
		float inv = 1 - Math.Clamp(t, 0, 1);
		return 1 - inv * inv * inv;
	}

	//------------------------------------------------------------------------------------------------
	//! Paints one glass surface on child widgets named GlassBlur/GlassScrim/GlassTint/GlassSpecular/GlassHairline(/GlassGlow). dark/light/accentGlass select the look (mutually exclusive, dark > light > accent); accentGlass mixes the scrim toward accentColor (defaults to AccentSettings()) and, if a GlassGlow child exists, paints that too. `chrome` swaps the dark scrim for the heavier GLASS_SCRIM_DARK_CHROME. Sets Opacity 1 on every layer - a caller revealing from scroll (ApplyCollapse) must reset that itself. Falls back to one flat solid panel when GLASS_ENABLED is false.
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
			//! Same perceived lightness, one layer, no edges. On a scrimmed surface the scrim is the
			//! layer that survives, because it is the one already clipped to the shape.
			Color tintTowards = GlassTint();
			if (useAccent)
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

			//! The flat path is for the world render target, where a per-surface blur is exactly the
			//! cost it exists to avoid.
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
			//! Scrim is the base fill colour, and where each variant's hue actually comes from.
			Color scrimBase = InkDeep();
			if (useLight)
				scrimBase = LightBase();
			else if (useAccent)
				scrimBase = accentColor;

			scrim.SetColor(WithAlpha(scrimBase, scrimAlpha));
			scrim.SetOpacity(1);
		}

		if (tint)
		{
			//! Tint stays the same neutral near-white material for every variant, accent glass included.
			tint.SetVisible(true);
			tint.SetColor(WithAlpha(GlassTint(), tintAlpha));
			tint.SetOpacity(1);
		}

		if (glow)
		{
			//! GlassGlow paints above the icon/glyph, not below it - only accent glass lights it.
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
				specularBase = GlassSpecular();

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
	//! Fades only the glass layers, not the bar or the label on it. Used by ApplyCollapse so a header
	//! chip (Back, the trailing action) can lose its own pill as the nav bar's glass comes in - the
	//! control stays, the button chrome does not.
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
	//! Sizes a loaded sprite to fit a square box without distorting it - an icons_wrapperUI cell isn't necessarily square, so a fixed Size N N shifts the ink off centre. The widget must be centre-aligned, not stretched, or stretching re-imposes the wrapper's aspect.
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
	//! Sets size, weight, and colour together on a shared TextWidget instead of leaving them as an unstated layout default.
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
