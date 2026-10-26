//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array thread list as {"items": <raw>} so JsonApiStruct can parse it.
class ELIFE_MessageThreadSummaryListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_MessageThreadSummary> items = {};

	void ELIFE_MessageThreadSummaryListDto()
	{
		RegV("items");
	}
}
