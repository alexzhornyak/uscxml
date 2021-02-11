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

#include "uscxml/Common.h"
#include "EventQueue.h"
#include "EventQueueImpl.h"
#include <string>
#include <map>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <event2/event.h>


namespace uscxml {

Event EventQueue::dequeue(size_t blockMs) {
	return _impl ? _impl->dequeue(blockMs) : Event();
}
void EventQueue::enqueue(const Event& event) {
	if (_impl)
		_impl->enqueue(event);
}
void EventQueue::reset() {
	if (_impl)
		_impl->reset();
}

Data EventQueue::serialize() {
	return _impl ? _impl->serialize() : Data();
}

void EventQueue::deserialize(const Data& data) {
	if (_impl)
		_impl->deserialize(data);
}

std::shared_ptr<EventQueueImpl> EventQueue::getImplBase() {
	return _impl;
}



PIMPL_OPERATORS_INHERIT_IMPL(DelayedEventQueue, EventQueue)

void DelayedEventQueue::enqueueDelayed(const Event& event, size_t delayMs, const std::string& eventUUID) {
	_impl->enqueueDelayed(event, delayMs, eventUUID);
}
void DelayedEventQueue::cancelDelayed(const std::string& eventUUID) {
	return _impl->cancelDelayed(eventUUID);
}

void DelayedEventQueue::cancelAllDelayed() {
	return _impl->cancelAllDelayed();
}

std::shared_ptr<DelayedEventQueueImpl> DelayedEventQueue::getImplDelayed() {
	return _impl;
}

}

