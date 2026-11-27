// Source: https://github.com/SanderMertens/flecs.git
// auto - Automatically assigns the variable's type based on the value assigned.

#include "ECS.h"

// Creates a new entity assigning it an ID
// Increment for next entry
// Store new entity
//EntityID registry::createEntity() {
//	EntityID entity = nextID++;
//	currEntities.insert(entity);
//	return entity;

// }

// Create a new entity and get its ID.
EntityID registry::createEntity() 
{
	flecs::entity e = world.entity();
	return e.id();
}

// Check if entity is alive. True if entity is alive. False otherwise.
bool registry::exists(EntityID entity) const 
{
	return world.is_alive(entity);
}

// Delete an entity if its alive. Entities have to be deleted explicitly, and are not deleted when the entity object goes out of scope.
void registry::delEntity(EntityID entity) 
{
	if (exists(entity)) 
	{
		flecs::entity e = world.entity(entity);
		e.destruct();
	}
}

// Set an entity to be a parent of another entity. Checks True/false.
bool registry::setParent(EntityID parent, EntityID child)
{
	if (exists(parent) && exists(child))
	{
		flecs::entity p = world.entity(parent);
		world.entity(child).child_of(p);
		return true;
	}
	return false;
}

// Gets the parent's ID from its child entity.
EntityID registry::getParent(EntityID child) const
{
	if (exists(child))
	{
		flecs::entity c = world.entity(child);
		flecs::entity p = c.parent();
		return p.id();
	}
	return 0;
}

// Get all children associated with a parent.
// Flecs manages and stores the relationships. std::vector returns the children ID's collected by flecs.
std::vector<EntityID> registry::getChildren(EntityID parent) const
{
	std::vector<EntityID> children;

	if (exists(parent))
	{
		flecs::entity p = world.entity(parent);
		// Store each child's ID
		p.children([&](flecs::entity child)
		{
			children.push_back(child.id());
		});
	}
	return children;
}

// TODO Delete the relationship between a parent and its child
// TODO Check if an entity is a parent directly
// TODO Get all parent entities
// TODO Search for entities by name
// TODO implement components, add/remove
