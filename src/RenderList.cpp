#include "RenderList.h"

RenderList::RenderList() {



}

void RenderList::Add(Actor* actor) {



}

void RenderList::Add(Light* light) {



}

void RenderList::Clear() {

	for (auto& actorBatch : actorBatches) {
		actorBatch->ClearContent();
	}

	lights.clear();

}