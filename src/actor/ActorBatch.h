#ifndef AE_ACTORBATCH_H
#define AE_ACTORBATCH_H

#include "../System.h"

#include <vector>

namespace Atlas {

    namespace Actor {

        /**
         * Manages the ordering of actors for an object
         * @tparam S The type of the object, e.g. Mesh
         * @tparam T The type of the actors (instances), e.g. MeshActor
         */
        template <class S, class T> class ActorBatch {

        public:
            /**
             * Constructs an ActoBatch object.
             * @param object The object of which the batch should be created.
             */
            ActorBatch(S object);

            /**
             * Adds an actor to the batch
             * @param actor The actor to be added
             */
            void Add(T actor);

            /**
             * Removes an actor from the batch
             * @param actor The actor to be removed
             */
            void Remove(T actor);

            /**
             * Returns the number of actors in the batch.
             * @return An integer with the number of actors
             */
            int32_t GetSize();

            /**
             * Returns the object that was used to create the batch
             * @return The object that was used to create the batch
             */
            S GetObject();

            /**
             * Removes all actors from the batch.
             */
            void Clear();

            std::vector<T> actors;

        private:
            const S object;

        };

        template <class S, class T>
        ActorBatch<S, T>::ActorBatch(S object) : object(object) {



        }

        template <class S, class T>
        void ActorBatch<S, T>::Add(T actor) {

            actors.push_back(actor);

        }

        template <class S, class T>
        void ActorBatch<S, T>::Remove(T actor) {

            for (auto iterator = actors.begin(); iterator != actors.end(); iterator++) {
                if (*iterator == actor) {
                    actors.erase(iterator);
                    return;
                }
            }

        }

        template <class S, class T>
        int32_t ActorBatch<S, T>::GetSize() {

            return (int32_t)actors.size();

        }

        template <class S, class T>
        S ActorBatch<S, T>::GetObject() {

            return object;

        }

        template <class S, class T>
        void ActorBatch<S, T>::Clear() {

            actors.clear();

        }

    }

}

#endif
