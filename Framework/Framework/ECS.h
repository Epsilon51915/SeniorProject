#pragma once
// Allows for fixed width integers types
#include <cstdint>

#include<unordered_set>

#include<variant>


// Creates an ID for an entity
using EntityID = std::uint64_t;


// Creates new Entity
// Checks if entity exists
// Deletes existing entity
class registry {
public:
	EntityID createEntity();
	bool exists(EntityID entity) const;
	void delEntity(EntityID entity);


private:
	// Stores the next ID and the current Entities ID's
	EntityID nextID = 1;
	std::unordered_set<EntityID> currEntities;
};

//using Component = std::variant< _ >;