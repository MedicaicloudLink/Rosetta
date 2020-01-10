#utility::keys Namespace Reference

Keys and key-accessed containers for fast, safe object lookup.

Detailed Description
--------------------

The utility::keys package provides key classes and key-accessed containers. The keys are intended for use as named "indexes" for object lookup. Keys are like safer, more flexible enums when the integral value of the key doesn't have a meaning in the code. Keys have a hidden index value that only the containers can access, forcing application code to be written in terms of named keys instead of "magic" index numbers or unsafe enums. This makes it safe and fast to add new keys or change their order. Key class hierarchies add a capability that enums lack and the key classes hold additional identifier strings. Keys can also be added to key collections in separate source files, which is important for application "pluggability".

The hidden index allows fast vector-based containers instead of much slower map or hash based containers. Special containers are provided for fast vector-based lookup when the container holds only a subset of the possible keys.

Key is the root interface class for all the key types.

AutoKey sets a unique index for each key of an AutoKey type. AutoKey has template arguments for the type of object being key-accessed and a class that is considered the client/user of the keys, giving the developer fine-grained control over the key types and collections. AutoKey is intended for fixed collections of keys that are created sequentially one time: if you create AutoKeys in a loop the assigned indexes will eventually overflow.

UserKey is like AutoKey but the index can be set at the time of creation and then only accessed externally by friend class containers. UserKey is intended for key collections that may be created multiple times or for which the user needs to control the assigned indexes.

AutoKey and UserKey are abstract base classes: the application abstract and concrete key classes derived from them can specify the friend container classes that can access their hidden index and any special data or behavior they need.

VariantKey can be used to hold a key of any type derived from a base key type.

KeyTuple classes are provided for combining 2, 3, or 4 keys of any key types into a single meta-key. KeyVector classes are similar but for keys of the same type. Note that std::pair could play this role for 2 keys and boost::tuple for multiple keys but the KeyTuple and KeyVector classes provide a consistent, natural interface without creating a dependence on the Boost library.

Keys in a collection can be added to the associated KeyLookup class to provide a way to lookup keys by their identifiers and to iterate over the keys.

There are two types of containers provided that use the keys, those that share a common index map with all containers of that key, value, and client type and those for which each container has its own index map. These containers act a lot like hash maps except that they make exactly two fast index lookups for each user lookup by key, which gives higher performance than typical hash maps.

ClassKeyVector behaves like a vector that is indexed by its key type and for which all containers of that ClassKeyVector type share the same mapping from active keys to the inner vector. A client class template argument, in addition to the key and value type arguments, allows fine-grained control over the key collections that share an index map. The index map in ClassKeyVector is static so the size of the map (the highest index of any active key) and sparsity of the active keys is not generally a storage issue.

ClassKeyMap is like ClassKeyVector except that it stores a std::pair of the key and the mapped value for use when the key of each element is needed while iterating through the container.

SmallKeyVector behaves like a vector that is indexed by its key type where each SmallKeyVector has its own set of active keys and map from active keys to the inner vector. The index map is a member in each SmallKeyVector so it is not space-efficient for large key sets with sparse active key subsets. A hash map should be used instead when the storage use outweighs the lookup speed benefit of SmallKeyVector.

SmallKeyMap is like SmallKeyVector except that it stores a std::pair of the key and the mapped value for use when the key of each element is needed while iterating through the container.

##See Also

* [[Utility namespace|namespace-utility]]
  * [[utility::io|namespace-utility-io]]
  * [[utility::factory|namespace-utility-factory]] **NO LONGER EXISTS**
  * [[utility::options|namespace-utility-options]]
* [[Src index page]]: Description of Rosetta library structure and code layout in the src directory
* [[Rosetta directory structure|rosetta-library-structure]]: Descriptions of contents of the major subdirectories in the Rosetta `main` directory
* [[Glossary]]: Brief definitions of Rosetta terms
* [[RosettaEncyclopedia]]: Detailed descriptions of additional concepts in Rosetta.
* [[Rosetta overview]]: Overview of major concepts in Rosetta
* [[Development Documentation]]: The main development documentation page