//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array store response as {"items": <raw>} so JsonApiStruct can parse it.
class ELIFE_StoreAppListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_StoreApp> items = {};

	void ELIFE_StoreAppListDto()
	{
		RegV("items");
	}

	//------------------------------------------------------------------------------------------------
	bool Offers(string appKey)
	{
		foreach (ELIFE_StoreApp app : items)
		{
			if (app && ELIFE_PhoneAppCatalog.SameKey(app.key, appKey))
				return true;
		}

		return false;
	}
}
