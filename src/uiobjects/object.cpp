#include <uiobjects/object.h>

Object::~Object(){
}

/**
 *
 */
Object::Object(){
    classType = COBJECT;
    x = 0;
    y = 0;
    w = 0;
    h = 0;
}

/**
*
*/
void Object::draw(SDL_Surface *video_page){
    //To implement on the child
}

void Object::draw(SDL_Surface *video_page, GameTicks gameTicks){
    //To implement on the child
}