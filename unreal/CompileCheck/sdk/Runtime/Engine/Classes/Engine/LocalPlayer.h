// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/LocalPlayer.h (UE 5.x source knowledge; reflected
// class: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Engine/Player.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

class UGameInstance;
class UGameViewportClient;
class FViewport;
struct FSceneViewProjectionData;

class ULocalPlayer : public UPlayer
{
	UE_STUB_CLASS_BODY(ULocalPlayer, UPlayer)
public:
	UPROPERTY()
	TObjectPtr<UGameViewportClient> ViewportClient;
	FVector2D Origin;
	FVector2D Size;
	int32 GetControllerId() const;
	int32 GetLocalPlayerIndex() const;
	UGameInstance* GetGameInstance() const;
	virtual UWorld* GetWorld() const override;
	template <typename TSubsystemClass>
	TSubsystemClass* GetSubsystem() const
	{
		static_assert(std::is_base_of_v<ULocalPlayerSubsystem, TSubsystemClass>, "ULocalPlayer::GetSubsystem<T>: T must derive from ULocalPlayerSubsystem");
		return nullptr;
	}
	template <typename TSubsystemClass>
	static TSubsystemClass* GetSubsystem(const ULocalPlayer* LocalPlayer)
	{
		static_assert(std::is_base_of_v<ULocalPlayerSubsystem, TSubsystemClass>, "ULocalPlayer::GetSubsystem<T>: T must derive from ULocalPlayerSubsystem");
		return nullptr;
	}
	bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex = INDEX_NONE) const;
	bool GetPixelBoundingBox(const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, const FVector2D* OptionalAllotedSize = nullptr);
	TSharedPtr<class FSlateUser> GetSlateUser() const;
};
