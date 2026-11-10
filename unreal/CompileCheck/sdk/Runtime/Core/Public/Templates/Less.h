// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/Templates/Less.h and Greater.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

template <typename T = void>
struct TLess { constexpr bool operator()(const T& A, const T& B) const { return A < B; } };
template <>
struct TLess<void> { template <typename T, typename U> constexpr bool operator()(T&& A, U&& B) const { return A < B; } };
template <typename T = void>
struct TGreater { constexpr bool operator()(const T& A, const T& B) const { return B < A; } };
template <>
struct TGreater<void> { template <typename T, typename U> constexpr bool operator()(T&& A, U&& B) const { return B < A; } };
