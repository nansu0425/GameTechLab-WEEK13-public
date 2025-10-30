#include "pch.h"
#include "LuaScriptComponent.h"

#include <state.hpp>

IMPLEMENT_CLASS(ULuaScriptComponent)

BEGIN_PROPERTIES(ULuaScriptComponent)
	MARK_AS_COMPONENT("Lua 스크립트 컴포넌트", "Lua 스크립트 컴포넌트입니다.")
END_PROPERTIES()

ULuaScriptComponent::ULuaScriptComponent()
{
}

ULuaScriptComponent::~ULuaScriptComponent()
{
}

void ULuaScriptComponent::BeginPlay()
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	lua.new_usertype<FVector>("Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"x", &FVector::X,
		"y", &FVector::Y,
		"z", &FVector::Z,
		sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
		sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; }
	);

	lua.new_usertype<GameObject>("GameObject",
		"UUID", &GameObject::UUID,
		"Location", &GameObject::Location,
		"Velocity", &GameObject::Velocity,
		"PrintLocation", &GameObject::PrintLocation
	);

	GameObject obj;
	obj.Location = FVector(0, 0, 0);
	obj.Velocity = FVector(10, 0, 0);

	GameObject obj2;
	obj2.Location = FVector(0, 0, 0);
	obj2.Velocity = FVector(9, 9, 9);

	lua["obj"] = &obj;

	lua.script_file("Data/Scripts/template.lua");

	//lua.script(R"(
	//  function Tick(dt)
	//      obj.Location = obj.Location + obj.Velocity * dt
	//  end
	//)");

	lua["BeginPlay"]();

	// 테스트 루프
	for (int i = 0; i < 5; ++i) {
		lua["Tick"](0.1f); // delta time = 0.1
	}

	lua["OnOverlap"](obj2);

	lua["EndPlay"]();
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{

}

void ULuaScriptComponent::EndPlay(EEndPlayReason Reason)
{
}
