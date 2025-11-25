#pragma once

// 파티클 시스템 통계 데이터
struct FParticleStats
{
    int32 ParticleSystemCount = 0;  // 파티클 시스템 수
    int32 EmitterCount = 0;          // 이미터 수
    int32 ParticleCount = 0;         // 활성 파티클 수
    int32 SpawnedThisFrame = 0;      // 이번 프레임 생성 수
    int32 KilledThisFrame = 0;       // 이번 프레임 사망 수
    uint64 MemoryBytes = 0;          // 총 메모리 (바이트)

    void Reset() { *this = FParticleStats(); }
};

// 파티클 통계 매니저 (싱글톤)
class FParticleStatManager
{
public:
    static FParticleStatManager& GetInstance()
    {
        static FParticleStatManager Instance;
        return Instance;
    }

    void UpdateStats(const FParticleStats& InStats) { CurrentStats = InStats; }
    const FParticleStats& GetStats() const { return CurrentStats; }
    void ResetStats() { CurrentStats.Reset(); }

private:
    FParticleStatManager() = default;
    ~FParticleStatManager() = default;
    FParticleStatManager(const FParticleStatManager&) = delete;
    FParticleStatManager& operator=(const FParticleStatManager&) = delete;

    FParticleStats CurrentStats;
};
