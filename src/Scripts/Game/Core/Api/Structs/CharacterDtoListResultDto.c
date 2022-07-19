class CharacterDtoListResultDto : JsonApiStruct
{
    ref array<ref MessageDto> messages = {};
    ref array<ref CharacterDto> data = {};
	
	void CharacterDtoListResultDto()
	{
		RegV("messages");
		RegV("data");
	}
}
