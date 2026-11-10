// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/UObject/UnrealNames.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

enum class EName : uint32
{
	None = 0,
	ByteProperty, IntProperty, BoolProperty, FloatProperty, ObjectProperty, NameProperty, DelegateProperty, DoubleProperty,
	ArrayProperty, StructProperty, VectorProperty, RotatorProperty, StrProperty, TextProperty, InterfaceProperty,
	Core, Engine, Editor, CoreUObject, Game, GameUserSettings, MaxHardcodedNameIndex,
};
inline constexpr EName NAME_None = EName::None;
inline constexpr EName NAME_Core = EName::Core;
inline constexpr EName NAME_Engine = EName::Engine;
inline constexpr EName NAME_Game = EName::Game;
