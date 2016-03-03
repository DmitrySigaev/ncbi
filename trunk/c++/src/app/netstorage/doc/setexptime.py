# Preconditions:
# - MS SQL access required for the service
def SetExpTime( locator,
                newExpiration,
                createIfNotFound = True ):

    objectSizeUnknown = False
    errors = []

    # Part I. MS SQL DB
    try:
        if DBRecordExists():
            if IsObjectExpired():
                return "ObjectExpired"

            UpdateExpiration( newExpiration )
            if ObjectSize == NULL:
                objectSizeUnknown = True
        else:
            objectSizeUnknown = True
            # cml 1 sw
            if createIfNotFound:
                CreateNewObjectRecord()
    except DBAccess:
        errors.append( "DBAccess" )
    except SPExecution:
        errors.append( "SPExecution" )

    # Part II. Backend storage
    ret = UpdateBackendExpiration( newExpiration )
    # cml 1 sw
    if ret == error:
        # cml 1 sw
        if objectSizeUnknown == False:
            errors.append( ret )
    if errors:
        return "Error"
    return "OK"
