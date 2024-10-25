#include "..\include\lua\lua.hpp"
#include <stdio.h>

int main()
{
    printf("[+] Creating Lua State...\n");
    // Step 1: Create a new Lua state (interpreter)
    lua_State *L = luaL_newstate();
    if(!L)
    {
        printf("[!] Failed to allocate new state\n");
    }
    printf("[+] Lua state created!\n");
    

    // Step 2: Open Lua libraries (this loads Lua's standard library)
    luaL_openlibs(L);

    // Step 3: Load and execute a Lua script
    if (luaL_dofile(L, "script.lua")) {
        const char *errorMsg = lua_tostring(L, -1);
        printf("Error running Lua script: %s\n", errorMsg);
        lua_pop(L, 1);  // Remove the error message from the stack
    } else {
        printf("Lua script loaded successfully.\n");
    }

    // Step 4: Access a Lua variable from the script (get the value of 'number')
    lua_getglobal(L, "number");  // Push the global 'number' onto the stack
    if (lua_isnil(L, -1)) {
        printf("'number' is nil or undefined in Lua script.\n");
    } else if (lua_isnumber(L, -1)) {   // Check if it's a number
        double number = lua_tonumber(L, -1);  // Convert Lua number to C number
        printf("The value of 'number' from Lua: %.0f\n", number);
    } else {
        printf("Lua variable 'number' is not a number\n");
    }
    lua_pop(L, 1);  // Remove the 'number' from the stack

    // Step 5: Clean up and close the Lua state
    lua_close(L);

    return 0;
}
