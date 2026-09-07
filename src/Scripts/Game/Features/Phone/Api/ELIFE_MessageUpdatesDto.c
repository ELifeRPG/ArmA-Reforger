//------------------------------------------------------------------------------------------------
//! Response of GET /apps/messages/updates. Without a "since" cursor every thread comes back whole;
//! "polledAt" is the cursor for the next poll.
class ELIFE_MessageUpdatesDto : JsonApiStruct
{
	string polledAt;
	ref array<ref ELIFE_ThreadDto> threads = {};

	void ELIFE_MessageUpdatesDto()
	{
		RegV("polledAt");
		RegV("threads");
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_MessageUpdatesDto Redact()
	{
		ELIFE_MessageUpdatesDto redacted = new ELIFE_MessageUpdatesDto();
		redacted.polledAt = polledAt;

		foreach (ELIFE_ThreadDto threadDto : threads)
			redacted.threads.Insert(threadDto.Redact());

		return redacted;
	}
}
