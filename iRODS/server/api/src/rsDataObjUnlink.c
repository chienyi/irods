/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* This is script-generated code (for the most part).  */ 
/* See dataObjUnlink.h for a description of this API call.*/

#include "dataObjUnlink.h"
#include "rodsLog.h"
#include "icatDefines.h"
#include "fileUnlink.h"
#include "unregDataObj.h"
#include "objMetaOpr.h"
#include "rsGlobalExtern.h"
#include "rcGlobalExtern.h"
#include "reGlobalsExtern.h"
#include "reDefines.h"
#include "rmCollOld.h"
#include "rmColl.h"
#include "dataObjRename.h"
#include "subStructFileUnlink.h"
#include "modDataObjMeta.h"

int
rsDataObjUnlink (rsComm_t *rsComm, dataObjInp_t *dataObjUnlinkInp)
{
    int status;
    ruleExecInfo_t rei;
    int trashPolicy;
    dataObjInfo_t *dataObjInfoHead = NULL;

    rodsServerHost_t *rodsServerHost = NULL;

    status = getAndConnRcatHost (rsComm, MASTER_RCAT,
     dataObjUnlinkInp->objPath, &rodsServerHost);

    if (status < 0) {
        return (status);
    } else if (rodsServerHost->rcatEnabled == REMOTE_ICAT) {
        int retval;
        retval = rcDataObjUnlink (rodsServerHost->conn, dataObjUnlinkInp);
        return status;
    }

    if (getValByKey (
      &dataObjUnlinkInp->condInput, IRODS_ADMIN_RMTRASH_KW) != NULL ||
      getValByKey (
      &dataObjUnlinkInp->condInput, IRODS_RMTRASH_KW) != NULL) {
        if (isTrashPath (dataObjUnlinkInp->objPath) == False) {
            return (SYS_INVALID_FILE_PATH);
        }
    }

    dataObjUnlinkInp->openFlags = O_WRONLY;  /* set the permission checking */
    status = getDataObjInfoIncSpecColl (rsComm, dataObjUnlinkInp, 
      &dataObjInfoHead);

    if (status < 0) return (status);

    if (getValByKey (&dataObjUnlinkInp->condInput, FORCE_FLAG_KW) != NULL ||
      getValByKey (&dataObjUnlinkInp->condInput, REPL_NUM_KW) != NULL ||
      dataObjInfoHead->specColl != NULL) {
        status = _rsDataObjUnlink (rsComm, dataObjUnlinkInp, dataObjInfoHead);
    } else {
        initReiWithDataObjInp (&rei, rsComm, dataObjUnlinkInp);
        status = applyRule ("acTrashPolicy", NULL, &rei, NO_SAVE_REI);
        trashPolicy = rei.status;

        if (trashPolicy != NO_TRASH_CAN) {
            status = rsMvDataObjToTrash (rsComm, dataObjUnlinkInp, 
	      dataObjInfoHead);
            return status;
        } else {
            status = _rsDataObjUnlink (rsComm, dataObjUnlinkInp, 
	      dataObjInfoHead);
        }
    }

    return (status);
}

int
_rsDataObjUnlink (rsComm_t *rsComm, dataObjInp_t *dataObjUnlinkInp,
dataObjInfo_t *dataObjInfoHead)
{
    int status;
    int retVal = 0;
    dataObjInfo_t *tmpDataObjInfo;

    tmpDataObjInfo = dataObjInfoHead;
    while (tmpDataObjInfo != NULL) {
	status = dataObjUnlinkS (rsComm, dataObjUnlinkInp, tmpDataObjInfo);
	if (status < 0) {
	    if (retVal == 0) {
	        retVal = status;
	    }
	}
        if (dataObjUnlinkInp->specColl != NULL) 	/* do only one */
	    break;
	tmpDataObjInfo = tmpDataObjInfo->next;
    }

    if (dataObjInfoHead->specColl == NULL)
        resolveDataObjReplStatus (rsComm, dataObjUnlinkInp);
    freeAllDataObjInfo (dataObjInfoHead);  

    return (retVal);
}

/* resolveDataObjReplStatus - a dirty copy may be deleted leaving no
 * dirty copy. In that case, pick the newest copy and mark it dirty
 */
int
resolveDataObjReplStatus (rsComm_t *rsComm, dataObjInp_t *dataObjUnlinkInp)
{
    int status;
    dataObjInfo_t *dataObjInfoHead = NULL;
    dataObjInfo_t *newestDataObjInfo = NULL;
    dataObjInfo_t *tmpDataObjInfo;

    if (getValByKey (&dataObjUnlinkInp->condInput, RESC_NAME_KW) == NULL &&
      getValByKey (&dataObjUnlinkInp->condInput, REPL_NUM_KW) == NULL) {
	return 0;
    } 
    status = getDataObjInfo (rsComm, dataObjUnlinkInp,
      &dataObjInfoHead, ACCESS_DELETE_OBJECT, 1);

    if (status < 0) return status;

    tmpDataObjInfo = dataObjInfoHead;
    while (tmpDataObjInfo != NULL) {
	if (tmpDataObjInfo->replStatus == 0) {
	    if (newestDataObjInfo == NULL) {
		newestDataObjInfo = tmpDataObjInfo;
	    } else if (atoi (tmpDataObjInfo->dataModify) >
	      atoi (newestDataObjInfo->dataModify)) {
		newestDataObjInfo = tmpDataObjInfo;
	    }
	} else {
	    newestDataObjInfo = NULL;
	    break;
	}
        tmpDataObjInfo = tmpDataObjInfo->next;
    }

    /* modify the repl status */
    if (newestDataObjInfo != NULL) {
        keyValPair_t regParam;
        char tmpStr[MAX_NAME_LEN];
        modDataObjMeta_t modDataObjMetaInp;

	memset (&regParam, 0, sizeof (regParam));
	memset (&modDataObjMetaInp, 0, sizeof (modDataObjMetaInp));
        snprintf (tmpStr, MAX_NAME_LEN, "%d", NEWLY_CREATED_COPY);
        addKeyVal (&regParam, REPL_STATUS_KW, tmpStr);
        modDataObjMetaInp.dataObjInfo = newestDataObjInfo;
        modDataObjMetaInp.regParam = &regParam;

        status = rsModDataObjMeta (rsComm, &modDataObjMetaInp);

        clearKeyVal (&regParam);
    }
    freeAllDataObjInfo (dataObjInfoHead);
    return (status);
}

int
dataObjUnlinkS (rsComm_t *rsComm, dataObjInp_t *dataObjUnlinkInp,
dataObjInfo_t *dataObjInfo)
{
    int status;
    unregDataObj_t unregDataObjInp;
    ruleExecInfo_t rei;

    initReiWithDataObjInp (&rei, rsComm, dataObjUnlinkInp);
    rei.doi = dataObjInfo;

    status = applyRule ("acDataDeletePolicy", NULL, &rei, NO_SAVE_REI);

    if (status < 0 && status != NO_MORE_RULES_ERR && 
      status != SYS_DELETE_DISALLOWED) {
        rodsLog (LOG_NOTICE,
          "dataObjUnlinkS: acDataDeletePolicy error for %s. status = %d",
          dataObjUnlinkInp->objPath, status);
        return (status);
    }

    if (rei.status == SYS_DELETE_DISALLOWED) {
        rodsLog (LOG_NOTICE,
        "dataObjUnlinkS:disallowed for %s via DataDeletePolicy,status=%d",
          dataObjUnlinkInp->objPath, rei.status);
	return (rei.status);
    }

    if (dataObjInfo->specColl == NULL) {
        unregDataObjInp.dataObjInfo = dataObjInfo;
        unregDataObjInp.condInput = &dataObjUnlinkInp->condInput;
        status = rsUnregDataObj (rsComm, &unregDataObjInp);

        if (status < 0) {
            rodsLog (LOG_NOTICE,
              "dataObjUnlinkS: rsUnregDataObj error for %s. status = %d",
              dataObjUnlinkInp->objPath, status);
	    return status;
        }
    }
    
    status = l3Unlink (rsComm, dataObjInfo);

    if (status < 0) {
	int myError = getErrno (status);
        rodsLog (LOG_NOTICE,
          "dataObjUnlinkS: l3Unlink error for %s. status = %d",
          dataObjUnlinkInp->objPath, status);
	/* allow ENOENT to go on and unregister */
	if (myError != ENOENT && myError != EACCES) {
            rodsLog (LOG_NOTICE,
              "dataObjUnlinkS: orphan file %s", dataObjInfo->filePath);
	    return (status);
	} else {
	    status = 0;
	}
    }

    return (status);
}

int
l3Unlink (rsComm_t *rsComm, dataObjInfo_t *dataObjInfo)
{
    int rescTypeInx;
    int rescClassInx;
    fileUnlinkInp_t fileUnlinkInp;
    int status;

    rescClassInx = dataObjInfo->rescInfo->rescClassInx;
    if (RescClass[rescClassInx].classType == BUNDLE_CL) return 0;

    if (getStructFileType (dataObjInfo->specColl) >= 0) {
        subFile_t subFile;
        memset (&subFile, 0, sizeof (subFile));
        rstrcpy (subFile.subFilePath, dataObjInfo->subPath,
          MAX_NAME_LEN);
        rstrcpy (subFile.addr.hostAddr, dataObjInfo->rescInfo->rescLoc,
          NAME_LEN);
        subFile.specColl = dataObjInfo->specColl;
        status = rsSubStructFileUnlink (rsComm, &subFile);
    } else {
        rescTypeInx = dataObjInfo->rescInfo->rescTypeInx;


        switch (RescTypeDef[rescTypeInx].rescCat) {
          case FILE_CAT:
            memset (&fileUnlinkInp, 0, sizeof (fileUnlinkInp));
            fileUnlinkInp.fileType = RescTypeDef[rescTypeInx].driverType;
            rstrcpy (fileUnlinkInp.fileName, dataObjInfo->filePath, 
	      MAX_NAME_LEN);
            rstrcpy (fileUnlinkInp.addr.hostAddr, 
	      dataObjInfo->rescInfo->rescLoc, NAME_LEN);
            status = rsFileUnlink (rsComm, &fileUnlinkInp);
            break;

          default:
            rodsLog (LOG_NOTICE,
              "l3Unlink: rescCat type %d is not recognized",
              RescTypeDef[rescTypeInx].rescCat);
            status = SYS_INVALID_RESC_TYPE;
            break;
	}
    }
    return (status);
}

int
rsMvDataObjToTrash (rsComm_t *rsComm, dataObjInp_t *dataObjInp,
dataObjInfo_t *dataObjInfoHead)
{
    int status;
    ruleExecInfo_t rei;
    char trashPath[MAX_NAME_LEN];
    dataObjCopyInp_t dataObjRenameInp;

    if (getValByKey (&dataObjInp->condInput, DATA_ACCESS_KW) == NULL) {
        addKeyVal (&dataObjInp->condInput, DATA_ACCESS_KW,
          ACCESS_DELETE_OBJECT);
    }

    status = getDataObjInfo (rsComm, dataObjInp, &dataObjInfoHead,
      ACCESS_DELETE_OBJECT, 0);

    if (status < 0) {
        rodsLog (LOG_NOTICE,
          "rsMvDataObjToTrash: getDataObjInfo error for %s. status = %d",
          dataObjInp->objPath, status);
        return (status);
    }

    initReiWithDataObjInp (&rei, rsComm, dataObjInp);
    rei.doi = dataObjInfoHead;

    status = applyRule ("acDataDeletePolicy", NULL, &rei, NO_SAVE_REI);

    if (status < 0 && status != NO_MORE_RULES_ERR &&
      status != SYS_DELETE_DISALLOWED) {
        rodsLog (LOG_NOTICE,
          "rsMvDataObjToTrash: acDataDeletePolicy error for %s. status = %d",
          dataObjInp->objPath, status);
        return (status);
    }

    if (rei.status == SYS_DELETE_DISALLOWED) {
        rodsLog (LOG_NOTICE,
        "rsMvDataObjToTrash:disallowed for %s via DataDeletePolicy,status=%d",
          dataObjInp->objPath, rei.status);
        return (rei.status);
    }

    status = rsMkTrashPath (rsComm, dataObjInp->objPath, trashPath);

    if (status < 0) {
        return (status);
    }

    memset (&dataObjRenameInp, 0, sizeof (dataObjRenameInp));

    dataObjRenameInp.srcDataObjInp.oprType =
      dataObjRenameInp.destDataObjInp.oprType = RENAME_DATA_OBJ;

    rstrcpy (dataObjRenameInp.destDataObjInp.objPath, trashPath, MAX_NAME_LEN);
    rstrcpy (dataObjRenameInp.srcDataObjInp.objPath, dataObjInp->objPath,
      MAX_NAME_LEN);

    status = rsDataObjRename (rsComm, &dataObjRenameInp);

    if (status < 0) {
        appendRandomToPath (dataObjRenameInp.destDataObjInp.objPath);
	status = rsDataObjRename (rsComm, &dataObjRenameInp);
        if (status < 0) {
            rodsLog (LOG_ERROR,
              "rsMvDataObjToTrash: rsDataObjRename error for %s",
              dataObjRenameInp.destDataObjInp.objPath);
            return (status);
	}
    }

    return (status);
}

