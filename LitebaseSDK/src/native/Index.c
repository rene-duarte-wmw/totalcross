/*********************************************************************************
 *  TotalCross Software Development Kit - Litebase                               *
 *  Copyright (C) 2000-2011 SuperWaba Ltda.                                      *
 *  All Rights Reserved                                                          *
 *                                                                               *
 *  This library and virtual machine is distributed in the hope that it will     *
 *  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                         *
 *                                                                               *
 *********************************************************************************/

/**
 * Defines functions to deal a B-Tree header. This has the implementation of a B-Tree. It is used to store the table indexes. It has some 
 * improvements for memory usage, disk space, and speed, targetting the creation of indexes, where the table's record is far greater than the index 
 * record.
 */

#include "Index.h"

/**
 * Creates a composed index.
 *
 * @param id The index id.
 * @param columns The columns of this index.
 * @param numberColumns The number of columns of this index.
 * @param heap The heap to allocate the compoded index structure.
 * @return The composed index.
 */
ComposedIndex* createComposedIndex(int32 id, uint8* columns, int32 numberColumns, Heap heap)
{
	TRACE("createComposedIndex")
   ComposedIndex* compIndex = (ComposedIndex*)TC_heapAlloc(heap, sizeof(ComposedIndex));
   compIndex->indexId = id;
   compIndex->columns = columns;
   compIndex->numberColumns = numberColumns;
   return compIndex;
}

/**
 * Constructs an index structure.
 *
 * @param context The thread context where the function is being executed.
 * @param table The table of the index.
 * @param keyTypes The types of the columns of the index.
 * @param colSizes The column sizes.
 * @param name The name of the index table.
 * @param numberColumns The number of columns of the index.
 * @param hasIndr Indicates if the index fas the .idr file.
 * @param exist Indicates that the index files already exist. 
 * @param heap A heap to allocate the index structure.
 * @return The index created or <code>null</code> if an error occurs.
 * @throws DriverException If is not possible to create the index files.
 */
Index* createIndex(Context context, Table* table, int32* keyTypes, int32* colSizes, CharP name, int32 numberColumns, bool hasIdr, bool exist, 
                                                                                                                                  Heap heap)
{
	TRACE("createIndex")
   Index* index = (Index*)TC_heapAlloc(heap, sizeof(Index));
   int32 keyRecSize = VALREC_SIZE,
         slot = table->slot;
   char buffer[DBNAME_SIZE];
   CharP sourcePath = table->sourcePath;
   XFile* fnodes = &index->fnodes;
   XFile* fvalues = &index->fvalues;

   index->heap = heap;
   index->tempKey.keys = (SQLValue*)TC_heapAlloc(heap,sizeof(SQLValue) * numberColumns);
   index->numberColumns = numberColumns;
   index->table = table;
   index->types = keyTypes;
   index->colSizes = colSizes;
   index->hasIdr = hasIdr;
   xstrcpy(index->name, name);

   while (--numberColumns >= 0) // Gets the key sizes for each column of the index.
      keyRecSize += typeSizes[keyTypes[numberColumns]];
   
	index->btreeMaxNodes = (SECTOR_SIZE - 5) / (keyRecSize + 2);

   // int size + key[k] + (Node = int)[k+1]
   index->nodeRecSize = 2 + index->btreeMaxNodes * (index->keyRecSize = keyRecSize) + ((index->btreeMaxNodes + 1) << 1); 
   
   index->basbuf = TC_heapAlloc(heap, index->nodeRecSize); // Creates the stream.
   index->basbufAux = TC_heapAlloc(heap, table->db->rowSize);
   index->cache = (Node**)TC_heapAlloc(heap, CACHE_SIZE * PTRSIZE); // Creates the cache. // juliana@230_32
   
// juliana@230_35: now the first level nodes of a b-tree index will be loaded in memory.
#ifndef PALMOS
   index->firstLevel = (Node**)TC_heapAlloc(heap, index->btreeMaxNodes * PTRSIZE); // Creates the first index level. 
#endif
   
   index->ancestors = newIntVector(null, 20, heap);
   
   // juliana@223_14: solved possible memory problems.
   // Creates the root node.
   index->root = createNode(index); 
   index->nodes = newIntVector(context, 10, heap); // A cache buffer used when climbing the index. // juliana@230_32 
   index->root->idx = 0;
   
   xstrcpy(buffer, name);
   xstrcat(buffer, IDK_EXT);
   if (!nfCreateFile(context, buffer, !exist, sourcePath, slot, fnodes, index->nodeRecSize << 1))
      return null;

   if (index->hasIdr)
   {
      buffer[xstrlen(buffer) - 1] = 'r';
		if (!nfCreateFile(context, buffer, !exist, sourcePath, slot, fvalues, CACHE_INITIAL_SIZE))
      {
   	   nfRemove(context, &index->fnodes, sourcePath, slot); // close node file which was open
         return null;
	   }
		fvalues->finalPos = fvalues->size; // juliana@211_2: Corrected a possible .idr corruption if it was used after a closeAll().
   }
   else
      fileInvalidate(fvalues->file);
   index->nodeCount = index->fnodes.size / index->nodeRecSize;

   if (index->fnodes.size)
      if (!nodeLoad(context, index->root))
      {
         nfClose(context, &index->fnodes);
         nfClose(context, &index->fvalues);
         return null;
      }
   return index;
}

/**
 * Creates an index.
 * 
 * @param context The thread context where the function is being executed.
 * @param table The table name whose index is to be created.
 * @param columnHashes The hashes of the index columns.
 * @param isPKCreation Indicates if the index to be created is the primary key.
 * @param indexCount The column numbers of the index.
 * @param composedPKCols The columns of the composed primary key.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 * @throws DriverException If a column for the index does not exist or is of type blob.
 * @throws SQLParseException If a column for the index is of type blob.
 */
bool driverCreateIndex(Context context, Table* table, int32* columnHashes, bool isPKCreation, int32 indexCount, uint8* composedPKCols)
{
	TRACE("driverCreateIndex")
   int32 idx = -1, 
         i, 
         saveType, 
         newIndexNumber,
         size = indexCount << 2;
   bool ret = true;
   Heap heap = heapCreate();
   PlainDB* plainDB = table->db;
   uint8* columns;
   int32* columnSizes;
   int32* columnTypes;

   IF_HEAP_ERROR(heap)
   {
      heapDestroy(heap);
      TC_throwExceptionNamed(context, "java.lang.OutOfMemoryError", null);
      return false;
   }
   
   columns = (uint8*)TC_heapAlloc(heap, indexCount);
   columnSizes = (int32*)TC_heapAlloc(heap, size);
   columnTypes = (int32*)TC_heapAlloc(heap, size);

   // juliana@226_4: now a table won't be marked as not closed properly if the application stops suddenly and the table was not modified since its 
   // last opening. 
   if (!table->isModified)
   {
      XFile* dbFile = &plainDB->db;
      
      i = (plainDB->isAscii? IS_ASCII : 0);
      nfSetPos(dbFile, 6);
      if (nfWriteBytes(context, dbFile, (uint8*)&i, 1) && flushCache(context, dbFile)) // Flushs .db.
         table->isModified = true;
      else
      {
         heapDestroy(heap);
         return false;
      }
   }

   i = indexCount;
   while (--i >= 0)
   {
		// juliana@222_3: Corrected a small issue that would make a DriverException not to be thrown when creating an index in a non-existing column on
      // Windows 32, Windows CE, Palm OS, iPhone, and Android.
      columns[i] = idx = (composedPKCols? composedPKCols[i] : TC_htGet32Inv(&table->htName2index, columnHashes[i]));
      if (idx < 0) // Column not found.
      {
         TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_COLUMN_NOT_FOUND), "");
         heapDestroy(heap);
         return false;
      }

      columnSizes[i] = table->columnSizes[idx];
      if ((columnTypes[i] = table->columnTypes[idx]) == BLOB_TYPE) // An index can't have a blob column.
      {
         TC_throwExceptionNamed(context, "litebase.SQLParseException", getMessage(ERR_BLOB_INDEX));
         table->primaryKeyCol = NO_PRIMARY_KEY;
         heapDestroy(heap);
         return false;
      }
   }

   newIndexNumber = verifyIfIndexAlreadyExists(context, table, columns, indexCount);

   if (indexCount == 1)
   {
      if (newIndexNumber < 0 || !indexCreateIndex(context, table, table->name, columns[0], columnSizes, columnTypes, false, false, heap))
      {
         heapDestroy(heap);
         return false; 
      }

      saveType = TSMD_ATLEAST_INDEXES;
   }
   else
   {
      if (newIndexNumber < 0 
  || !indexCreateComposedIndex(context, table, table->name, columns, columnSizes, columnTypes, indexCount, newIndexNumber, true, false, false, heap))
      {
         heapDestroy(heap);
         return false;
      }
      saveType = TSMD_EVERYTHING; 
   }

   // guich@555_4: changed from 1 to 0 because this is now a row count, not a record count.
   if (plainDB->rowCount > 0) // The header may be created but the table may still be empty 
   {
      // Catchs the PrimaryKeyViolation exception to drop the recreated index.
      if (indexCount == 1)
      {
         if (!tableReIndex(context, table, idx, isPKCreation, null))
         {
            driverDropIndex(context, table, idx);
            if (table->primaryKeyCol == idx) // juliana@114_9
               table->primaryKeyCol = NO_PRIMARY_KEY; // no return: we must save the metadata
            ret = false;
         }
      }
      else
      {
         i = (newIndexNumber < 0)? -newIndexNumber : newIndexNumber;
         if (isPKCreation) 
            table->composedPK = i - 1;
         if (!tableReIndex(context, table, -1, isPKCreation, table->composedIndexes[i - 1]))
         {
            driverDropComposedIndex(context, table, table->composedPrimaryKeyCols, table->numberComposedPKCols, i - 1, true);
            if (isPKCreation)
            {
               table->composedPK = NO_PRIMARY_KEY;
               table->numberComposedPKCols = 0;
            }
            ret = false;
         }
      }
   }
   return tableSaveMetaData(context, table, saveType) && ret; // guich@560_24: saves table meta data.
}

/**
 * Removes a value from the index.
 *
 * @param context The thread context where the function is being executed.
 * @param key The key to be removed.
 * @param value The repeated value index.
 * @return <code>true</code> If the value was removed; <code>false</code> otherwise.
 * @throws DriverException If its not possible to find the key record to delete or the index is corrupted.
 */
bool indexRemoveValue(Context context, Key* key, Val* value)
{
	TRACE("indexRemoveValue")
   Index* index = key->index;

   if (index->fnodes.size)
   {
      Node* curr = index->root; // 0 is always the root.
		int32 nodeCounter = index->nodeCount,
            pos,
            numberColumns = index->numberColumns;
      Key* keyFound;

      while (true)
      {
         keyFound = &curr->keys[pos = nodeFindIn(context, curr, key, false)]; // juliana@201_3 // Finds the key position.
         if (pos < curr->size && keyEquals(key, keyFound, numberColumns)) 
         {
            switch (keyRemove(context, keyFound, value)) // Tries to remove the key.
            {
               // It successfully removed the key.
               case REMOVE_SAVE_KEY:
                  if (!nodeSaveDirtyKey(context, curr, pos)) // no break!
                     return false;
               case REMOVE_VALUE_ALREADY_SAVED:
                  return true;
            }
            return false;
         }
         if (nodeIsLeaf(curr)) // If there are children, load them if the key was not found yet.
            break;

			if (--nodeCounter < 0) // juliana@220_16: does not let the index access enter in an infinite loop. 
			{
				TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
				return false;
			}
         if (!(curr = indexLoadNode(context, index, curr->children[pos])))
            return false;
      }
   }

   // Could not find the key record to be deleted.
   TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_IDX_RECORD_DEL));
   return false;
}

/**
 * Loads a node.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index.
 * @param idx The index of the value to be loaded.
 * @return The node or <code>null</code> in case of an error.
 * @throws DriverException If the index is corrupted.
 */
Node* indexLoadNode(Context context, Index* index, int32 idx)
{
	TRACE("indexLoadNode")
   int32 i = CACHE_SIZE;
   Node* cand;
   Node** nodes;
   
   if (!idx) // If the index is 0, return the root.
      return index->root;
   if (idx == LEAF) // If the node is a leaf, the index is corrupted.
   {
      TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
      return null;
   }
   
#ifndef PALMOS
   // Tries to find the node in the nodes of the first level.
   if (idx <= index->btreeMaxNodes)
   {
      if (!(cand = (nodes = index->firstLevel)[idx - 1]))
      {
         (cand = nodes[idx - 1] = createNode(index))->idx = idx;
         nodeLoad(context, cand);
      }
      return cand;
   }
#endif   
   
   // Loads the cache if the node is in a deeper level.
   nodes = index->cache;
   while (--i >= 0) // Loads the cache.
      if (nodes[i] && nodes[i]->idx == idx) 
         return nodes[index->cacheI = i];
   
   if (++index->cacheI >= CACHE_SIZE)
      index->cacheI = 0;

   IF_HEAP_ERROR(index->heap) // juliana@223_14: solved possible memory problems.
   {
      TC_throwExceptionNamed(context, "java.lang.OutOfMemoryError", null);
      return null;
   }
   
   // juliana@230_25: solved a bug with index with repeated keys which could not be built correctly.
	if (!(cand = nodes[index->cacheI]))
      cand = nodes[index->cacheI] = createNode(index);
   
   if (index->isWriteDelayed && cand->isDirty && nodeSave(context, cand, false, 0, cand->size) < 0) // Saves this one if it is dirty.
      return null;
   cand->idx = idx;
   
   // Loads the node.
   if (!nodeLoad(context, cand))
      return null;
   return cand;
}

/**
 * Finds the given key and make the monkey climb on the values.
 *
 * @param context The thread context where the function is being executed.
 * @param key The key to be found.
 * @param monkey A pointer to a monkey structure.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 * @throws DriverException If the index is corrupted.
 */
bool indexGetValue(Context context, Key* key, Monkey* monkey)
{
	TRACE("indexGetValue")
   Index* index = key->index;

   if (index->fnodes.size)
   {
      Node* curr = index->root; // 0 is always the root.
		int32 nodeCounter = index->nodeCount,
            numberColumns = index->numberColumns,
            pos;
      Key* keyFound;
      monkeyOnKeyFunc onKey = monkey->onKey;

      while (true) 
      {
         keyFound = &curr->keys[pos = nodeFindIn(context, curr, key, false)]; // juliana@201_3 // Finds the key position.
         if (pos < curr->size && keyEquals(key, keyFound, numberColumns)) 
         {
            if (onKey(context, keyFound, monkey) == -1)
               return false;
            break;
         }
         if (nodeIsLeaf(curr)) // If there are children, load them if the key was not found yet.
            break;
			if (--nodeCounter < 0) // juliana@220_16: does not let the index access enter in an infinite loop. 
			{
				TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
				return false;
			}
         if (!(curr = indexLoadNode(context, index,curr->children[pos])))
            return false;
      }
   }
   return true;
}

/**
 * Climbs on the nodes that are greater or equal than the current one.
 *
 * @param context The thread context where the function is being executed.
 * @param node The node to be compared with.
 * @param nodes A vector of nodes.
 * @param start The first key of the node to be searched.
 * @param monkey The monkey object.
 * @param stop Indicates when the climb process can be finished.
 * @return If it has to stop the climbing process or not, or <code>false</code> if an error occured.
 */
bool indexClimbGreaterOrEqual(Context context, Node* node, IntVector* nodes, int32 start, Monkey* monkey, bool* stop)
{
	TRACE("indexClimbGreaterOrEqual")
   int32 ret,
         size = node->size;
   int16* children = node->children;
   Key* keys = node->keys;
   Index* index = node->index;
   monkeyOnKeyFunc onKey = monkey->onKey;
   
   if (start >= 0)
   {
      *stop = !(ret = onKey(context, &keys[start], monkey));  
      if (ret == -1)
         return false;
   }
   if (nodeIsLeaf(node))
      while (!(*stop) && ++start < size)
      {
         *stop = !(ret = onKey(context, &keys[start], monkey)); 
         if (ret == -1)
            return false;
      }
   else
   {
		Node* curr;
		Node* loaded;

		if (nodes->size > 0) 
			curr = (Node*)IntVectorPop((*nodes));
		else 
         curr = createNode(index); // juliana@230_32: corrected a bug of inequality searches in big indices not returning all the results.

      while (!(*stop) && ++start <= size)
      {
         if (!(loaded = getLoadedNode(context, index, children[start])))
         {
            (loaded = curr)->idx = children[start];
            if (!nodeLoad(context, curr))
               return false;
         }
         if (!indexClimbGreaterOrEqual(context, loaded, nodes, -1, monkey, stop))
            return false;
         if (start < size && !(*stop))
         {
            *stop = !(ret = onKey(context, &node->keys[start], monkey)); 
            if (ret == -1)
               return false;
         }
      }
      IntVectorPush(context, nodes, (int32)curr);
   }
   return true;
}

/**
 * Starts from the root to find the left key, then climbs from it until the end.
 *
 * @param context The thread context where the function is being executed.
 * @param left The left key.
 * @param monkey The Monkey object.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 * @throws DriverException If the index is corrupted.
 * @throws OutOfMemoryError If there is not enougth memory allocate memory. 
 */
bool indexGetGreaterOrEqual(Context context, Key* left, Monkey* monkey)
{
	TRACE("indexGetGreaterOrEqual")
   Index* index = left->index;

   if (index->fnodes.size)
   {
      int32 pos,
            comp,
			   nodeCounter = index->nodeCount;
      IntVector intVector1 = newIntVector(context, 10, null);
      Node* curr = index->root; // Starts from the root.
     
      if (!intVector1.items)
      {
         TC_throwExceptionNamed(context, "java.lang.OutOfMemoryError", null);
         return false;
      }

      while (true)
      {
         if ((pos = nodeFindIn(context, curr, left, false)) < curr->size)  // juliana@201_3
         {
            // Compares left keys with curr keys. If this value is above or equal to the one being looked for, stores it.
            if ((comp = keyCompareTo(left, &curr->keys[pos], index->numberColumns)) <= 0) 
            {
               if (!IntVectorPush(context, &intVector1, curr->idx) || !IntVectorPush(context, &intVector1, pos))
               {
                  xfree(intVector1.items);
                  return false;
               }
            }
            if (comp >= 0) // left >= curr.keys[pos] ?
               break;
         }
         if (nodeIsLeaf(curr)) // If there are children, load them if the key was not found yet. 
            break;

			if (--nodeCounter < 0) // juliana@220_16: does not let the index access enter in an infinite loop. 
			{
				TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
				xfree(intVector1.items);
            return false;
			}
         if (!(curr = indexLoadNode(context, index, curr->children[pos])))
         {
            xfree(intVector1.items);
            return false;
         }
      }
      if (intVector1.size > 0)
      {
         bool stop;
         
         // juliana@230_32: corrected a bug of inequality searches in big indices not returning all the results.
         while (intVector1.size > 0)
         {
            stop = false;
            pos = IntVectorPop(intVector1);
            if (!(curr = indexLoadNode(context, index, IntVectorPop(intVector1))) 
             || !indexClimbGreaterOrEqual(context, curr, &index->nodes, pos, monkey, &stop))
            {
               xfree(intVector1.items);
               return false;
            }
            if (stop)
               break;
         }
      }
      xfree(intVector1.items);
   }
   return true;
}

/**
 * Splits the overflown node of this B-Tree. The stack ancestors contains all ancestors of the node, together with the known insertion position in 
 * each of these ancestors.
 *
 * @param context The thread context where the function is being executed.
 * @param curr The current node.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 */
bool indexSplitNode(Context context, Node* curr)
{
	TRACE("indexSplitNode")
   Key* med;
   Index* index = curr->index;
   Node* root = index->root;
   IntVector ancestors = index->ancestors;

   // guich@110_3: curr.size * 3/4 - note that medPos never changes, because the node is always split when the same size is reached.
   int32 medPos = index->isOrdered ? (curr->size - 1) : (curr->size / 2),

         btreeMaxNodes = index->btreeMaxNodes,
         left,
         right;
   
   while (curr)
   {
      med = &curr->keys[medPos];
      if ((right = nodeSave(context, curr, true, medPos + 1, curr->size)) < 0) // right sibling - must be the first one to save!
         return false;

      if (curr->idx) // guich@110_4: not the root? reuses this node; cut it at medPos.
      {
         left = curr->idx;
         curr->size = medPos;
         if (nodeSave(context, curr, false, 0, curr->size) < 0
          || !(curr = indexLoadNode(context, index, IntVectorPop(ancestors)))
          || !nodeInsert(context, curr, med, left, right, IntVectorPop(ancestors))) // Loads the parent.
            return false;
			if (curr->size < btreeMaxNodes) // Parent has not overflown?
            break;
      }
      else
      {
         if ((left = nodeSave(context, curr, true, 0, medPos)) < 0) // Left sibling.
            return false;
         nodeSet(root, med, left, right); // Replaces the root record.
         if (nodeSave(context, root, false, 0, root->size) < 0)
            return false;
         break;
      }
   }
   return true;
}

/**
 * Removes the index files.
 * 
 * @param context The thread context where the function is being executed.
 * @param index The index to be removed.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 */
bool indexRemove(Context context, Index* index)
{
	TRACE("indexRemove")
   Table* table = index->table;

   if (!nfRemove(context, &index->fnodes, table->sourcePath, table->slot)
   || (fileIsValid(index->fvalues.file) && !nfRemove(context, &index->fvalues, table->sourcePath, table->slot)))
      return false;
   
   fileInvalidate(index->fnodes.file);
   heapDestroy(index->heap);
   return true;
}

/**
 * Closes the index files.
 * 
 * @param context The thread context where the function is being executed.
 * @param index The index to be removed.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 */
bool indexClose(Context context, Index* index)
{
	TRACE("indexClose")
   int32 ret;
      
   index->fnodes.finalPos = index->nodeCount * index->nodeRecSize; // Calculated the used space; the file will have no zeros at the end. 
   ret = nfClose(context, &index->fnodes) && nfClose(context, &index->fvalues);
   heapDestroy(index->heap);
   return ret;
}

/**
 * Empties the index files, since the rows were deleted.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index to be erased.
 * @throws DriverException If it is not possible to truncate the index files.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 */
bool indexDeleteAllRows(Context context, Index* index)
{
	TRACE("indexDeleteAllRows")
   int32 i;
   Node** cache = index->cache;
   XFile* fnodes = &index->fnodes;
   XFile* fvalues = &index->fvalues;

   // It is faster truncating a file than re-creating it again. 
   if ((i = fileSetSize(&fnodes->file, 0)))
   {
      fileError(context, i, fnodes->name);
      return false;
   }
   if (index->hasIdr)
   {
      if ((i = fileSetSize(&fvalues->file, 0)))
      {
         fileError(context, i, fvalues->name);
         return false;
      }
      fvalues->size = fvalues->position = fvalues->finalPos = fvalues->cachePos = fvalues->cacheIsDirty = 0;
   }
   
   i = CACHE_SIZE;
   while (--i >= 0) // Erases the cache.
      if (cache[i])
			cache[i]->idx = -1;

   // juliana@220_6: The node count should be reseted when recreating the indices.
   index->cacheI = index->nodeCount = fnodes->size = fnodes->position = fnodes->finalPos = fnodes->cachePos = fnodes->cacheIsDirty = 0;
   return true;
}

/** 
 * Delays the write to disk, caching them at memory. 
 * 
 * @param context The thread context where the function is being executed.
 * @param index The index.
 * @param delayed Indicates if the writing process is to be done later or not.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 */
bool indexSetWriteDelayed(Context context, Index* index, bool delayed)
{
	TRACE(delayed ? "indexSetWriteDelayed on" : "indexSetWriteDelayed off") 
   int32 i;
   bool ret = true;
   Node** nodes;

   ret &= nodeSetWriteDelayed(context, index->root, delayed); // Commits the pending keys.
   
// Commits the pending first level nodes.
#ifndef PALMOS   
   i = index->btreeMaxNodes;
   nodes = index->firstLevel;
   while (--i >= 0)
      ret &= nodeSetWriteDelayed(context, nodes[i], delayed);
#endif   
   
   // Commits the pending cache nodes.
   nodes = index->cache;
   i = CACHE_SIZE;
   while (--i >= 0)
      ret &= nodeSetWriteDelayed(context, nodes[i], delayed);
      
   if (!delayed) // Shrinks the values.
      ret &= nfGrowTo(context, &index->fnodes, index->nodeCount * index->nodeRecSize);
   index->isWriteDelayed = delayed;
   return ret;
}

/**
 * Adds a key to an index.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index where the key is going to be inserted.
 * @param values The key to be inserted.
 * @param record The record of the key in the table.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 * @throws DriverException If the index is corrupted.
 */
bool indexAddKey(Context context, Index* index, SQLValue** values, int32 record)
{
	TRACE("indexAddKey")
   Val value;
   Key* key = &index->tempKey;
   Node* root = index->root;
   bool splitting = false;
   int32 numberColumns = index->numberColumns;

   valueSet(value, record); // Sets the record.
   keySet(key, values, index, numberColumns); // Sets the key.
  
   // Inserts the key.
   if (!index->fnodes.size)
   {
      if (!keyAddValue(context, key, &value, index->isWriteDelayed))
         return false;
      nodeSet(root, key, LEAF, LEAF);
      if (nodeSave(context, root, true, 0, 1) < 0)
         return false;
   }
   else
   {
      Node* curr = root;
      Key* keyFound;
		int32 nodeCounter = index->nodeCount,
            btreeMaxNodesLess1 = index->btreeMaxNodes - 1,
            pos;

      while (true)
      {
         keyFound = &curr->keys[pos = nodeFindIn(context, curr,key, true)]; // juliana@201_3
         if (pos < curr->size && keyEquals(key, keyFound, numberColumns)) 
         {
            // Adds the repeated key to the currently stored one.
            if (!keyAddValue(context, keyFound, &value, index->isWriteDelayed) || !nodeSaveDirtyKey(context, curr, pos)) 
               return false;
            break;
         }
         else if (nodeIsLeaf(curr))
         {
            // If the node will becomes full, the insert is done again, this time keeping track of the ancestors. Note: with k = 50 and 200000 
            // values, there are about 1.1 million useless pushes without this redundant insert.
				if (!splitting && curr->size == btreeMaxNodesLess1)
            {
               splitting = true;
               curr = index->root;
               index->ancestors.size = 0;
					nodeCounter = index->nodeCount;
            }
            else
            {
               if (!keyAddValue(context, key, &value, index->isWriteDelayed) || !nodeInsert(context, curr, key, LEAF, LEAF, pos))
                  return false;
               if (splitting && !indexSplitNode(context, curr)) // Curr has overflown.
                     return false;
               break;
            }
         }
         else
         {
            if (splitting)
            {
               IntVectorPush(context, &index->ancestors, pos);
               IntVectorPush(context, &index->ancestors, curr->idx);
            }
				if (--nodeCounter < 0) // juliana@220_16: does not let the index access enter in an infinite loop. 
				{
					TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
					return false;
				}
            if (!(curr = indexLoadNode(context, index, curr->children[pos])))
               return false;
         }
      }
   }
   return true;
}

/**
 * Renames the index files.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index which will be renamed.
 * @param newName The new name for the index.
 * @return <code>false</code> if an error occured; <code>true</code>, otherwise.
 */
bool indexRename(Context context, Index* index, CharP newName)
{
	TRACE("indexRename")
   char buffer[DBNAME_SIZE];
   CharP sourcePath = index->table->sourcePath;
   int32 slot = index->table->slot;

   // Renames the keys.
   xstrcpy(index->name, newName);
   xstrcpy(buffer, newName);
   xstrcat(buffer, IDK_EXT);
   if (!nfRename(context, &index->fnodes, buffer, sourcePath, slot)) 
      return false;

   // Renames the repeated values
   buffer[xstrlen(buffer) - 1] = 'r';
	if (index->hasIdr && !nfRename(context, &index->fvalues, buffer, sourcePath, slot)) 
   {
      // Renames .idk back if an error occurs when renaming .idr.
      buffer[xstrlen(buffer) - 1] = 'k';
      nfRename(context, &index->fnodes, buffer, sourcePath, slot);
      return false;
   }
   return true;
}

/**
 * Returns a node already loaded or loads it if there is empty space in the cache node to avoid loading already loaded nodes.
 * 
 * @param context The thread context where the function is being executed.
 * @param index The index where a node is going to be fetched.
 * @return The loaded node, a new cache node with the requested node loaded, a first level node if not palm OS, or <code>null</code> if it is not 
 * already loaded and its cache is full.
 */
Node* getLoadedNode(Context context, Index* index, int32 idx) 
{
   Node* node;
   Node** nodes;
   int32 i = -1;
   
#ifndef PALMOS
   // Tries to find the node in the nodes of the first level.
   if (idx <= index->btreeMaxNodes)
   {
      if (!(node = (nodes = index->firstLevel)[idx - 1]))
      {
         (node = nodes[idx - 1] = createNode(index))->idx = idx;
         nodeLoad(context, node);
      }
      return node;
   }
#endif
   
   // Tries to get an already loaded node if it is a node from a deeper level.
   nodes = index->cache;
   while (++i < CACHE_SIZE && nodes[i]) 
      if (nodes[i]->idx == idx)
         return nodes[index->cacheI = i];   
   
   if (i < CACHE_SIZE) // Loads the node if there is enough space in the node cache.
   {
      (node = nodes[index->cacheI = i] = createNode(index))->idx = idx;
      nodeLoad(context, node);
      return node;
   }
   
   return null;
}

// juliana@230_21: MAX() and MIN() now use indices on simple queries.   
/**
 * Finds the minimum value of an index in a range.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index where to find the minimum value.
 * @param sqlValue The minimum value inside the given range to be returned.
 * @param bitMap The table bitmap wich indicates which rows will be in the result set. 
 * @param heap A heap to allocate a temporary stack if necessary.
 * @return <code>false</code> if an error occurs; <code>true</code>, otherwise.
 */
bool findMinValue(Context context, Index* index, SQLValue* sqlValue, IntVector* bitMap, Heap heap)
{
   PlainDB* plainDB = index->table->db;
   Node* curr;
   Stack stack = TC_newStack(index->nodeCount, 2, heap);
   int32 size,
         idx = 0,
         valRec,
         i = -1,
         nodeCounter = index->nodeCount + 1;
   XFile* fvalues = &index->fvalues;
   Val value; 
      
   // Recursion using a stack.
   TC_stackPush(stack, &idx);
   while (TC_stackPop(stack, &idx))
   {
      if (--nodeCounter < 0) // juliana@220_16: does not let the index access enter in an infinite loop.
      {
			TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
			return false;
	   }
      if (!(curr = indexLoadNode(context, index, idx)))
         return false;
      
      // Searches for the smallest key of the node marked in the result set or is not deleted. 
      size = curr->size;
      
      if (bitMap)
      {
         while (++i < size)
            if ((valRec = curr->keys[i].valRec) < 0)
            {
               if (IntVectorisBitSet(bitMap, -1 - curr->keys[i].valRec))
               {
                  xmemmove(sqlValue, curr->keys[i].keys, sizeof(SQLValue));
                  break;
               }
            }
            else if (valRec != NO_VALUE)
            {
               while (valRec != NO_MORE)
               {
                  nfSetPos(fvalues, valRec * VALUERECSIZE);
                  valueLoad(context, &value, fvalues);
                  if (IntVectorisBitSet(bitMap, value.record))
                  {
                     xmemmove(sqlValue, curr->keys[i].keys, sizeof(SQLValue));
                     break;
                  }
                  valRec = value.next;
               }
               if (valRec != NO_MORE)
                  break;
            }
      }
      else
         while (++i < size)
            if (curr->keys[i].valRec != NO_VALUE)
            {
               xmemmove(sqlValue, curr->keys[i].keys, sizeof(SQLValue));
               break;
            }
         
      // Now searches the children nodes whose keys are smaller than the one marked or all of them if no one is marked. 
      i++;   
      while (--i >= 0)
         if (curr->children[i] != LEAF)
            TC_stackPush(stack, &curr->children[i]);
   }
   
   return loadStringForMaxMin(context, index, sqlValue); 
}

/**
 * Finds the maximum value of an index in a range.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index where to find the minimum value.
 * @param bitMap The table bitmap wich indicates which rows will be in the result set.
 * @param sqlValue The maximum value inside the given range to be returned.
 * @param heap A heap to allocate a temporary stack if necessary.
 * @return <code>false</code> if an error occurs; <code>true</code>, otherwise.
 */
bool findMaxValue(Context context, Index* index, SQLValue* sqlValue, IntVector* bitMap, Heap heap)
{
   Node* curr;
   Stack stack = TC_newStack(index->nodeCount, 2, heap);
   int32 size,
         idx = 0,
         valRec,
         i = -1,
         nodeCounter = index->nodeCount + 1;
   XFile* fvalues = &index->fvalues;
   Val value; 
      
   // Recursion using a stack.   
   TC_stackPush(stack, &idx);
   while (TC_stackPop(stack, &idx))
   {
      if (--nodeCounter < 0) // juliana@220_16: does not let the index access enter in an infinite loop.
      {
			TC_throwExceptionNamed(context, "litebase.DriverException", getMessage(ERR_CANT_LOAD_NODE));
			return false;
	   }
      if (!(curr = indexLoadNode(context, index, idx)))
         return false;
      
      // Searches for the smallest key of the node marked in the result set.
      i = size = curr->size;
      
      if (bitMap)
      {
         while (--i >= 0)
            if ((valRec = curr->keys[i].valRec) < 0)
            {
               if (IntVectorisBitSet(bitMap, -1 - curr->keys[i].valRec))
               {
                  xmemmove(sqlValue, curr->keys[i].keys, sizeof(SQLValue));
                  break;
               }
            }
            else if (valRec != NO_VALUE)
            {
               while (valRec != NO_MORE)
               {
                  nfSetPos(fvalues, valRec * VALUERECSIZE);
                  valueLoad(context, &value, fvalues);
                  if (IntVectorisBitSet(bitMap, value.record))
                  {
                     xmemmove(sqlValue, curr->keys[i].keys, sizeof(SQLValue));
                     break;
                  }
                  valRec = value.next;
               }
               if (valRec != NO_MORE)
                  break;
            }
      }
      else
         while (--i >= 0)
            if (curr->keys[i].valRec != NO_VALUE)
            {
               xmemmove(sqlValue, curr->keys[i].keys, sizeof(SQLValue));
               break;
            }
      
      // Now searches the children nodes whose keys are smaller than the one marked or all of them if no one is marked.   
      while (++i <= size && curr->children[i] != LEAF)
         TC_stackPush(stack, &curr->children[i]);
   }

   return loadStringForMaxMin(context, index, sqlValue); 
}

/**
 * Loads a string from the table if needed.
 *
 * @param context The thread context where the function is being executed.
 * @param index The index where to find the minimum value. 
 * @param sqlValue The record structure which will hold (holds) the string.
 * @return <code>false</false> if an error occurs; <code>true</code>, otherwise or no record was found.
 */
bool loadStringForMaxMin(Context context, Index* index, SQLValue* sqlValue)
{
   PlainDB* plainDB = index->table->db;
   
   if (sqlValue->isNull) // No record found.
      return true;

   sqlValue->asBlob = (uint8*)plainDB;
   
   // If the type is string and the value is not loaded, loads it.
   if ((*index->types == CHARS_TYPE || *index->types == CHARS_NOCASE_TYPE) && !sqlValue->length)
   { 
      XFile* dbo = &plainDB->dbo;
      int32 length = 0;
      nfSetPos(dbo, sqlValue->asInt); // Gets and sets the string position in the .dbo.
         
      // Fetches the string length.
      if (nfReadBytes(context, dbo, (uint8*)&length, 2) != 2
       && !loadString(context, plainDB, sqlValue->asChars, sqlValue->length = length))
         return false;
   }
   return true;  
}

#ifdef ENABLE_TEST_SUITE

/**
 * Tests if <code>createComposedIndex()</code> works properly.
 * 
 * @param testSuite The test structure.
 * @param currentContext The thread context where the test is being executed.
 */
TESTCASE(createComposedIndex)
{
   Heap heap = heapCreate();
   uint8 columns[256];
   int32 i = 256;
   ComposedIndex* compIndex;
   UNUSED(currentContext)

   IF_HEAP_ERROR(heap)
   {
      heapDestroy(heap);
      TEST_FAIL(tc, "OutOfMemoryError");
      goto finish;
   }

   while (--i >= 0) // Creates composed index structures of various sizes.
   {
      xmemset(columns, i, i);
      ASSERT1_EQUALS(NotNull, compIndex = createComposedIndex(i, columns, i, heap));
      ASSERT2_EQUALS(I32, compIndex->indexId, i);
      ASSERT2_EQUALS(I32, compIndex->numberColumns, i);
      ASSERT3_EQUALS(Block, columns, compIndex->columns, i);
   }

   heapDestroy(heap);

finish: ;
}
#endif
