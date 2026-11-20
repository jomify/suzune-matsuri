// Suzune's Night Festival - the boss's language model client (DeepSeek or a local Ollama), OpenAI-compatible.

#include "MatsuriBossModelClient.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SuzuneMatsuri.h"

namespace
{
	TAutoConsoleVariable<FString> CVarProvider(
		TEXT("matsuri.BossLLM"), TEXT("deepseek"),
		TEXT("Who reasons for the boss: deepseek (needs DEEPSEEK_API_KEY), ollama (local, http://localhost:11434), off (local learner only)."));

	TAutoConsoleVariable<FString> CVarModel(
		TEXT("matsuri.BossModel"), TEXT(""),
		TEXT("Model name override (default deepseek-flash, or qwen2.5:3b for ollama)."));

	TAutoConsoleVariable<float> CVarTimeout(
		TEXT("matsuri.BossLLMTimeout"), 6.0f,
		TEXT("Seconds the boss waits for a decision before the local learner decides instead."));

	const TCHAR* const DeepSeekUrl = TEXT("https://api.deepseek.com/chat/completions");
	const TCHAR* const DeepSeekModel = TEXT("deepseek-flash");
	const TCHAR* const OllamaUrl = TEXT("http://localhost:11434/v1/chat/completions");
	const TCHAR* const OllamaModel = TEXT("qwen2.5:3b");
	const TCHAR* const KeyVariable = TEXT("DEEPSEEK_API_KEY");
	constexpr float ReflectionTimeoutSeconds = 120.0f;
}

FMatsuriModelSettings FMatsuriModelSettings::FromConsole()
{
	FMatsuriModelSettings Out;
	Out.Provider = CVarProvider.GetValueOnGameThread().ToLower();
	Out.TimeoutSeconds = FMath::Max(1.0f, CVarTimeout.GetValueOnGameThread());
	const FString Model = CVarModel.GetValueOnGameThread();
	if (Out.Provider == TEXT("deepseek"))
	{
		Out.Url = DeepSeekUrl;
		Out.Model = Model.IsEmpty() ? DeepSeekModel : Model;
		Out.ApiKey = FPlatformMisc::GetEnvironmentVariable(KeyVariable);
	}
	else if (Out.Provider == TEXT("ollama"))
	{
		Out.Url = OllamaUrl;
		Out.Model = Model.IsEmpty() ? OllamaModel : Model;
		Out.bDisableThinking = false;	// not a DeepSeek field
	}
	return Out;
}

bool FMatsuriModelSettings::IsUsable() const
{
	if (Provider == TEXT("deepseek"))
	{
		return !ApiKey.IsEmpty();
	}
	return Provider == TEXT("ollama");
}

FMatsuriBossModelClient::FMatsuriBossModelClient(const FMatsuriModelSettings& InSettings)
	: Settings(InSettings)
{
}

FString FMatsuriBossModelClient::BuildBody(const FString& System, const FString& User, int32 MaxTokens, bool bThinking, bool bJson) const
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Settings.Model);
	TArray<TSharedPtr<FJsonValue>> Messages;
	auto AddMessage = [&Messages](const TCHAR* Role, const FString& Content)
	{
		TSharedRef<FJsonObject> One = MakeShared<FJsonObject>();
		One->SetStringField(TEXT("role"), Role);
		One->SetStringField(TEXT("content"), Content);
		Messages.Add(MakeShared<FJsonValueObject>(One));
	};
	AddMessage(TEXT("system"), System);
	AddMessage(TEXT("user"), User);
	Body->SetArrayField(TEXT("messages"), Messages);
	Body->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Body->SetBoolField(TEXT("stream"), false);
	if (bJson)
	{
		TSharedRef<FJsonObject> Format = MakeShared<FJsonObject>();
		Format->SetStringField(TEXT("type"), TEXT("json_object"));
		Body->SetObjectField(TEXT("response_format"), Format);
	}
	if (Settings.Provider == TEXT("deepseek"))
	{
		TSharedRef<FJsonObject> Thinking = MakeShared<FJsonObject>();
		Thinking->SetStringField(TEXT("type"), bThinking || !Settings.bDisableThinking ? TEXT("enabled") : TEXT("disabled"));
		Body->SetObjectField(TEXT("thinking"), Thinking);
		if (bThinking)
		{
			Body->SetStringField(TEXT("reasoning_effort"), TEXT("high"));
		}
		else
		{
			Body->SetNumberField(TEXT("temperature"), 0.7);
		}
	}
	else
	{
		Body->SetNumberField(TEXT("temperature"), 0.7);
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Body, Writer);
	return Text;
}

FMatsuriModelReply FMatsuriBossModelClient::ParseResponse(const FString& Body)
{
	FMatsuriModelReply Reply;
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) || !Json)
	{
		Reply.Error = TEXT("the response is not JSON");
		return Reply;
	}
	const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
	if (Json->TryGetObjectField(TEXT("error"), ErrorObject))
	{
		Reply.Error = (*ErrorObject)->GetStringField(TEXT("message")).Left(200);
		return Reply;
	}
	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!Json->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
	{
		Reply.Error = TEXT("no choices in the response");
		return Reply;
	}
	const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
	const TSharedPtr<FJsonObject> Message = Choice->GetObjectField(TEXT("message"));
	if (!Message || !Message->TryGetStringField(TEXT("content"), Reply.Content) || Reply.Content.TrimStartAndEnd().IsEmpty())
	{
		FString Finish;
		Choice->TryGetStringField(TEXT("finish_reason"), Finish);
		Reply.Error = FString::Printf(TEXT("empty answer (finish_reason %s)"), Finish.IsEmpty() ? TEXT("?") : *Finish);	// "length": out of tokens
		return Reply;
	}
	const TSharedPtr<FJsonObject>* Usage = nullptr;
	if (Json->TryGetObjectField(TEXT("usage"), Usage))
	{
		Reply.PromptTokens = static_cast<int32>((*Usage)->GetNumberField(TEXT("prompt_tokens")));
		Reply.CompletionTokens = static_cast<int32>((*Usage)->GetNumberField(TEXT("completion_tokens")));
	}
	Reply.bOk = true;
	return Reply;
}

void FMatsuriBossModelClient::Chat(const FString& System, const FString& User, int32 MaxTokens, bool bThinking, bool bJson,
	TFunction<void(const FMatsuriModelReply&)> Done) const
{
	if (!Settings.IsUsable())
	{
		FMatsuriModelReply Reply;
		Reply.Error = Settings.Provider == TEXT("deepseek") ? TEXT("DEEPSEEK_API_KEY is not set") : TEXT("the model is off");
		Done(Reply);
		return;
	}
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Settings.Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!Settings.ApiKey.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Settings.ApiKey);
	}
	Request->SetContentAsString(BuildBody(System, User, MaxTokens, bThinking, bJson));
	Request->SetTimeout(bThinking ? FMath::Max(ReflectionTimeoutSeconds, Settings.TimeoutSeconds) : Settings.TimeoutSeconds);	// reflection is not in a hurry
	const double Started = FPlatformTime::Seconds();
	Request->OnProcessRequestComplete().BindLambda([Done = MoveTemp(Done), Started](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		FMatsuriModelReply Reply;
		if (!bConnected || !Response.IsValid())
		{
			Reply.Error = TEXT("no connection or timed out");
		}
		else if (Response->GetResponseCode() / 100 != 2)
		{
			Reply = ParseResponse(Response->GetContentAsString());
			Reply.bOk = false;
			Reply.Error = FString::Printf(TEXT("HTTP %d %s"), Response->GetResponseCode(), *Reply.Error);
		}
		else
		{
			Reply = ParseResponse(Response->GetContentAsString());
		}
		Reply.Seconds = FPlatformTime::Seconds() - Started;
		Done(Reply);
	});
	Request->ProcessRequest();
}
