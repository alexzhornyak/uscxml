/**
 *  @file
 *  @author     2016 Stefan Radomski (stefan.radomski@cs.tu-darmstadt.de)
 *  @copyright  Simplified BSD
 *
 *  @cond
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the FreeBSD license as published by the FreeBSD
 *  project.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the FreeBSD license along with this
 *  program. If not, see <http://www.opensource.org/licenses/bsd-license>.
 *  @endcond
 */

#include "EventQueueImpl.h"
#include <event2/util.h>                // for evutil_socket_t
#include <event2/thread.h>
#include <assert.h>

#include <easylogging++.h>

namespace uscxml {

EventQueueImpl::EventQueueImpl() {
}
EventQueueImpl::~EventQueueImpl() {
}

Event EventQueueImpl::dequeue(bool blocking) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	if (blocking) {
		while (_queue.empty()) {
			_cond.wait(_mutex);
		}
	}
	if (_queue.size() > 0) {
		Event event = _queue.front();
		_queue.pop_front();
//        LOG(ERROR) << event.name;
		return event;
	}
	return Event();

}

void EventQueueImpl::enqueue(const Event& event) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	_queue.push_back(event);
	_cond.notify_all();
}

static void dummyCallback(evutil_socket_t fd, short what, void *arg) {
	timeval tv;
	tv.tv_sec = 365 * 24 * 3600;
	tv.tv_usec = 0;
	event *ev = *(event **)arg;
	evtimer_add(ev, &tv);
}

DelayedEventQueueImpl::DelayedEventQueueImpl(DelayedEventQueueCallbacks* callbacks) {
	_callbacks = callbacks;
#ifndef _WIN32
	evthread_use_pthreads();
#else
	evthread_use_windows_threads();
#endif
	_eventLoop = event_base_new();

	// see here: https://github.com/named-data/ndn.cxx/blob/master/scheduler/scheduler.cc
	// and here: https://www.mail-archive.com/libevent-users@seul.org/msg01676.html
	timeval tv;
	tv.tv_sec = 365 * 24 * 3600;
	tv.tv_usec = 0;
	_dummyEvent = evtimer_new(_eventLoop, dummyCallback, &_dummyEvent);
	evtimer_add(_dummyEvent, &tv);

	_thread = NULL;
	_isStarted = false;
	start();
}

DelayedEventQueueImpl::~DelayedEventQueueImpl() {
	stop();
	evtimer_del(_dummyEvent);
	event_free(_dummyEvent);
	event_base_free(_eventLoop);
}

void DelayedEventQueueImpl::timerCallback(evutil_socket_t fd, short what, void *arg) {
	struct callbackData *data = (struct callbackData*)arg;
	std::lock_guard<std::recursive_mutex> lock(data->eventQueue->_mutex);

	if (data->eventQueue->_callbackData.find(data->eventUUID) == data->eventQueue->_callbackData.end())
		return;

	event_free(data->event);
	data->eventQueue->_callbacks->eventReady(data->userData, data->eventUUID);
	data->eventQueue->_callbackData.erase(data->eventUUID);
}

void DelayedEventQueueImpl::enqueueDelayed(const Event& event, size_t delayMs, const std::string& eventUUID) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	if(_callbackData.find(eventUUID) != _callbackData.end()) {
		cancelDelayed(eventUUID);
	}

	_callbackData[eventUUID].eventUUID = eventUUID;
	_callbackData[eventUUID].userData = event;
	_callbackData[eventUUID].eventQueue = this;

	struct timeval delay = {static_cast<int32_t>(delayMs / 1000), static_cast<int32_t>((delayMs % 1000) * 1000)};
	struct event* e = event_new(_eventLoop, -1, 0, timerCallback, &_callbackData[eventUUID]);

	_callbackData[eventUUID].event = e;

	event_add(e, &delay);
}

void DelayedEventQueueImpl::cancelAllDelayed() {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	while(_callbackData.size() > 0) {
		std::pair<std::string, callbackData> item = *_callbackData.begin();
		Event data = item.second.userData;
		event_del(item.second.event);
		event_free(item.second.event);
		_callbackData.erase(item.first);
	}

}

void DelayedEventQueueImpl::cancelDelayed(const std::string& eventId) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	if(_callbackData.find(eventId) != _callbackData.end()) {
		event_del(_callbackData[eventId].event);
		event_free(_callbackData[eventId].event);
		_callbackData.erase(eventId);
	}
}

void DelayedEventQueueImpl::run(void* instance) {
	DelayedEventQueueImpl* INSTANCE = (DelayedEventQueueImpl*)instance;
	int result;
	while(INSTANCE->_isStarted) {
		/**
		 * EVLOOP_NO_EXIT_ON_EMPTY was removed in libevent2.1 - we are
		 * using the event in the far future approach to get blocking
		 * behavior back (see comments in contructor)
		 */

		// #ifndef EVLOOP_NO_EXIT_ON_EMPTY
//		    result = event_base_dispatch(INSTANCE->_eventLoop);
		// #else
		// TODO: this is polling when no events are enqueued
		result = event_base_loop(INSTANCE->_eventLoop, EVLOOP_ONCE);
//        assert(false); // NON-BLOCKING?!
		//#endif
		(void)result;
	}
}

void DelayedEventQueueImpl::start() {
	if (_isStarted) {
		return;
	}
	_isStarted = true;
	_thread = new std::thread(DelayedEventQueueImpl::run, this);
}

void DelayedEventQueueImpl::stop() {
	if (_isStarted) {
		_isStarted = false;
		event_base_loopbreak(_eventLoop);
		cancelAllDelayed();
	}
	if (_thread) {
		_thread->join();
		delete _thread;
		_thread = NULL;
	}
}

}