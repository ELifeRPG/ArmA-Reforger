//------------------------------------------------------------------------------------------------
//! Registers the local phone ChimeraMenus.
//!
//! WORKBENCH (required):
//! 1. Resource Browser → ArmaReforger / Configs/System/chimeraMenus.conf (GUID {C747AFB6B750CE9A})
//! 2. Right-click → Override in ELifeRPG (keeps the vanilla GUID)
//! 3. Add an entry named ELIFE_PhoneMenu
//!    - Class: ELIFE_PhoneMenu
//!    - Layout: UI/layouts/Menus/Phone/PhoneMenu.layout
//!    - ActionContext: MenuContext (cursor; N is already on that context)
//! 4. Add an entry named ELIFE_PhoneMapMenu
//!    - Class: ELIFE_PhoneMapMenuUI
//!    - Layout: UI/layouts/Map/MapMenu.layout (same GUID as vanilla MapMenu)
//!    - ActionContext: MapContext
modded enum ChimeraMenuPreset
{
	ELIFE_PhoneMenu,
	ELIFE_PhoneMapMenu
}
