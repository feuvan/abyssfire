// abyss::ISaveStorage over UE's platform file (DECISIONS U1 / U2; save-ui-input.md 3.10; ue58-platform.md 10.5).
//
// * Files: <ProjectSavedDir>/SaveGames/abyssfire_slot{N}.json, N = 0..2 (3 character slots), plus settings.json.
//   The physical location is the platform's writable dir (Windows %LOCALAPPDATA%, macOS user Library, iOS app sandbox,
//   Android app storage) - never hard-code paths.
// * Write = write <file>.tmp -> flush -> keep the previous file as <file>.bak -> rename tmp over the main file.
//   Reading falls back to the .bak only when the caller asks (the GameInstance offers it after a parse failure).
// * Write() serialises nothing: it takes the JSON produced by GameSim::SaveGame on the game thread and queues the file
//   I/O on a worker task. Writes are chained, so they land in submission order; Read / List / Remove / WriteSync first
//   wait for the queue. Call Flush() before the app may be suspended or the storage is destroyed.
// * Thread safety: the public API is game-thread only; the worker tasks touch nothing but their own byte copy.
#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>
#include <vector>

#include "Tasks/Task.h"

#include "abyss/save/SaveIO.h"

class ABYSSFIRE_API FAbyssSaveStorage final : public abyss::ISaveStorage
{
public:
	/** DECISIONS U1: three character slots. */
	static constexpr int32 NumSlots = 3;

	FAbyssSaveStorage();
	virtual ~FAbyssSaveStorage() override;

	FAbyssSaveStorage(const FAbyssSaveStorage&) = delete;
	FAbyssSaveStorage& operator=(const FAbyssSaveStorage&) = delete;

	// ---- abyss::ISaveStorage ----
	/** Reads the main file of a slot. False when it does not exist or cannot be read. */
	virtual bool Read(int32_t Slot, std::string& Out) override;
	/** Queues an atomic write of the slot (async). True when queued. */
	virtual bool Write(int32_t Slot, std::string_view Bytes) override;
	/** Deletes the slot's main, .bak and .tmp files. */
	virtual bool Remove(int32_t Slot) override;
	/** One entry per slot (0..NumSlots-1): exists + SummarizeSave of the parsed main file (or its .bak when the main file is
	 *  unreadable / corrupt, so the menu still shows the character). */
	virtual void List(std::vector<abyss::SaveSlotInfo>& Out) override;

	// ---- extras ----
	/** Reads <slot>.json.bak (the previous good save). */
	bool ReadBackup(int32 Slot, std::string& Out);
	bool HasBackup(int32 Slot);
	/** Atomic write on the calling (game) thread, after the queue drained: app background / terminate / quit. */
	bool WriteSync(int32 Slot, std::string_view Bytes);
	/** Waits for every queued write. */
	void Flush();

	/** settings.json (device settings, FAbyssUserSettings). */
	bool ReadSettings(std::string& Out);
	bool WriteSettings(std::string_view Bytes);

	static bool IsValidSlot(int32 Slot) { return Slot >= 0 && Slot < NumSlots; }
	const FString& GetSaveDir() const { return SaveDir; }
	FString GetSlotPath(int32 Slot) const;

private:
	static bool ReadFileBytes(const FString& Path, std::string& Out);
	/** tmp -> flush -> .bak -> rename (any thread). */
	static bool WriteFileAtomic(const FString& Path, const std::string& Bytes);

	FString SaveDir;
	UE::Tasks::FTask LastWrite;
};
