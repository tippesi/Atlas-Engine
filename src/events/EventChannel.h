#ifndef EVENTCHANNEL_H
#define EVENTCHANNEL_H

#include "../System.h"

#include <mutex>
#include <unordered_map>

#define IMMEDIATE_PUBLICATION
#define BUFFERED_PUBLICATION

/**
 * A handle for an event subscription. Needed to unsubcribe from EventChannel.
 */
typedef struct EventSubscriberHandle {

	int32_t type;
	uint32_t ID;

}EventSubscriberHandle;

/**
 * Handles the event/message flow between subscriber and publisher.
 * @tparam T May be the event type or a class to be addressed in case of class-class communication.
 * @tparam Types Variable amount of parameters which can be exchanged with the channel (class-class communication)
 * @example
 * // In pseudo code
 * class A;
 * class B;
 * struct EventStruct;
 * // Normal event communication. A will receive the event structure
 * EventChannel<EventStruct>::Subscribe(A.method(EventStruct param));
 * EventChannel<EventStruct>::Publish(EventStruct event);
 * // Communication between to classes
 * EventChannel<A, string>::Subscribe(A.method(string param));
 * EventChannel<A, string>::Publish("Hello A");
 */
template<class T, class ... Args> class EventChannel {

public:
	/**
	 * Subcribes a method/function to the event channel which wants to receive the event T.
	 * @param subcriber A method/function which has the required T as a parameter
	 * @return A handle which can later be used to unsubscribe from the event channel
	 */
	static EventSubscriberHandle Subscribe(std::function<void(const T&)> subscriber) {

		lock_guard<mutex> guard(eventChannelMutex);

		EventSubscriberHandle handle;

		handle.ID = IDCount++;
		handle.type = eventSubscriberType;

		eventSubscriber[handle.ID] = subscriber;

		return handle;

	}

    /**
     * Subcribes a method/function to the event channel which wants to receive the
     * @param subcriber A method/function which has the required T and Types as parameters
     * @return A handle which can later be used to unsubscribe from the event channel
     */
	static EventSubscriberHandle Subscribe(std::function<void(Args ...)> subscriber) {

		lock_guard<mutex> guard(eventChannelMutex);

		EventSubscriberHandle handle;

		handle.ID = IDCount++;
		handle.type = argsSubscriberType;

		argsSubscriber[handle.ID] = subscriber;

		return handle;

	}

	/**
	 * Unsubscribes a method/function from the event channel
	 * @param handle The handle which was returned previously at the time of subscription
	 */
	static void UnSubscribe(EventSubscriberHandle handle) {

		lock_guard<mutex> guard(eventChannelMutex);

		switch (handle.type)
		{
		case eventSubscriberType: eventSubscriber.erase(handle.ID);
		case argsSubscriberType: argsSubscriber.erase(handle.ID);
		}

	}

	/**
	 * Publishes T to all subscribers which assigned for T (event communication)
	 * @param t The event data T
	 */
	static void Publish(const T& t) {

		lock_guard<mutex> guard(eventChannelMutex);

        for (const auto& handle : eventSubscriber)
        {
            handle.second(t);
        }

	}

	/**
	 * Publishes Args arguments to the subscribers which assigned for Args (class-class communication)
	 * @param args The arguments which should be published
	 */
	static void Publish(Args ... args) {

		lock_guard<mutex> guard(eventChannelMutex);

		for (const auto& handle : argsSubscriber)
		{
			handle.second(args ...);
		}

	}

	/**
	 * Changes the publication behaviour of the channel. The default is IMMEDIATE_PUBLICATION,
	 * which means that all published events get distributed immediately to the subscribers. The
	 * other behaviour would be BUFFERED_PUBLICATION, where all events get distributed after a
	 * PublishPublications() call.
	 * @param behaviour The method of publication the channel should use.
	 */
	static void SetPubicationBehaviour(int32_t behaviour) {

		lock_guard<mutex> guard(eventChannelMutex);

		publishBehaviour = behaviour;

	}

	/**
	 * Just relevant if the publication behaviour is set to BUFFERED_PUBLICATION. The default is
	 * IMMEDIATE_PUBLICATION. If this method is called all events will be distributed to the subscribers.
	 */
	static void PublishPublications() {

		lock_guard<mutex> guard(eventChannelMutex);

	}

private:
	static uint32_t IDCount;

	static uint32_t publishBehaviour;

	static const int32_t eventSubscriberType = 0;
	static const int32_t argsSubscriberType = 0;

	static std::mutex eventChannelMutex;

	static std::unordered_map<uint32_t, std::function<void(const T&)>> eventSubscriber;
	static std::unordered_map<uint32_t, std::function<void(Args ...)>> argsSubscriber;

};

template<class T, class ... Args>
uint32_t EventChannel<T, Args ...>::IDCount = 0;

template<class T, class ... Args>
uint32_t EventChannel<T, Args ...>::publishBehaviour = IMMEDIATE_PUBLICATION;

template<class T, class ... Args>
std::mutex EventChannel<T, Args ...>::eventChannelMutex;

template<class T, class ... Args>
std::unordered_map<uint32_t, std::function<void(const T&)>> EventChannel<T, Args ...>::eventSubscriber;

template<class T, class ... Args>
std::unordered_map<uint32_t, std::function<void(Args ...)>> EventChannel<T, Args ...>::argsSubscriber;

#endif