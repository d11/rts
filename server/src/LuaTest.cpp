/*
 * =====================================================================================
 *
 *       Filename:  LuaTest.cpp
 *    Description:  
 *         Author:  Dan Horgan (danhgn), danhgn@googlemail.com
 *
 * =====================================================================================
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const int threadNum = 5;

vector< lua_State * > luaStates;

// TODO
//
// call sethook on a timer ?

void counthook( lua_State * threadState, lua_Debug *ar ) {
   lua_yield( threadState, 0 );
}

int main( int argc, char *argv[] )
{
   cout << "hi" << endl;

   lua_State * mainState = lua_open();   /* opens Lua */
   luaL_openlibs( mainState );

   lua_newtable( mainState ); // table of threads

   int error;
   for( int i = 0; i < threadNum ; ++ i)
   {
      // Create new thread and add to a big table of threads
      cout << "setting up " << i << endl;
      lua_pushnumber( mainState, i );
      lua_State * threadState = lua_newthread( mainState );
      luaStates.push_back( threadState );
      lua_settable( mainState, -3 );

      int instructionCount = 30;
      int res = lua_sethook ( threadState, counthook, LUA_MASKCOUNT, instructionCount );
      cout << "hook set: " << res << endl;

      stringstream stre;
      stre << "local k = 0" << endl
           << "while 1 do" << endl
           << "   print( \"" << char(65+i) << " \" .. tostring(k) )" << endl
           << "   k = k + 1" << endl
           << "end" << endl;
      string program( stre.str() );

      error = luaL_loadbuffer( threadState, program.c_str(), program.length(), "line");

//   while (fgets(buff, sizeof(buff), stdin) != NULL) {
   //error = luaL_loadbuffer(L, s.c_str(), s.length(), "line") || lua_pcall(L, 0, 0, 0);
   /*if (error) {
      cerr << lua_tostring(L, -1);
      lua_pop(L, 1);  
   }*/

   }

   while ( 1 )
   {

      // cycle through all threads once
      vector< lua_State *>::iterator threadIter;
      for ( threadIter = luaStates.begin() ; threadIter != luaStates.end() ; ++threadIter )
      {
         int resume = lua_resume( *threadIter, 0 );
         if ( resume == LUA_YIELD )
         {

         }
         else if ( resume == 0 ) // ended successfully
         {

         }
         else // error code...
         {

         }
      }

      cout << endl;
   }

   //error = luaL_loadbuffer(L, s.c_str(), s.length(), "line")
//   while (fgets(buff, sizeof(buff), stdin) != NULL) {
   //error = luaL_loadbuffer(L, s.c_str(), s.length(), "line") || lua_pcall(L, 0, 0, 0);
   /*if (error) {
      cerr << lua_tostring(L, -1);
      lua_pop(L, 1);  
   }*/

//   }

   lua_close( mainState );

   return 0;
}
