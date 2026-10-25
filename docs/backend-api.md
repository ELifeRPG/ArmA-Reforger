# Backend API client

`ELIFE_Api` is the mod's REST client, wrapping Enfusion's `RestApi` to talk to the
Bridge (the local, unauthenticated proxy the gameserver runs alongside itself — never
the Central API directly).

## Setup

### Workbench (local dev)

No setup needed — defaults to `http://127.0.0.1:5200/` if no config file exists.

Only create `$profile:ELifeRPG.json` if you need to point at something other than a
locally-running Bridge:

```json
{
  "serverUrl": "http://127.0.0.1:5200/"
}
```

- Workbench profile folder: `Documents/My Games/ArmaReforgerWorkbench/profile/`

### Production / dedicated server

- **The config file is required.** No fallback — the server refuses to start
  (`GetGame().RequestClose()`) if it's missing or `serverUrl` is empty.
- Pin the profile directory explicitly with the `-profile <path>` startup parameter
  rather than relying on platform defaults — makes the deployment predictable and
  easy to back up.
- Place `ELifeRPG.json` (same format as above) at `<profile path>/ELifeRPG.json`,
  pointed at the real Bridge URL for that environment.
- On startup the server also checks the Bridge is actually reachable — retries 5x,
  2s apart, then refuses to start if it still can't connect. Workbench only logs a
  warning for the same check and keeps running (see `ELIFE_GameMode.OnGameStart()`).

## Using it in new features

```c
class MyFeatureCallback : ELIFE_BaseRestCallback
{
    override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
    {
        MyResponseDto dto = new MyResponseDto();
        dto.ExpandFromRAW(data);
        resultData = dto;
        return ELIFE_EApiStatusCode.SUCCESS;
    }
}

class MyFeatureComponent : ScriptComponent
{
    protected ref MyFeatureCallback m_Callback; // must be a ref field, see below

    void RequestSomething()
    {
        m_Callback = new MyFeatureCallback();
        m_Callback.SetCallback(this, "OnSomethingResult");
        ELIFE_Api.GetInstance().GetElifeApi().GET(m_Callback, "some/route");
    }

    void OnSomethingResult(ELIFE_EApiStatusCode status, JsonApiStruct data)
    {
        // handle result
    }
}
```

- **`GetElifeApi()`** returns a `RestContext` — call `.GET()`, `.POST()`, `.PUT()`,
  `.DELETE()` on it with a callback and a route relative to the configured server URL.
- **DTOs** are `JsonApiStruct` subclasses: register each field with `RegV()` in the
  constructor, then `ExpandFromRAW(data)` to populate one from a response body.
- **Two rules, both silent if broken:**
  - The callback object must be held in a **`ref` field** on something long-lived
    (a component, a manager singleton) — never a local variable. A local gets
    garbage-collected before an async response can arrive, and the response is just
    lost with no error.
  - The method named in `SetCallback(instance, "MethodName")` must be **public** — a
    protected/private target throws a VM exception once a response actually arrives.
