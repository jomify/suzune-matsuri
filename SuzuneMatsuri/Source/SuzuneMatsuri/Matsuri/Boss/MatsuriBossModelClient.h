// Suzune's Night Festival - the boss's language model client (DeepSeek or a local Ollama), OpenAI-compatible.

#pragma once

#include "CoreMinimal.h"

/** Where her decisions are sent. */
struct FMatsuriModelSettings
{
	FString Provider;		// "deepseek", "ollama" or "off"
	FString Url;			// full chat/completions URL
	FString Model;
	FString ApiKey;			// never logged, never written anywhere
	float TimeoutSeconds = 6.0f;
	bool bDisableThinking = true;	// DeepSeek: fast, non-thinking answers during the fight

	/** From matsuri.BossLLM / matsuri.BossModel and the DEEPSEEK_API_KEY environment variable. */
	static FMatsuriModelSettings FromConsole();

	bool IsUsable() const;
};

/** One reply: the answer text, or why there is none. */
struct FMatsuriModelReply
{
	bool bOk = false;
	FString Content;
	FString Error;
	double Seconds = 0.0;
	int32 PromptTokens = 0;
	int32 CompletionTokens = 0;
};

/**
 *  Sends one chat request and calls back on the game thread. Asynchronous: the fight never waits on it.
 *  Safe to outlive the world (the callback captures only what it needs).
 */
class SUZUNEMATSURI_API FMatsuriBossModelClient : public TSharedFromThis<FMatsuriBossModelClient>
{
public:
	explicit FMatsuriBossModelClient(const FMatsuriModelSettings& InSettings);

	const FMatsuriModelSettings& GetSettings() const { return Settings; }

	/** bThinking asks DeepSeek to think first (the after-fight reflection); bJson asks for a JSON object. */
	void Chat(const FString& System, const FString& User, int32 MaxTokens, bool bThinking, bool bJson,
		TFunction<void(const FMatsuriModelReply&)> Done) const;

	/** The request body (tests check it without sending anything). */
	FString BuildBody(const FString& System, const FString& User, int32 MaxTokens, bool bThinking, bool bJson) const;

	/** Pulls content and token counts out of a chat/completions response. */
	static FMatsuriModelReply ParseResponse(const FString& Body);

private:
	FMatsuriModelSettings Settings;
};
