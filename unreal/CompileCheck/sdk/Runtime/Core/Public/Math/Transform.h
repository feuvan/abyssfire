// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/Math/Transform.h, TransformNonVectorized.h /
// TransformVectorized.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"

namespace UE::Math
{
	template <typename T>
	struct TTransform
	{
		using FReal = T;
		static const TTransform<T> Identity;
		TTransform() UE_STUB_BODY
		explicit TTransform(const TVector<T>& InTranslation) UE_STUB_BODY
		explicit TTransform(const TQuat<T>& InRotation) UE_STUB_BODY
		explicit TTransform(const TRotator<T>& InRotation) UE_STUB_BODY
		TTransform(const TQuat<T>& InRotation, const TVector<T>& InTranslation, const TVector<T>& InScale3D = TVector<T>::OneVector) UE_STUB_BODY
		TTransform(const TRotator<T>& InRotation, const TVector<T>& InTranslation, const TVector<T>& InScale3D = TVector<T>::OneVector) UE_STUB_BODY
		explicit TTransform(ENoInit) UE_STUB_BODY
		explicit TTransform(const TMatrix<T>& InMatrix) UE_STUB_BODY
		TTransform(const TVector<T>& InX, const TVector<T>& InY, const TVector<T>& InZ, const TVector<T>& InTranslation) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit TTransform(const TTransform<FArg>& From) UE_STUB_BODY
		void DebugPrint() const UE_STUB_BODY
		FString ToHumanReadableString() const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		bool InitFromString(const FString& InSourceString) UE_STUB_BODY
		TMatrix<T> ToMatrixWithScale() const UE_STUB_BODY
		TMatrix<T> ToInverseMatrixWithScale() const UE_STUB_BODY
		TTransform Inverse() const UE_STUB_BODY
		TMatrix<T> ToMatrixNoScale() const UE_STUB_BODY
		void Blend(const TTransform& Atom1, const TTransform& Atom2, T Alpha) UE_STUB_BODY
		void BlendWith(const TTransform& OtherAtom, T Alpha) UE_STUB_BODY
		TTransform operator+(const TTransform& Atom) const UE_STUB_BODY
		TTransform& operator+=(const TTransform& Atom) UE_STUB_BODY
		TTransform operator*(T Mult) const UE_STUB_BODY
		TTransform& operator*=(T Mult) UE_STUB_BODY
		TTransform operator*(const TTransform& Other) const UE_STUB_BODY
		void operator*=(const TTransform& Other) UE_STUB_BODY
		TTransform operator*(const TQuat<T>& Other) const UE_STUB_BODY
		void operator*=(const TQuat<T>& Other) UE_STUB_BODY
		static bool AnyHasNegativeScale(const TVector<T>& InScale3D, const TVector<T>& InOtherScale3D) UE_STUB_BODY
		void ScaleTranslation(const TVector<T>& InScale3D) UE_STUB_BODY
		void ScaleTranslation(const T& Scale) UE_STUB_BODY
		void RemoveScaling(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		T GetMaximumAxisScale() const UE_STUB_BODY
		T GetMinimumAxisScale() const UE_STUB_BODY
		TTransform GetRelativeTransform(const TTransform& Other) const UE_STUB_BODY
		TTransform GetRelativeTransformReverse(const TTransform& Other) const UE_STUB_BODY
		void SetToRelativeTransform(const TTransform& ParentTransform) UE_STUB_BODY
		TVector4<T> TransformFVector4(const TVector4<T>& V) const UE_STUB_BODY
		TVector4<T> TransformFVector4NoScale(const TVector4<T>& V) const UE_STUB_BODY
		TVector<T> TransformPosition(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> TransformPositionNoScale(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> InverseTransformPosition(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> InverseTransformPositionNoScale(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> TransformVector(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> TransformVectorNoScale(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> InverseTransformVector(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> InverseTransformVectorNoScale(const TVector<T>& V) const UE_STUB_BODY
		TQuat<T> TransformRotation(const TQuat<T>& Q) const UE_STUB_BODY
		TQuat<T> InverseTransformRotation(const TQuat<T>& Q) const UE_STUB_BODY
		TTransform GetScaled(T Scale) const UE_STUB_BODY
		TTransform GetScaled(TVector<T> Scale) const UE_STUB_BODY
		TVector<T> GetScaledAxis(EAxis::Type InAxis) const UE_STUB_BODY
		TVector<T> GetUnitAxis(EAxis::Type InAxis) const UE_STUB_BODY
		void Mirror(EAxis::Type MirrorAxis, EAxis::Type FlipAxis) UE_STUB_BODY
		static TVector<T> GetSafeScaleReciprocal(const TVector<T>& InScale, T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		TVector<T> GetLocation() const UE_STUB_BODY
		TRotator<T> Rotator() const UE_STUB_BODY
		T GetDeterminant() const UE_STUB_BODY
		void SetLocation(const TVector<T>& Origin) UE_STUB_BODY
		bool ContainsNaN() const UE_STUB_BODY
		bool IsValid() const UE_STUB_BODY
		bool RotationEquals(const TTransform& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool TranslationEquals(const TTransform& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool Scale3DEquals(const TTransform& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool Equals(const TTransform& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool Identical(const TTransform* Other, uint32 PortFlags) const UE_STUB_BODY
		bool EqualsNoScale(const TTransform& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		static void Multiply(TTransform* OutTransform, const TTransform* A, const TTransform* B) UE_STUB_BODY
		void SetComponents(const TQuat<T>& InRotation, const TVector<T>& InTranslation, const TVector<T>& InScale3D) UE_STUB_BODY
		void SetIdentity() UE_STUB_BODY
		void SetIdentityZeroScale() UE_STUB_BODY
		void MultiplyScale3D(const TVector<T>& Scale3DMultiplier) UE_STUB_BODY
		void SetTranslation(const TVector<T>& NewTranslation) UE_STUB_BODY
		void CopyTranslation(const TTransform& Other) UE_STUB_BODY
		void ConcatenateRotation(const TQuat<T>& DeltaRotation) UE_STUB_BODY
		void AddToTranslation(const TVector<T>& DeltaTranslation) UE_STUB_BODY
		static TVector<T> AddTranslations(const TTransform& A, const TTransform& B) UE_STUB_BODY
		static TVector<T> SubtractTranslations(const TTransform& A, const TTransform& B) UE_STUB_BODY
		void SetRotation(const TQuat<T>& NewRotation) UE_STUB_BODY
		void CopyRotation(const TTransform& Other) UE_STUB_BODY
		void SetScale3D(const TVector<T>& NewScale3D) UE_STUB_BODY
		void CopyScale3D(const TTransform& Other) UE_STUB_BODY
		void SetTranslationAndScale3D(const TVector<T>& NewTranslation, const TVector<T>& NewScale3D) UE_STUB_BODY
		void Accumulate(const TTransform& SourceAtom) UE_STUB_BODY
		void NormalizeRotation() UE_STUB_BODY
		bool IsRotationNormalized() const UE_STUB_BODY
		TQuat<T> GetRotation() const UE_STUB_BODY
		TVector<T> GetTranslation() const UE_STUB_BODY
		TVector<T> GetScale3D() const UE_STUB_BODY
		void CopyRotationPart(const TTransform& SrcBA) UE_STUB_BODY
		void CopyTranslationAndScale3D(const TTransform& SrcBA) UE_STUB_BODY
		void SetFromMatrix(const TMatrix<T>& InMatrix) UE_STUB_BODY
	};
}
