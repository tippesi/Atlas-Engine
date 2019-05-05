#ifndef AE_EVENTDELEGATE_H
#define AE_EVENTDELEGATE_H

#include "../System.h"
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace Atlas {

	namespace Events {

		/**
         * Handles the event flow between subscriber and publisher.
         * @tparam Args Variable amount of parameters which can be exchanged with the delegate.
         */
		template<class ... Args>
		class EventDelegate {

		public:
			/**
             * Constructs a EventDelegate object
             */
			EventDelegate();

			/**
             * Subscribes a method/function to the event delegate which wants to receive the arguments specified in Args.
             * @param handle A method/function which has the required Args as parameters
             * @return A handle which can later be used to unsubscribe from the event delegate.
             * @note To subscribe a method of an object to the event delegate use std::bind().
			 * If the handle is invalidated, the object will be unsubscribed automatically.
             */
			uint32_t Subscribe(std::function<void(Args ...)> handle);

			/**
             * Unsubscribes a method/function from the event delegate.
             * @param subscriberID The handle which was returned previously at the time of subscription.
             */
			void Unsubscribe(uint32_t subscriberID);

			/**
             * Publishes Args arguments to the subscribers.
             * @param args The arguments which should be published
             */
			void Fire(Args ... args);

		private:
			std::map<uint32_t, std::function<void(Args ...)>> handler;
			uint32_t count;

			std::mutex mutex;

		};

		template<class... Args>
		EventDelegate<Args...>::EventDelegate() {
			count = 0;
		}

		template<class... Args>
		uint32_t EventDelegate<Args...>::Subscribe(std::function<void(Args ...)> handle) {

			std::lock_guard<std::mutex> guard(mutex);

			auto id = count++;
			handler[id] = handle;
			return id;

		}

		template<class... Args>
		void EventDelegate<Args...>::Unsubscribe(uint32_t subscriberID) {

			std::lock_guard<std::mutex> guard(mutex);

			if (handler.find(subscriberID) != handler.end()) {
				handler.erase(subscriberID);
			}

		}

		template<class... Args>
		void EventDelegate<Args...>::Fire(Args ... args) {

			std::unique_lock<std::mutex> lock(mutex);

			auto copy = std::vector<std::function<void(Args ...)>>();

			for (const auto &handleKey : handler) {
				copy.push_back(handleKey.second);
			}

			lock.unlock();

			for (auto &handle : copy) {
				handle(args ...);
			}

		}

	}

}

#endif