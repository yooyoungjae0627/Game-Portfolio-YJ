#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

void UMosesUIRegistrySubsystem::SetLobbyWidgetClassPath(const FSoftClassPath& InPath)
{
	LobbyWidgetClassPath = InPath;
}

void UMosesUIRegistrySubsystem::ClearLobbyWidgetClassPath()
{
	LobbyWidgetClassPath = FSoftClassPath();
}

FSoftClassPath UMosesUIRegistrySubsystem::GetLobbyWidgetClassPath() const
{
	return LobbyWidgetClassPath;
}
