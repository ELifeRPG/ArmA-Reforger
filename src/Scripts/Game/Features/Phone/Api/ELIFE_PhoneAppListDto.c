//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array installed-apps response as {"items": <raw>} so JsonApiStruct can parse it.
class ELIFE_PhoneAppListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_PhoneApp> items = {};

	void ELIFE_PhoneAppListDto()
	{
		RegV("items");
	}

	//------------------------------------------------------------------------------------------------
	bool Has(string appKey)
	{
		foreach (ELIFE_PhoneApp app : items)
		{
			if (app && ELIFE_PhoneAppCatalog.SameKey(app.key, appKey))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh, packed copy with appKey added or removed - this one may be ExpandFromRAW'd.
	ELIFE_PhoneAppListDto With(string appKey, bool installed)
	{
		ELIFE_PhoneAppListDto copy = new ELIFE_PhoneAppListDto();

		foreach (ELIFE_PhoneApp app : items)
		{
			if (!app || ELIFE_PhoneAppCatalog.SameKey(app.key, appKey))
				continue;

			copy.items.Insert(CopyOf(app.key, app.displayName));
		}

		if (installed)
			copy.items.Insert(CopyOf(appKey, appKey));

		copy.StashPacked();
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	protected static ELIFE_PhoneApp CopyOf(string key, string displayName)
	{
		ELIFE_PhoneApp app = new ELIFE_PhoneApp();
		app.key = key;
		app.displayName = displayName;
		return app;
	}
}
