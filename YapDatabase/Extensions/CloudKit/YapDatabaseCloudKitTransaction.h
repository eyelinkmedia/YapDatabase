#import <Foundation/Foundation.h>
#import "YapDatabaseExtensionTransaction.h"


@interface YapDatabaseCloudKitTransaction : YapDatabaseExtensionTransaction

/**
 * This method is use to associate an existing CKRecord with a row in the database.
 * There are two primary use cases for this method.
 * 
 * 1. To associate a discovered/pulled CKRecord with a row in the database before we insert it.
 *    In particular, for the following situation:
 *    
 *    - You're pulling record changes from the server via CKFetchRecordChangesOperation (or similar).
 *    - You discover a record that was inserted by another device.
 *    - You need to add a corresponding row to the database,
 *      but you also need to inform the YapDatabaseCloud extension about the existing record,
 *      so it won't bother invoking the recordHandler, or attempting to upload the existing record.
 *    - So you invoke this method FIRST.
 *    - And THEN you insert the corresponding object into the database via the
 *      normal setObject:forKey:inCollection: method (or similar methods).
 *
 * 2. To assist in the migration process when switching to YapDatabaseCloudKit.
 *    In particular, for the following situation:
 * 
 *    - You have an existing object in the database that is associated with a CKRecord.
 *    - But you've been handling CloudKit manually (not via YapDatabaseCloudKit).
 *    - And you now want YapDatabaseCloudKit to manage the CKRecord for you.
 * 
 * Thus, this methods works as a simple "hand-off" of the CKRecord to the YapDatabaseCloudKit extension.
 *
 * In other words, YapDatbaseCloudKit will write the system fields of the given CKRecord to its internal table,
 * and associate it with the given collection/key tuple.
 * 
 * @param record
 *   The CKRecord to associate with the collection/key tuple.
 * 
 * @param databaseIdentifer
 *   The identifying string for the CKDatabase.
 *   @see YapDatabaseCloudKitDatabaseBlock.
 *
 * @param key
 *   The key of the row to associaed the record with.
 * 
 * @param collection
 *   The collection of the row to associate the record with.
 * 
 * @param shouldUpload
 *   If NO, then the record is simply associated with the collection/key,
 *     and YapDatabaseCloudKit doesn't attempt to push the record to the cloud.
 *   If YES, then the record is associated with the collection/key,
 *     and YapDatabaseCloutKit assumes the given record is dirty and attempts to push the record to the cloud.
 * 
 * @return
 *   YES if the record was associated with the given collection/key.
 *   NO if one of the following errors occurred.
 * 
 * The following errors will prevent this method from succeeding:
 * - The given record is nil.
 * - The given collection/key is already associated with another record (must detach it first).
 * - The recordID/databaseIdentifier is already associated with another collection/key (must detach it first).
 * 
 * Important: This method only works if within a readWriteTrasaction.
 * Invoking this method from within a read-only transaction will throw an exception.
**/
- (BOOL)attachRecord:(CKRecord *)record
  databaseIdentifier:(NSString *)databaseIdentifier
              forKey:(NSString *)key
        inCollection:(NSString *)collection
  shouldUploadRecord:(BOOL)shouldUpload;

/**
 * This method is use to unassociate an existing CKRecord with a row in the database.
 * There are three primary use cases for this method.
 * 
 * 1. To properly handle CKRecordID's that are reported as deleted from the server.
 *    In particular, for the following situation:
 *    
 *    - You're pulling record changes from the server via CKFetchRecordChangesOperation (or similar).
 *    - You discover a recordID that was deleted by another device.
 *    - You need to remove the associated record from the database,
 *      but you also need to inform the YapDatabaseCloud extension that it was remotely deleted,
 *      so it won't bother attempting to upload the already deleted recordID.
 *    - So you invoke this method FIRST.
 *    - And THEN you remove the corresponding object from the database via the
 *      normal remoteObjectForKey:inCollection: method (or similar methods).
 * 
 * 2. To assist in various migrations, such as version migrations.
 *    For example:
 * 
 *    - In version 2 of your app, you need to move a few CKRecords into a new zone.
 *    - But you don't want to delete the items from the old zone,
 *      because you need to continue supporting v1.X for awhile.
 *    - So you invoke this method first in order to drop the previously associated record.
 *    - And then you can attach the new CKRecords,
 *      and have YapDatabaseCloudKit upload the new records (to their new zone).
 * 
 * 3. To "move" an object from the cloud to "local-only".
 *    For example:
 * 
 *    - You're making a Notes app that allows user to stores notes locally, or in the cloud.
 *    - The user moves an existing note from the cloud, to local-storage only.
 *    - This method can be used to delete the item from the cloud without deleting it locally.
 * 
 * @param key
 *   The key of the row associated with the record to detach.
 *   
 * @param collection
 *   The collection of the row associated with the record to detach.
 * 
 * @param wasRemoteDeletion
 *   If you're invoking this method because the server notified you of a deleted CKRecordID,
 *   then be sure to pass YES for this parameter. Doing so allows the extension to properly modify the
 *   changeSets that are still queued for upload so that it can remove potential modifications for this recordID.
 * 
 * @param shouldUpload
 *   Whether or not the extension should push a deleted CKRecordID to the cloud.
 *   In use case #2 (from the discussion of this method, concerning migration), you'd pass NO.
 *   In use case #3 (from the discussion of this method, concerning moving), you'd pass YES.
 *   This parameter is ignored if wasRemoteDeletion is YES.
 * 
 * Note: If you're notified of a deleted CKRecordID from the server,
 *       and you're unsure of the associated local collection/key,
 *       then you can use the getKey:collection:forRecordID:databaseIdentifier: method.
 * 
 * @see getKey:collection:forRecordID:databaseIdentifier:
**/
- (void)detachRecordForKey:(NSString *)key
              inCollection:(NSString *)collection
         wasRemoteDeletion:(BOOL)wasRemoteDeletion
      shouldUploadDeletion:(BOOL)shouldUpload;

/**
 * This method is used to merge a pulled record from the server with what's in the database.
 * In particular, for the following situation:
 *
 * - You're pulling record changes from the server via CKFetchRecordChangesOperation (or similar).
 * - You discover a record that was modified by another device.
 * - You need to properly merge the changes with your own version of the object in the database,
 *   and you also need to inform YapDatabaseCloud extension about the merger
 *   so it can properly handle any changes that were pending a push to the cloud.
 * 
 * Thus, you should use this method, which will invoke your mergeBlock with the appropriate parameters.
 * 
 * @param remoteRecord
 *   A record that was modified remotely, and discovered via CKFetchRecordChangesOperation (or similar).
 *   This value will be passed as the remoteRecord parameter to the mergeBlock.
 * 
 * @param databaseIdentifier
 *   The identifying string for the CKDatabase.
 *   @see YapDatabaseCloudKitDatabaseBlock.
 * 
 * @param key (optional)
 *   If the key & collection of the corresponding object are known, then you should pass them.
 *   This allows the method to skip the overhead of doing the lookup itself.
 *   If unknown, then you can simply pass nil, and it will do the appropriate lookup.
 * 
 * @param collection (optional)
 *   If the key & collection of the corresponding object are known, then you should pass them.
 *   This allows the method to skip the overhead of doing the lookup itself.
 *   If unknown, then you can simply pass nil, and it will do the appropriate lookup.
**/
- (void)mergeRecord:(CKRecord *)remoteRecord
 databaseIdentifier:(NSString *)databaseIdentifer
             forKey:(NSString *)key
       inCollection:(NSString *)collection;

/**
 * If the given recordID & databaseIdentifier are associated with row in the database,
 * then this method will return YES, and set the collectionPtr/keyPtr with the collection/key of the associated row.
 * 
 * @param keyPtr (optional)
 *   If non-null, and this method returns YES, then the keyPtr will be set to the associated row's key.
 * 
 * @param collectionPtr (optional)
 *   If non-null, and this method returns YES, then the collectionPtr will be set to the associated row's collection.
 * 
 * @param recordID
 *   The CKRecordID to look for.
 * 
 * @param databaseIdentifier
 *   The identifying string for the CKDatabase.
 *   @see YapDatabaseCloudKitDatabaseBlock.
 * 
 * @return
 *   YES if the given recordID & databaseIdentifier are associated with a row in the database.
 *   NO otherwise.
**/
- (BOOL)getKey:(NSString **)keyPtr collection:(NSString **)collectionPtr
                                  forRecordID:(CKRecordID *)recordID
                           databaseIdentifier:(NSString *)databaseIdentifier;

/**
 * If the given key/collection tuple is associated with a record,
 * then this method returns YES, and sets the recordIDPtr & databaseIdentifierPtr accordingly.
 * 
 * @param recordIDPtr (optional)
 *   If non-null, and this method returns YES, then the recordIDPtr will be set to the associated recordID.
 * 
 * @param databaseIdentifierPtr (optional)
 *   If non-null, and this method returns YES, then the databaseIdentifierPtr will be set to the associated value.
 *   Keep in mind that nil is a valid databaseIdentifier,
 *   and is generally used to signify the defaultContainer/privateCloudDatabase.
 *
 * @param key
 *   The key of the row in the database.
 * 
 * @param collection
 *   The collection of the row in the database.
 * 
 * @return
 *   YES if the given collection/key is associated with a CKRecord.
 *   NO otherwise.
**/
- (BOOL)getRecordID:(CKRecordID **)recordIDPtr
 databaseIdentifier:(NSString **)databaseIdentifierPtr
             forKey:(NSString *)key
       inCollection:(NSString *)collection;

@end