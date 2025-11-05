#pragma once
#include "SceneComponent.h"

class USound;
struct IXAudio2SourceVoice;

class UAudioComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UAudioComponent, USceneComponent)
    GENERATED_REFLECTION_BODY();

    UAudioComponent();
    virtual ~UAudioComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual void EndPlay() override;

    void Play();
    void Stop();
    void PlaySlot(uint32 SlotIndex);

    // Convenience: set first slot
    void SetSound(USound* NewSound) { if (Sounds.IsEmpty()) Sounds.Add(NewSound); else Sounds[0] = NewSound; }

public:
    // Multiple sounds accessible by index
    TArray<USound*> Sounds;

    float Volume;
    float Pitch;
    bool  bIsLooping;
    bool  bAutoPlay;

    // Duplication
    virtual void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UAudioComponent)

protected:
    bool bIsPlaying;
    IXAudio2SourceVoice* SourceVoice = nullptr;
};
