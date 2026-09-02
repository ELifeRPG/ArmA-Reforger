//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array contacts response so JsonApiStruct (object-rooted) can parse it -
//! caller wraps the raw array text as {"items": <raw>} before ExpandFromRAW().
class ELIFE_ContactListDto : JsonApiStruct
{
	ref array<ref ELIFE_ContactDto> items = {};

	void ELIFE_ContactListDto()
	{
		RegV("items");
	}
}
