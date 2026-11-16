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
#include "common/events.h"

#include "common/system.h"

namespace Common {

bool isMouseEvent(const Event &event) {
	return event.type == EVENT_LBUTTONDOWN
	        || event.type == EVENT_LBUTTONUP
	        || event.type == EVENT_RBUTTONDOWN
	        || event.type == EVENT_RBUTTONUP
	        || event.type == EVENT_MBUTTONDOWN
	        || event.type == EVENT_MBUTTONUP
	        || event.type == EVENT_X1BUTTONDOWN
	        || event.type == EVENT_X1BUTTONUP
	        || event.type == EVENT_X2BUTTONDOWN
	        || event.type == EVENT_X2BUTTONUP
	        || event.type == EVENT_WHEELDOWN
	        || event.type == EVENT_WHEELUP
	        || event.type == EVENT_MOUSEMOVE;
}

EventSource::~EventSource() {}

EventObserver::~EventObserver() {}

EventMapper::~EventMapper() {}

EventManager::~EventManager() {}

EventDispatcher::EventDispatcher() {
}

EventDispatcher::~EventDispatcher() {
	for (auto rfIt55 = _mappers.begin(), rfEnd55 = _mappers.end(); rfIt55 != rfEnd55; ++rfIt55) {
		auto &mapper = *rfIt55;
		if (mapper.autoFree)
			delete mapper.mapper;
	}

	for (auto rfIt60 = _sources.begin(), rfEnd60 = _sources.end(); rfIt60 != rfEnd60; ++rfIt60) {
		auto &source = *rfIt60;
		if (source.autoFree)
			delete source.source;
	}

	for (auto rfIt65 = _observers.begin(), rfEnd65 = _observers.end(); rfIt65 != rfEnd65; ++rfIt65) {
		auto &observer = *rfIt65;
		if (observer.autoFree)
			delete observer.observer;
	}
}

void EventDispatcher::dispatch() {
	Event event;
	List<Event> mappedEvents;

	dispatchPoll();

	for (auto rfIt77 = _sources.begin(), rfEnd77 = _sources.end(); rfIt77 != rfEnd77; ++rfIt77) {
		auto &source = *rfIt77;
		if (source.ignore)
			continue;
		while (source.source->pollEvent(event)) {
			// We only try to process the events via the setup event mapper, when
			// we have a setup mapper and when the event source allows mapping.
			if (source.source->allowMapping()) {
				bool matchedAction = false;

				// Backends may not produce directly action event types, those are meant
				// to be the output of the event mapper.
				assert(event.type != EVENT_CUSTOM_BACKEND_ACTION_START);
				assert(event.type != EVENT_CUSTOM_BACKEND_ACTION_END);
				assert(event.type != EVENT_CUSTOM_ENGINE_ACTION_START);
				assert(event.type != EVENT_CUSTOM_ENGINE_ACTION_END);

				for (auto rfIt93 = _mappers.begin(), rfEnd93 = _mappers.end(); rfIt93 != rfEnd93; ++rfIt93) {
					auto &m = *rfIt93;
					if (!mappedEvents.empty())
						mappedEvents.clear();

					if (!m.mapper->mapEvent(event, mappedEvents))
						continue;

					for (auto rfIt100 = mappedEvents.begin(), rfEnd100 = mappedEvents.end(); rfIt100 != rfEnd100; ++rfIt100) {
						auto &mappedEvent = *rfIt100;
						dispatchEvent(mappedEvent);
					}

					matchedAction = true;
				}

				if (!matchedAction)
					dispatchEvent(event);
			} else {
				dispatchEvent(event);
			}
		}
	}
}

void EventDispatcher::clearEvents() {
	Event event;

	for (auto rfIt119 = _sources.begin(), rfEnd119 = _sources.end(); rfIt119 != rfEnd119; ++rfIt119) {
		auto &source = *rfIt119;
		if (source.ignore)
			continue;
		while (source.source->pollEvent(event)) {}
	}
}

void EventDispatcher::registerMapper(EventMapper *mapper, bool autoFree) {
	MapperEntry newEntry;

	newEntry.mapper = mapper;
	newEntry.autoFree = autoFree;
	newEntry.ignore = false;

	_mappers.push_back(newEntry);
}

void EventDispatcher::unregisterMapper(EventMapper *mapper) {
	for (List<MapperEntry>::iterator i = _mappers.begin(); i != _mappers.end(); ++i) {
		if (i->mapper == mapper) {
			if (i->autoFree)
				delete mapper;

			_mappers.erase(i);
			return;
		}
	}
}

void EventDispatcher::registerSource(EventSource *source, bool autoFree) {
	SourceEntry newEntry;

	newEntry.source = source;
	newEntry.autoFree = autoFree;
	newEntry.ignore = false;

	_sources.push_back(newEntry);
}

void EventDispatcher::unregisterSource(EventSource *source) {
	for (List<SourceEntry>::iterator i = _sources.begin(); i != _sources.end(); ++i) {
		if (i->source == source) {
			if (i->autoFree)
				delete source;

			_sources.erase(i);
			return;
		}
	}
}

void EventDispatcher::ignoreSources(bool ignore) {
	for (auto rfIt171 = _sources.begin(), rfEnd171 = _sources.end(); rfIt171 != rfEnd171; ++rfIt171) {
		auto &source = *rfIt171;
		source.ignore = ignore;
	}
}

void EventDispatcher::registerObserver(EventObserver *obs, uint priority, bool autoFree, bool notifyPoll) {
	ObserverEntry newEntry;

	newEntry.ignore = false;
	newEntry.observer = obs;
	newEntry.priority = priority;
	newEntry.autoFree = autoFree;
	newEntry.poll = notifyPoll;

	for (List<ObserverEntry>::iterator i = _observers.begin(); i != _observers.end(); ++i) {
		if (i->priority < priority) {
			_observers.insert(i, newEntry);
			return;
		}
	}

	_observers.push_back(newEntry);
}

void EventDispatcher::unregisterObserver(EventObserver *obs) {
	for (List<ObserverEntry>::iterator i = _observers.begin(); i != _observers.end(); ++i) {
		if (i->observer == obs) {
			if (i->autoFree)
				delete obs;

			_observers.erase(i);
			return;
		}
	}
}

void EventDispatcher::dispatchEvent(const Event &event) {
	for (auto rfIt208 = _observers.begin(), rfEnd208 = _observers.end(); rfIt208 != rfEnd208; ++rfIt208) {
		auto &observer = *rfIt208;
		if (observer.observer->notifyEvent(event))
			break;
	}
}

void EventDispatcher::dispatchPoll() {
	for (auto rfIt215 = _observers.begin(), rfEnd215 = _observers.end(); rfIt215 != rfEnd215; ++rfIt215) {
		auto &observer = *rfIt215;
		if (observer.poll)
			observer.observer->notifyPoll();
	}
}

class KeyboardRepeatEventSourceWrapper : public Common::EventSource {
public:
	KeyboardRepeatEventSourceWrapper(Common::EventSource *delegate) :
			_delegate(delegate),
			_keyRepeatTime(0) {
		assert(delegate);
	}

	// EventSource API
	bool pollEvent(Common::Event &event) override {
		uint32 time = g_system->getMillis(true);
		bool gotEvent = _delegate->pollEvent(event);

		if (gotEvent) {
			switch (event.type) {
			case Common::EVENT_KEYDOWN:
				// init continuous event stream
				_currentKeyDown = event.kbd;
				_keyRepeatTime = time + kKeyRepeatInitialDelay;
				break;

			case Common::EVENT_KEYUP:
				if (event.kbd.keycode == _currentKeyDown.keycode) {
					// Only stop firing events if it's the current key
					_currentKeyDown.keycode = Common::KEYCODE_INVALID;
				}
				break;

			default:
				break;
			}

			return true;
		} else {
			// Check if event should be sent again (keydown)
			if (_currentKeyDown.keycode != Common::KEYCODE_INVALID && _keyRepeatTime <= time) {
				// fire event
				event.type = Common::EVENT_KEYDOWN;
				event.kbdRepeat = true;
				event.kbd = _currentKeyDown;
				_keyRepeatTime = time + kKeyRepeatSustainDelay;

				return true;
			}

			return false;
		}
	}

	bool allowMapping() const override {
		return _delegate->allowMapping();
	}

private:
	// for continuous events (keyDown)
	enum {
		kKeyRepeatInitialDelay = 400,
		kKeyRepeatSustainDelay = 100
	};

	Common::EventSource *_delegate;

	Common::KeyState _currentKeyDown;
	uint32 _keyRepeatTime;
};

EventSource *makeKeyboardRepeatingEventSource(EventSource *eventSource) {
	if (!eventSource) {
		return nullptr;
	}

	return new KeyboardRepeatEventSourceWrapper(eventSource);
}

} // End of namespace Common
