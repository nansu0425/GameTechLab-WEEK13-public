#include "pch.h"
#include "LuaComponentProxy.h"

sol::object LuaComponentProxy::Index(sol::this_state LuaState, LuaComponentProxy& Self, const char* Key)
{
    if (!Self.Instance)
    {
        UE_LOG("[LuaProxy] Index: Instance is null for key '%s'\n", Key);
        return sol::nil;
    }

    sol::state_view LuaView(LuaState);

    // Get the registered Lua table for this class (contains both properties and methods)
    sol::table& BindTable = FLuaBindRegistry::Get().EnsureTable(LuaView, Self.Class);

    if (!BindTable.valid())
    {
        UE_LOG("[LuaProxy] Index: BindTable invalid for class %p, key '%s'\n",
            Self.Class, Key);
        return sol::nil;
    }

    sol::object Result = BindTable[Key];

    if (!Result.valid())
    {
        UE_LOG("[LuaProxy] Index: Key '%s' not found in BindTable for class %p\n",
            Key, Self.Class);
        return sol::nil;
    }

    // Check if it's a property descriptor table
    if (Result.is<sol::table>())
    {
        sol::table propDesc = Result.as<sol::table>();
        sol::optional<bool> isProperty = propDesc["is_property"];

        if (isProperty && *isProperty)
        {
            UE_LOG("[LuaProxy] Index: Property '%s' found for class %p\n",
                Key, Self.Class);

            // It's a property - get the getter function and call it
            sol::object getterObj = propDesc["get"];
            if (getterObj.valid())
            {
                sol::protected_function getter = getterObj.as<sol::protected_function>();
                auto pfr = getter(Self);

                if (!pfr.valid())
                {
                    sol::error err = pfr;
                    UE_LOG("[LuaProxy] Index: Property '%s' getter error: %s\n", Key, err.what());
                    return sol::nil;
                }

                UE_LOG("[LuaProxy] Index: Property '%s' getter succeeded\n", Key);
                // Get the first return value as sol::object
                return pfr.get<sol::object>();
            }
            else
            {
                UE_LOG("[LuaProxy] Index: Property '%s' has no getter\n", Key);
            }
            return sol::nil;
        }
    }

    // If it's a function, return it as-is (for methods)
    UE_LOG("[LuaProxy] Index: Returning method '%s' for class %p\n",
        Key, Self.Class);
    return Result;
}

void LuaComponentProxy::NewIndex(LuaComponentProxy& Self, const char* Key, sol::object Obj)
{
    if (!Self.Instance || !Self.Class) return;

    sol::state_view LuaView = Obj.lua_state();
    sol::table& BindTable = FLuaBindRegistry::Get().EnsureTable(LuaView, Self.Class);

    if (!BindTable.valid()) return;

    sol::object Property = BindTable[Key];

    if (!Property.valid())
        return; // Property doesn't exist

    // Check if it's a property descriptor table
    if (Property.is<sol::table>())
    {
        sol::table propDesc = Property.as<sol::table>();
        sol::optional<bool> isProperty = propDesc["is_property"];

        if (isProperty && *isProperty)
        {
            // Check if it's read-only
            sol::optional<bool> readOnly = propDesc["read_only"];
            if (readOnly && *readOnly)
            {
                UE_LOG("[LuaProxy] Attempted to set read-only property: %s\n", Key);
                return;
            }

            // It's a writable property - call the setter
            sol::optional<sol::function> setter = propDesc["set"];
            if (setter)
            {
                auto result = (*setter)(Self, Obj);
                if (!result.valid())
                {
                    sol::error err = result;
                    UE_LOG("[LuaProxy] Error setting property %s: %s\n", Key, err.what());
                }
            }
            return;
        }
    }

    // If it's not a property, do nothing (can't set methods)
}
