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

// What info do we get from model loading?
// What other customization options do we need?
struct MeshInfo
{
	int posx, posy;
};

struct Material
{
	std::vector<char> vertexShader;
	std::vector<char> fragmentShader;

	// Might not be vec, might not be char, just a placeholder
	std::vector<char> params;
};
// Each material will have X children entity, each child being a mesh that will be rendered under the parent's vertex and fragment shaders with the parent's parameters.

// Creates new Entity
// Checks if entity exists
// Deletes existing entity
class registry {
public:
	EntityID createEntity();
	EntityID createMesh();
	EntityID createMaterial();
	bool exists(EntityID entity) const;
	void delEntity(EntityID entity);
	bool setParent(EntityID parent, EntityID child); // Sets a parent and child relationship between entities. Checks true/false
	EntityID getParent(EntityID child) const; // Gets the parent ID from the child entity
	std::vector<EntityID> getChildren(EntityID parent) const; // Gets all children of a parent entity
	void delParent(EntityID child); // Deletes the relationship between a parent and its child
	registry();
	void collectionSystem();

private:
	// Stores the next ID and the current Entities ID's
	//EntityID nextID = 1;
	//std::unordered_set<EntityID> currEntities;
	// Replaced with Flecs

	// Container for all ecs data
	flecs::world world;
};



//using Component = std::variant< _ >;