#include "pch.h"
#include "GPUTimer.h"

FGPUTimer::FGPUTimer()
{
}

FGPUTimer::~FGPUTimer()
{
	Release();
}

bool FGPUTimer::Initialize(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	Release(); // 기존 리소스 해제

	// 타임스탬프 쿼리 생성 (시작 시점)
	D3D11_QUERY_DESC QueryDesc = {};
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
	QueryDesc.MiscFlags = 0;

	HRESULT hr = Device->CreateQuery(&QueryDesc, &QueryBegin);
	if (FAILED(hr))
	{
		return false;
	}

	// 타임스탬프 쿼리 생성 (종료 시점)
	hr = Device->CreateQuery(&QueryDesc, &QueryEnd);
	if (FAILED(hr))
	{
		Release();
		return false;
	}

	// Disjoint 쿼리 생성 (주파수 및 유효성 확인용)
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	hr = Device->CreateQuery(&QueryDesc, &QueryDisjoint);
	if (FAILED(hr))
	{
		Release();
		return false;
	}

	bInitialized = true;
	return true;
}

void FGPUTimer::Begin(ID3D11DeviceContext* DeviceContext)
{
	if (!bInitialized || !DeviceContext)
	{
		return;
	}

	// Disjoint 쿼리 시작 (주파수 측정 시작)
	DeviceContext->Begin(QueryDisjoint);

	// 시작 타임스탬프 기록
	DeviceContext->End(QueryBegin);
}

void FGPUTimer::End(ID3D11DeviceContext* DeviceContext)
{
	if (!bInitialized || !DeviceContext)
	{
		return;
	}

	// 종료 타임스탬프 기록
	DeviceContext->End(QueryEnd);

	// Disjoint 쿼리 종료
	DeviceContext->End(QueryDisjoint);
}

float FGPUTimer::GetElapsedTimeMS(ID3D11DeviceContext* DeviceContext)
{
	if (!bInitialized || !DeviceContext)
	{
		return -1.0f;
	}

	// 비동기 쿼리 결과 대기 (논블로킹)
	UINT64 BeginTime = 0;
	UINT64 EndTime = 0;
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};

	// Disjoint 데이터가 준비될 때까지 대기
	HRESULT hr = DeviceContext->GetData(QueryDisjoint, &DisjointData, sizeof(DisjointData), 0);
	if (hr != S_OK)
	{
		// 데이터가 아직 준비되지 않음
		return LastElapsedMS; // 이전 값 반환
	}

	// GPU가 타임스탬프 측정 중 일시 중단되지 않았는지 확인
	if (DisjointData.Disjoint)
	{
		// 타임스탬프가 불연속적임 (신뢰할 수 없음)
		return LastElapsedMS;
	}

	// 시작/종료 타임스탬프 가져오기
	hr = DeviceContext->GetData(QueryBegin, &BeginTime, sizeof(UINT64), 0);
	if (hr != S_OK)
	{
		return LastElapsedMS;
	}

	hr = DeviceContext->GetData(QueryEnd, &EndTime, sizeof(UINT64), 0);
	if (hr != S_OK)
	{
		return LastElapsedMS;
	}

	// 타임스탬프 차이를 밀리초로 변환
	// 주파수: Ticks per second
	UINT64 Delta = EndTime - BeginTime;
	double FrequencyInverse = 1.0 / static_cast<double>(DisjointData.Frequency);
	double ElapsedSeconds = static_cast<double>(Delta) * FrequencyInverse;
	float ElapsedMS = static_cast<float>(ElapsedSeconds * 1000.0);

	// 캐시 업데이트
	LastElapsedMS = ElapsedMS;

	return ElapsedMS;
}

void FGPUTimer::Release()
{
	if (QueryBegin)
	{
		QueryBegin->Release();
		QueryBegin = nullptr;
	}

	if (QueryEnd)
	{
		QueryEnd->Release();
		QueryEnd = nullptr;
	}

	if (QueryDisjoint)
	{
		QueryDisjoint->Release();
		QueryDisjoint = nullptr;
	}

	bInitialized = false;
	LastElapsedMS = 0.0f;
}
