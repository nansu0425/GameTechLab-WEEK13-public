#pragma once

class UPrimitiveComponent;
class UPhysicalMaterial;
class FPhysScene;
struct FBodyInstance;
struct FConstraintInstance;
struct FShapeElem;

enum class EUserDataType : uint8
{
    Invalid,
    PrimitiveComponent,
    PhysicalMaterial,
    PhysScene,    
    BodyInstance,
    ConstraintInstance,
    AggShape,
    // 언리얼에서는 CustomPayload인데 우린 댕글링포인터 검출용
    Max
};

struct FUserData
{
    FUserData() = default;
    FUserData(UPrimitiveComponent* InPayload) : Type(EUserDataType::PrimitiveComponent), Payload(InPayload) {}
    FUserData(UPhysicalMaterial* InPayload) : Type(EUserDataType::PhysicalMaterial), Payload(InPayload) {}
    FUserData(FPhysScene* InPayload) : Type(EUserDataType::PhysScene), Payload(InPayload) {}
    FUserData(FBodyInstance* InPayload) : Type(EUserDataType::BodyInstance), Payload(InPayload) {}
    FUserData(FConstraintInstance* InPayload) : Type(EUserDataType::ConstraintInstance), Payload(InPayload) {}
    FUserData(FShapeElem* InPayload) : Type(EUserDataType::AggShape), Payload(InPayload) {}

    template <typename T>
    static T* Get(void* UserData)
    {
        if (!UserData)
        {
            return nullptr;
        }

        FUserData* Data = static_cast<FUserData*>(UserData);
        if (Data->Type != GetTypeFrom<T>())
        {
            return nullptr;
        }

        return static_cast<T*>(Data->Payload);
    }

    template <typename T>
    static void Set(void* UserData, T* Payload)
    {
        assert(UserData && "[FUserData/Set] UserData is nullptr");
        
        FUserData* Data = static_cast<FUserData*>(UserData);
        Data->Type = GetTypeFrom<T>();
        Data->Payload = Payload;
    }

    EUserDataType Type = EUserDataType::Invalid;
    void* Payload = nullptr;
    
private:
    template<typename T>
    static constexpr EUserDataType GetTypeFrom()
    {
        return EUserDataType::Invalid;
    }
};

template<>
inline constexpr EUserDataType FUserData::GetTypeFrom<UPrimitiveComponent>()
{
    return EUserDataType::PrimitiveComponent;
}

template<>
inline constexpr EUserDataType FUserData::GetTypeFrom<UPhysicalMaterial>()
{
    return EUserDataType::PhysicalMaterial;
}

template<>
inline constexpr EUserDataType FUserData::GetTypeFrom<FPhysScene>()
{
    return EUserDataType::PhysScene;
}

template<>
inline constexpr EUserDataType FUserData::GetTypeFrom<FBodyInstance>()
{
    return EUserDataType::BodyInstance;
}

template<>
inline constexpr EUserDataType FUserData::GetTypeFrom<FConstraintInstance>()
{
    return EUserDataType::ConstraintInstance;
}

template<>
inline constexpr EUserDataType FUserData::GetTypeFrom<FShapeElem>()
{
    return EUserDataType::AggShape;
}