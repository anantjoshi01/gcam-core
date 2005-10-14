#ifndef _HASH_MAP_H_
#define _HASH_MAP_H_
#if defined(_MSC_VER)
#pragma once
#endif

/*! 
* \file hash_map.h  
* \ingroup Objects
* \brief Header file for the HashMap class.
* \author Josh Lurz
* \date $Date$
* \version $Revision$
*/
#include <string>
#include <vector>
#include <memory>

#include "util/base/include/atom.h"
#include <boost/functional/hash/hash.hpp>
#include <boost/shared_ptr.hpp>

//! Turn on hash map tuning. This imposes a slight overhead.
#define TUNING_STATS 0

//! Set a default initial size for hashmaps.
#define DEFAULT_SIZE 23

#if TUNING_STATS
#include <iostream>
#endif
/*!
* \ingroup Objects
* \brief A templated object which implements a mapping of key to value using a
*        hash function to determine the position of a key in the internal
*        storage.
* \details The hashmap is a type of map which allows fast access to values by
*          using a hash function. A hash function is a function which converts a
*          key into a pseudo-random value distributed over the range of the
*          internal storage array. If the hash function converts two distinct
*          keys into the same value, a collision occurs which the map must
*          handle. The hash map is implemented as a list of buckets, accessed by
*          an integer index. Each bucket can contain a single key-value pairing,
*          or a chain of pairings implemented as a linked list. If there are no
*          collisions, then each bucket only contains a single key-value pair.
*          When a collision does occur, the collided values are placed in the
*          bucket's chain. As a result of this design, access to a value given a
*          key can occur in constant time unless there is a collision on the
*          key. To minimize collisions, the hashmap automatically increases its
*          size to prevent the map from reaching over 40 percent of its
*          capacity.
* \note This is not currently a complete map implementation, it only allows for
*       getting and setting individual values. There is currently not a way to
*       iterate over the key-value set or remove keys from the map.
* \note The Value type is required to implement the no-argument constructor.
*       This condition must be true for standard library containers as well.
* \note Do not use auto_ptrs as Values as they may be accidentally deleted during
*       resizes. This is true of standard library containers as well.
* \author Josh Lurz
*/

template <class Key, class Value>
class HashMap {
public:
	// Forward declare the Item so the interface is easier to read.
	struct Item;

private:
    //! Typedef which defines the type of a pair containing a const pointer to
    //! an Item and it's position in the bucket vector.
    typedef std::pair<const Item*, size_t> ConstItemPair;

    //! Typedef which defines the type of a pair containing pointer to an Item
    //! and it's position in the bucket vector.
    typedef std::pair<Item*,size_t> ItemPair;
public:
	/*! \brief Mutable iterator to a HashMap.
	*/
	class iterator {
		// Needed for the copy-constructor work-around.
		friend class const_iterator;
	public:
		explicit iterator( Item* aCurrentItem,
                           const size_t aBucketPosition,
                           const HashMap* aParent );

		bool operator==( const iterator& aOther ) const;
		bool operator!=( const iterator& aOther ) const;
		std::pair<Key, Value>* operator->();
		std::pair<Key, Value>& operator*();
        iterator& operator++();    // prefix ++
        iterator operator++(int); // postfix ++
	private:
        //! The current item at which the iterator is pointing and it's bucket
        //! spot.
        ItemPair mCurrentItem;

        //! Pointer to the parent hashmap of the iterator.
        const HashMap* mParent;
	};
	
	/*! \brief Constant iterator to a HashMap. */
	class const_iterator {
	public:
		explicit const_iterator( const Item* aCurrentItem,
                                 const size_t aBucketPosition,
                                 const HashMap* aParent );

		// Visual studio tries to use the non-const version of constant
        // functions even when the return types do not match. This copy
		// constructor allows the non-constant version of functions returning
        // iterators to automatically convert their return values into
		// constant iterators.
		const_iterator( iterator& aOther );
		bool operator==( const const_iterator& aOther ) const;
		bool operator!=( const const_iterator& aOther ) const;
		const std::pair<Key, Value>* operator->() const;
		const std::pair<Key, Value>& operator*() const;
        const_iterator& operator++();    // prefix ++
        const_iterator operator++(int); // postfix ++
	private:
        //! The current item at which the iterator is pointing and it's bucket
        //! spot.
        ConstItemPair mCurrentItem;

        //! Pointer to the parent hashmap of the iterator.
        const HashMap* mParent;
	};

	explicit HashMap( const size_t aSize = DEFAULT_SIZE );
	~HashMap();
    bool empty() const;
    size_t size() const;
	std::pair<iterator, bool> insert( const std::pair<Key, Value> aKeyValuePair );
	iterator find( const Key& aKey );
	const_iterator find( const Key& aKey ) const;
	iterator begin();
	const_iterator begin() const;
	iterator end();
	const_iterator end() const;
private:
	void resize( const size_t aNewSize );
    
    ConstItemPair getFirstItem() const;
    ConstItemPair getNextItem( const ConstItemPair& aItemPair ) const;
    
    ItemPair getFirstItem();
    ItemPair getNextItem( const ItemPair& aItemPair );

	//! The internal storage for the buckets.
	std::vector<boost::shared_ptr<Item> > mBuckets;

	//! Current size of the storage vector, which is greater than the number of
    //! values.
	size_t mSize;

	//! Number of entries.
	size_t mNumEntries;

	//! The hashmap's hash function
	boost::hash<Key> mHashFunction;

	/*! \brief An item stores a key value pairing together.
	* \details The Item structure stores the key value pairing, along with a
	*          pointer to the next item in the bucket to allow chaining.
    */
	struct Item {
		inline Item( const std::pair<Key, Value>& aKeyValuePair );
		/*! \brief The key value pair.
        */
		std::pair<Key, Value> mKeyValuePair;

		/*! \brief A pointer to the next Item in the bucket, null if it is the
		*          last item in the bucket.
		* \note The use of a smart pointer allows for an entire chain to be
        *       deleted by deleting the head of the chain, which induces a
        *       cascading deletion.
        */
		boost::shared_ptr<Item> mNext;
	};

#if( TUNING_STATS )
	//! Number of collisions if TUNING_STATS is on.
	unsigned int mNumCollisions;

	//! Number of resizes if TUNING_STATS is on.
	unsigned int mNumResizes;
#endif
};

/*! \brief Constructor
* \details Construct a hashmap with a specified size.
* \param aSize The initial size of the map. The map may grow from this size if
*        enough entries are added.
*/
template <class Key, class Value>
HashMap<Key, Value>::HashMap( const unsigned int aSize ):
mBuckets( aSize ), mSize( aSize ), mNumEntries( 0 )
#if( TUNING_STATS )
, mNumCollisions( 0 ),
mNumResizes( 0 )
#endif
{
}

/*! \brief Destructor
* \details All memory deallocation is performed by the smart pointers, the
*          destructor is only responsible for printing hash map statistics(if
*          TUNING_STATS is compiled on).
* \warning Deleting the map will not delete any allocated memory the user
*          specified as a value, which is congruent to how standard library
*          containers are implemented.
*/
template <class Key, class Value>
HashMap<Key, Value>::~HashMap(){
#if( TUNING_STATS )
	std::cout << "Hashmap stats - Size: " << static_cast<unsigned int>( mSize ) 
		<< " Number of entries: " << static_cast<unsigned int>( mNumEntries )
		<< " Collisions: " << mNumCollisions << " Percent full : " 
		<< static_cast<double>( mNumEntries ) / mSize * 100 
		<< " Number of resizes: " << mNumResizes << std::endl;
#endif
}

/*! \brief Return whether there are any items in the hashmap.
* \return Whether there are any items in the hashmap.
*/
template <class Key, class Value>
bool
HashMap<Key, Value>::empty() const {
    return mNumEntries == 0;
}

/*! \brief Return the number of items in the hashmap.
* \return The number of items in the hashmap.
*/
template <class Key, class Value>
size_t
HashMap<Key, Value>::size() const {
    return mNumEntries;
}

/*! \brief Insert a key-value pair to the map.
* \details This function takes a key value pairing and adds it to the hashmap.
*          To do this, it first calls the hash function on the key to determine
*          which bucket the item should reside in. If the bucket is currently
*          empty, the item is added as the first item in the bucket. Otherwise
*          the linked list is traversed to see if the item already exists. If it
*          does, the value is updated and the function will return true.
*          Otherwise an item is added to the end of the list for this key-value
*          pairing.
* \param aKeyValuePair The key value pair to add to the hashmap.
* \return A pair consisting of the iterator where the value was found and a bool
*         representing whether the insert was a new value.
*/
template <class Key, class Value>
std::pair<typename HashMap<Key, Value>::iterator, bool>
HashMap<Key, Value>::insert( const std::pair<Key, Value> aKeyValuePair ){
	// First find the hash value and use the modulo function to reduce it to
	// within the size of the bucket vector.
	const size_t bucketSpot = mHashFunction( aKeyValuePair.first ) % mSize;

	// We now know which bucket to either add the value to or update. Begin the
    // search.
	HashMap<Key, Value>::Item* curr = mBuckets[ bucketSpot ].get();
	HashMap<Key, Value>::Item* prev = 0;   

	// If the head of the chain is null, avoid attempting to search. Keep
	// searching until we find the correct Item or the end of the chain. Track
	// the previous position in the chain so that we can add to the end if the
	// key is not found.
	bool update = false;
	while( curr ){
		// Check if this is the correct spot.
		if( curr->mKeyValuePair.first == aKeyValuePair.first ){
			// Stop the search here as this is the postion to update.
			update = true;
			break;
		}

		// Move to the next item in the chain, which may be null.
		prev = curr;

		/*! \invariant Ensure that a pointer is not being assigned a reference
        *              to itself. 
        */
		assert( curr != curr->mNext.get() );

		curr = curr->mNext.get();
	}

	// If we found a spot to update, curr will not be null as the loop will have
	// been exited by the successful match. Update the value and return that the
	// value existed.
	if( update ){
		curr->mKeyValuePair.second = aKeyValuePair.second;
		return make_pair( iterator( curr, bucketSpot, this ), true );
	}

	// We are not updating, so a new value must be added.
	boost::shared_ptr<Item> newValue( new HashMap<Key, Value>::Item( aKeyValuePair ) );
	++mNumEntries;

	// Add the entry as the first item in the bucket if there is not a previous
    // pairing.
	if( !prev ){
		mBuckets[ bucketSpot ] = newValue;
	}
	else {
		/*! \invariant Ensure that a loop is not created. */
		assert( prev != newValue.get() );

		// Add the new item to the end of the chain.
		prev->mNext = newValue;
#if( TUNING_STATS )
		// Record the collision.
		++mNumCollisions;
#endif
	}
	// The ratio of entries to the maximum size of the map at which to increase
	// the map size. This is currently a low threshhold as the maps are adjusted
    // for performance not size. This may need to be adjusted for small maps.
	const double CAPACITY_THRESHHOLD = 0.4;

	// The multiple of the current size to which to set the new size.
	const unsigned int RESIZE_MULTIPLE = 3;

	// An additional size increment which helps performance for small maps where
    // resizing by the above factor would not be enough.
	const unsigned int ADDITIONAL_INCREMENT = 5;

	// Check if the size of the bucket vector should be increased for
	// performance. This should be done if the ratio of full to total buckets
	// exceeds CAPACITY_THRESHHOLD.
	if( static_cast<double>( mNumEntries ) / mSize > CAPACITY_THRESHHOLD ){
		// Resize to three times the number of current entries plus several
		// extra to handle small hashmaps. The bucket was empty, so add it as
		// the first value.
#if( TUNING_STATS )
		++mNumResizes;
#endif
		resize( mNumEntries * RESIZE_MULTIPLE + ADDITIONAL_INCREMENT );
	}
	// Return that an add and not an update occurred.
	return make_pair( iterator( newValue.get(), bucketSpot, this ), false );
}


/*! \brief Returns a mutable iterator for a given key.
* \details
* \param aKey Key for which to return the value.
* \return An iterator to the requested value or the end iterator if the key was not found.
*/
template <class Key, class Value>
typename HashMap<Key, Value>::iterator
HashMap<Key, Value>::find( const Key& aKey ){
	// First find the hash value and use the modulus function to reduce it to
	// within the size of the bucket vector.
	const size_t bucketSpot = mHashFunction( aKey ) % mSize;

	// We now know which bucket the key resides in if it exists.
	HashMap<Key, Value>::Item* curr = mBuckets[ bucketSpot ].get();

	// Keep searching until we find the correct Item or the end of the chain.
	while( curr ){
		// Check if this is the correct spot.
		if( curr->mKeyValuePair.first == aKey ){
			return iterator( curr, bucketSpot, this );
		}
		// Move to the next item in the chain, which may be null.
		curr = curr->mNext.get();
	}
	// Return the end iterator is the key was not found.
	return end();
}

/*! \brief Returns an immutable value for a given key.
* \details
* \param aKey Key for which to return the value.
* \return A constant iterator to the result or the end iterator if the key is
*         not found.
*/
template <class Key, class Value>
typename HashMap<Key, Value>::const_iterator
HashMap<Key, Value>::find( const Key& aKey ) const {
	// First find the hash value and use the mod function to reduce it to within
	// the size of the bucket vector.
	const size_t bucketSpot = mHashFunction( aKey ) % mSize;

	// We now know which bucket the key resides in if it exists.
	const HashMap<Key, Value>::Item* curr = mBuckets[ bucketSpot ].get();

	// Keep searching until we find the correct Item or the end of the chain.
	while( curr ){
		// Check if this is the correct spot.
		if( curr->mKeyValuePair.first == aKey ){
			return const_iterator( curr );
		}
		// Move to the next item in the chain, which may be null.
		curr = curr->mNext.get();
	}
	// Return the end iterator is the key was not found.
	return end();
}

/*! \brief Return the begin iterator.
* \details
* \return The begin iterator.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::iterator
HashMap<Key, Value>::begin() {
    ItemPair itemPair = getFirstItem();
	return iterator( itemPair.first, itemPair.second, this );
}

/*! \brief Return the constant begin iterator.
* \details
* \return The constant begin iterator.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::const_iterator
HashMap<Key, Value>::begin() const {
    CItemPair itemPair = getFirstItem();
	return const_iterator( itemPair.first, itemPair.second );
}

/*! \brief Return the end iterator.
* \details
* \return The end iterator.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::iterator
HashMap<Key, Value>::end() {
	return iterator( 0, 0, 0 );
}

/*! \brief Return the constant end iterator.
* \details
* \return The constant end iterator.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::const_iterator
HashMap<Key, Value>::end() const {
	return const_iterator( 0, 0 );
}

/*! \brief Resize the bucket vector. 
* \details To resize the vector, the function first walks the hashmap to store
*          the list of all items in the map. It then resizes the bucket vector
*          and reinserts all the values into the map.
* \warning This operation is very slow as it requires rehashing all entries.
* \param aNewSize New size of the bucket vector.
*/
template<class Key, class Value>
void HashMap<Key, Value>::resize( const unsigned int aNewSize ){
	// Check if the new and old size are the same to avoid resizing.
	if( aNewSize == mSize ){
		return;
	}

	// Walk the hashmap and create a list of all items in the tree. Create a
	// temporary vector to hold all the entries.
	std::vector<boost::shared_ptr<Item> > temporaryList;

	// Iterate over the buckets.
	for( unsigned int i = 0; i < mSize; ++i ){
		// Set a pointer to the first item in the bucket.
		boost::shared_ptr<Item> curr = mBuckets[ i ];
		// Add all items in the chain to the temporary list.
		while( curr.get() ){
			temporaryList.push_back( curr );
			
			/*! \invariant Ensure that no pointer refers to itself. */
			assert( curr.get() != curr->mNext.get() );

			// Move to the next item in the chain.
			curr = curr->mNext;
		}
	}
	// Check that the temporary list contains all entries.
	assert( temporaryList.size() == mNumEntries );

	// Resize the bucket vector.
	mSize = aNewSize;
	// Clear the bucket vector before resizing so all pointers are null, not
	// pointing at old values.
	mBuckets.clear();
	mBuckets.resize( mSize );

#if( TUNING_STATS )
	// Reset the collision count.
	mNumCollisions = 0;
#endif
	// Now add each item back to the list. Can't use the add function as we
	// already have allocated items and we don't want to change the count of the
	// number of entries.
	for( unsigned int i = 0; i < temporaryList.size(); ++i ){
		/*! \invariant Every position in the temporary list has a valid pointer. */
		assert( temporaryList[ i ].get() );
		
		/*! \invariant There is a second reference to the item pointed to by the
        *              next pointer. 
        */
		assert( !temporaryList[ i ]->mNext.unique() );
		
		// Clear out the next pointer as it is no longer valid. The temporary
		// list is holding a reference to all items in the list, so this cannot
        // cause a deallocation.
		temporaryList[ i ]->mNext.reset();

		// Add a single item to the map. Find the hash value and use the mod
        // function to reduce it to within the size of the bucket vector.
		const size_t bucketSpot = mHashFunction( temporaryList[ i ]->mKeyValuePair.first ) % mSize;

		// We now know which bucket the key resides in if it exists.
		HashMap<Key, Value>::Item* curr = mBuckets[ bucketSpot ].get();

		// If the spot is null, add this item as the head of the bucket's chain.
		if( !curr ){
			mBuckets[ bucketSpot ] = temporaryList[ i ];
		}
		// Otherwise we need to search for the end of the current bucket's
        // chain.
		else {
			while( curr->mNext.get() ){
				/*! \invariant Ensure that no pointer refers to itself. */
				assert( curr != curr->mNext.get() );
				
				curr = curr->mNext.get();
			}
			// Curr now points to the last item in the chain, add the current
			// item as the end of the chain.
			/*! \invariant Ensure that no pointer refers to itself. */
			assert( curr != temporaryList[ i ].get() );

			curr->mNext = temporaryList[ i ];
#if( TUNING_STATS )
			// Record the collision.
			++mNumCollisions;
#endif
		}
	}
}

/*! \brief Find the first item in the hashmap.
* \details This function is used to initialize the constant begin iterator. It
*          returns a pair containing a constant pointer to the first item and
*          the position of the item in the bucket vector.
* \return A pair containing a constant pointer to the first item and its
*         position in the bucket vector. If the vector is empty this will return
*         a pair containing null and zero.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::ConstItemPair
HashMap<Key, Value>::getFirstItem() const {
    // Check for empty hashmap first to avoid a slow unsuccessful search.
    if( empty() ){
        return ConstItemPair( 0, 0 );
    }

    // Search starting at the beginning of the bucket vector to find the first
    // item.
    for( unsigned int i = 0; i < mBuckets.size(); i++ ){
        if( mBuckets[ i ] ){
            return ConstItemPair( mBuckets[ i ], i );
        }
    }

    /*! \post An item must have been found because this function initially
    *         checks for an empty list. 
    */
    assert( false );
    
    // Make the compiler happy.
    return ConstItemPair( 0, 0 );
}

/*! \brief Return the next item in the hashmap.
* \details This function is used by the iterator to find the next value in the
*          hashmap.
* \param aItemPair A pair containing the current item and its bucket position.
* \return A pair containing the next item and its bucket position or the end
*         iterator if it was the last item.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::ConstItemPair
HashMap<Key, Value>::getNextItem( const ConstItemPair& aItemPair ) const {
    // First check if there is a next item in the current item's chain.
    if( aItemPair.first->mNext ){
        // Return a pair containing the next item and the current bucket spot
        // since the item is in the same chain.
        return ConstItemPair( aItemPair.first->mNext.get(), aItemPair.second );
    }

    // Otherwise search forward in the bucket vector starting at the current position.
    for( size_t i = aItemPair.second + 1; i < mBuckets.size(); ++i ){
        // If there is an item at this bucket position return it.
        if( mBuckets[ i ] ){
            return ConstItemPair( mBuckets[ i ].get(), i );
        }
    }

    // The end of the bucket vector was reached so there is not a next item.
    return ConstItemPair( 0, 0 );
}

/*! \brief Find the first item in the hashmap.
* \details This function is used to initialize the begin iterator. It returns a
*          pair containing a pointer to the first item and the position of the
*          item in the bucket vector.
* \return A pair containing a pointer to the first item and its position in the
*         bucket vector. If the vector is empty this will return a pair
*         containing null and zero.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::ItemPair HashMap<Key, Value>::getFirstItem() {
    // Check for empty hashmap first to avoid a slow unsuccessful search.
    if( empty() ){
        return ItemPair( 0, 0 );
    }

    // Search starting at the beginning of the bucket vector to find the first
    // item.
    for( unsigned int i = 0; i < mBuckets.size(); i++ ){
        if( mBuckets[ i ] ){
            return ItemPair( mBuckets[ i ].get(), i );
        }
    }

    /*! \post An item must have been found because this function initially
    *         checks for an empty list. 
    */
    assert( false );
    
    // Make the compiler happy.
    return ItemPair( 0, 0 );
}

/*! \brief Return the next item in the hashmap.
* \details This function is used by the iterator to find the next value in the
*          hashmap.
* \param aItemPair A pair containing the current item and its bucket position.
* \return A pair containing the next item and its bucket position or the end
*         iterator if it was the last item.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::ItemPair
HashMap<Key, Value>::getNextItem( const ItemPair& aItemPair ) {
    // First check if there is a next item in the current item's chain.
    if( aItemPair.first->mNext ){
        // Return a pair containing the next item and the current bucket spot
        // since the item is in the same chain.
        return ItemPair( aItemPair.first->mNext, aItemPair.second );
    }

    // Otherwise search forward in the bucket vector starting at the current position.
    for( size_t i = aItemPair.second + 1; i < mBuckets.size(); ++i ){
        // If there is an item at this bucket position return it.
        if( mBuckets[ i ] ){
            return ItemPair( mBuckets[ i ], i );
        }
    }

    // The end of the bucket vector was reached so there is not a next item.
    return end();
}

// ItemPair getNextItem( const ItemPair& aItemPair );
/*! \brief Item structure constructor.
* \param aKey The key.
* \param aValue The value.
*/
template<class Key, class Value>
HashMap<Key, Value>::Item::Item( const std::pair<Key, Value>& aKeyValuePair ):
mKeyValuePair( aKeyValuePair ){
}

/*! \brief iterator constructor.
* \param aCurrentItem The current Item.
* \param aBucketPosition The position of the Item in the bucket vector.
* \param aParent A pointer to the parent hashmap.
*/
template<class Key, class Value>
HashMap<Key, Value>::iterator::iterator( Item* aCurrentItem,
                                         const size_t aBucketPosition,
                                         const HashMap* aParent ):
mCurrentItem( aCurrentItem, aBucketPosition ),
mParent( aParent ){
}

/*! \brief Equals operator
* \param aKey The key.
* \param aValue The value.
*/
template<class Key, class Value>
bool HashMap<Key, Value>::iterator::operator ==( const typename HashMap<Key, Value>::iterator& aOther ) const {
	return ( mCurrentItem == aOther.mCurrentItem );
}

/*! \brief Not-equals operator
* \param aKey The key.
* \param aValue The value.
*/
template<class Key, class Value>
bool HashMap<Key, Value>::iterator::operator !=( const typename HashMap<Key, Value>::iterator& aOther ) const {
	return !( *this == aOther );
}

/*! \brief Pointer dereference operator
* \return A pointer to the key value pair.
*/
template<class Key, class Value>
std::pair<Key, Value>* HashMap<Key, Value>::iterator::operator->(){
	/*! \pre The current item pointer must be non-null. */
	assert( mCurrentItem.first != 0 );
	return &mCurrentItem.first->mKeyValuePair;
}

/*! \brief Dereference operator
* \return A reference to the key value pair.
*/
template<class Key, class Value>
std::pair<Key, Value>& HashMap<Key, Value>::iterator::operator*() {
	/*! \pre The current item pointer must be non-null. */
	assert( mCurrentItem.first != 0 );
	return mCurrentItem.first->mKeyValuePair;
}

/*! \brief Prefix increment operator.
* \return The incremented iterator.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::iterator&
HashMap<Key, Value>::iterator::operator++(){
    /*! \pre Need a non-null parent hashmap. */
    assert( mParent );

    mCurrentItem = mParent->findNext( mCurrentItem );
    return *this;
}

/*! \brief Postfix increment operator.
* \return The iterator before it is incremented.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::iterator
HashMap<Key, Value>::iterator::operator++ (int){
   iterator curr = *this;
   ++(*this);
   return curr;
 }

/*! \brief const_iterator constructor.
* \param aCurrentItem The current Item.
* \param aBucketPosition The position of the current Item within the bucket
*        vector.
*/
template<class Key, class Value>
HashMap<Key, Value>::const_iterator::const_iterator( const Item* aCurrentItem,
                                                     const size_t aBucketPosition,
                                                     const HashMap* aParent )
:mCurrentItem( aCurrentItem, aBucketPosition ),
mParent( aParent ){
}

/*! \brief Constructor which converts from a non-const interator to a constant
*          iterator.
* \details Visual studio tries to use the non-const version of constant
*          functions even when the return types do not match. This works around
*          the problem by converting the non-constant iterator return value of
*          the function to a constant iterator.
* \param aOther Mutable iterator from which to create a constant iterator.
*/
template<class Key, class Value>
HashMap<Key, Value>::const_iterator::const_iterator( iterator& aOther ):
mCurrentItem( aOther.mCurrentItem ),
mParent( aOther.mParent ){
}

/*! \brief Equals operator
* \param aKey The key.
* \param aValue The value.
*/
template<class Key, class Value>
bool HashMap<Key, Value>::const_iterator::operator ==( const typename HashMap<Key, Value>::const_iterator& aOther ) const {
	return ( mCurrentItem == aOther.mCurrentItem );
}

/*! \brief Not-equals operator
* \param aKey The key.
* \param aValue The value.
*/
template<class Key, class Value>
bool HashMap<Key, Value>::const_iterator::operator !=( const typename HashMap<Key, Value>::const_iterator& aOther ) const {
	return !( *this == aOther );
}

/*! \brief Pointer dereference operator
* \return A pointer to the key value pair.
*/
template<class Key, class Value>
const std::pair<Key, Value>* HashMap<Key, Value>::const_iterator::operator->() const {
	/*! \pre The current item pointer must be non-null. */
	assert( mCurrentItem.first != 0 );
	return &mCurrentItem.first->mKeyValuePair;
}

/*! \brief Prefix increment operator.
* \return The incremented iterator.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::const_iterator&
HashMap<Key, Value>::const_iterator::operator++(){
    /*! \pre Need a non-null parent hashmap. */
    assert( mParent );

    mCurrentItem = mParent->getNextItem( mCurrentItem );
    return *this;
}

/*! \brief Postfix increment operator.
* \return The iterator before it is incremented.
*/
template<class Key, class Value>
typename HashMap<Key, Value>::const_iterator
HashMap<Key, Value>::const_iterator::operator++ (int){
   const_iterator curr = *this;
   ++(*this);
   return curr;
 }

/*! \brief Dereference operator
* \return A reference to the key value pair.
*/
template<class Key, class Value>
const std::pair<Key, Value>& HashMap<Key, Value>::const_iterator::operator*() const {
	/*! \pre The current item pointer must be non-null. */
	assert( mCurrentItem.first != 0 );
	return mCurrentItem.first->mKeyValuePair;
}

#endif // _HASH_MAP_H_
