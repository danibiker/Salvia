/* ScummVM - Graphic Adventure Engine
*
* ScummVM is the legal property of its developers, whose names
* are too numerous to list here. Please refer to the COPYRIGHT
* file distributed with this source distribution.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#ifndef TINSEL_NOTEBOOK_PAGE_H // prevent multiple includes
#define TINSEL_NOTEBOOK_PAGE_H

#include "common/scummsys.h"
#include "tinsel/anim.h"
#include "tinsel/tinsel.h"
#include "tinsel/object.h"

namespace Tinsel {

class NotebookLine {
public:
	NotebookLine() : _id(0), _crossedOut(false), _anim(), _obj(nullptr), _scribbles(nullptr), _scribbleAnim() {}
private:

public:
	int _id;
	void crossOut();
	void clear();
	void fillIn(int pageLine);
private:
	bool _crossedOut;
	ANIM _anim;
	OBJECT *_obj;
	OBJECT *_scribbles;
	ANIM _scribbleAnim;
};

class NotebookPage {
public:
	NotebookPage() : _pointedClue(-1), _lines(), _numLines(0) {}
private:

public:
	bool containsClue(int id);
	void crossClue(int id);
	void addLine(int id);
	void setTitle(int id);
	int32 getTitle() const;
	void fillIn();
	void clear();
	int getPointedClue(const Common::Point &point) const;
	int getClueForLine(int line) const;
	void handlePointAtLine(int line);
private:
	int indexOfClue(int id) const;

	int _pointedClue;
	const static uint32 MAX_ENTRIES_PER_PAGE = 8;
	NotebookLine _lines[MAX_ENTRIES_PER_PAGE];
	uint32 _numLines;
};

} // End of namespace Tinsel

#endif // SCUMMVM_NOTEBOOK_PAGE_H
