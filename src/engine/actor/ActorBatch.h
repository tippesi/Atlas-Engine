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
            ActorBatch(S object) : object(object) {}

            /**
             * Adds an actor to the batch
             * @param actor The actor to be added
             */
            inline void Add(T actor) {
                actors.push_back(actor);
            }

            /**
             * Removes an actor from the batch
             * @param actor The actor to be removed
             */
            inline void Remove(T actor) {
                for (auto iterator = actors.begin(); iterator != actors.end(); iterator++) {
                    if (*iterator == actor) {
                        actors.erase(iterator);
                        return;
                    }
                }
            }

            /**
             * Returns the number of actors in the batch.
             * @return An integer with the number of actors
             */
            inline int32_t GetSize() {
                return int32_t(actors.size());
            }

            /**
             * Returns the object that was used to create the batch
             * @return The object that was used to create the batch
             */
            inline S GetObject() {
                return object;
            }

            /**
             * Removes all actors from the batch.
             */
            inline void Clear() {
                actors.clear();
            }

            std::vector<T> actors;

        private:
            const S object;

        };

    }

}

#endif
