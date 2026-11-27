#pragma once
// Source: https://github.com/SanderMertens/flecs.git
// Allows for fixed width integers types
#include <cstdint>

#include<unordered_set>

#include<variant>

#include <flecs.h>
// Dynamic array container
#include <vector>


// Creates an ID for an entity
// using EntityID = std::uint64_t; - replaced with flecs
using EntityID = flecs::entity_t;

// Creates new Entity
// Checks if entity exists
// Deletes existing entity
class registry {
public:
	EntityID createEntity();
	bool exists(EntityID entity) const;
	void delEntity(EntityID entity);
	bool setParent(EntityID parent, EntityID child); // Sets a parent and child relationship between entities. Checks true/false
	EntityID getParent(EntityID child) const; // Gets the parent ID from the child entity
	std::vector<EntityID> getChildren(EntityID parent) const; // Gets all children of a parent entity
	void delParent(EntityID child); // Deletes the relationship between a parent and its child

private:
	// Stores the next ID and the current Entities ID's
	//EntityID nextID = 1;
	//std::unordered_set<EntityID> currEntities;
	// Replaced with Flecs

	// Container for all ecs data
	flecs::world world;
};



//using Component = std::variant< _ >;