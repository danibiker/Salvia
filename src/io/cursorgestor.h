#pragma once

#include <const/Constant.h>
#include <SDL.h>

class CursorGestor{
    public :
        CursorGestor();
        ~CursorGestor();

        SDL_Cursor * getCursor(int i){
            return arrayCursors.Cursor[i];
        }

    private:
        void cargarCursores();
        SDL_Cursor * init_system_cursor(const char *image[]);

        struct my_struct_arrays {
          SDL_Cursor *Cursor[totalCursors];
          int numCursors;
        } arrayCursors;
};

