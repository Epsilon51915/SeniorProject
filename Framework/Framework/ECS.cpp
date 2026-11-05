#include "ECS.h"

// Creates a new entity assigning it an ID
// Increment for next entry
// Store new entity
EntityID registry::createEntity() {
	EntityID entity = nextID++;
	currEntities.insert(entity);
	return entity;

}