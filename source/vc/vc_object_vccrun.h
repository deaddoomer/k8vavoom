  DECLARE_FUNCTION(get_GC_ImmediateDelete)
  DECLARE_FUNCTION(set_GC_ImmediateDelete)

  // implemented in main vccrun source
  DECLARE_FUNCTION(CreateTimer)
  DECLARE_FUNCTION(CreateTimerWithId)
  DECLARE_FUNCTION(DeleteTimer)
  DECLARE_FUNCTION(IsTimerExists)
  DECLARE_FUNCTION(IsTimerOneShot)
  DECLARE_FUNCTION(GetTimerInterval)
  DECLARE_FUNCTION(SetTimerInterval)
  DECLARE_FUNCTION(GetTickCount)

  DECLARE_FUNCTION(fsysAppendDir)
  DECLARE_FUNCTION(fsysAppendPak)
  DECLARE_FUNCTION(fsysRemovePak)
  DECLARE_FUNCTION(fsysRemovePaksFrom)
  DECLARE_FUNCTION(fsysFindPakByPrefix)
  DECLARE_FUNCTION(fsysFileExists)
  DECLARE_FUNCTION(fsysFileFindAnyExt)
  DECLARE_FUNCTION(fsysGetPakPath)
  DECLARE_FUNCTION(fsysGetPakPrefix)
  DECLARE_FUNCTION(fsysGetLastPakId)

  DECLARE_FUNCTION(get_fsysKillCommonZipPrefix)
  DECLARE_FUNCTION(set_fsysKillCommonZipPrefix)

  DECLARE_FUNCTION(appSetName)
  DECLARE_FUNCTION(appSaveOptions)
  DECLARE_FUNCTION(appLoadOptions)

  DECLARE_FUNCTION(ccmdClearText)
  DECLARE_FUNCTION(ccmdClearCommand)
  DECLARE_FUNCTION(ccmdParseOne)
  DECLARE_FUNCTION(ccmdGetArgc)
  DECLARE_FUNCTION(ccmdGetArgv)
  DECLARE_FUNCTION(ccmdTextSize)
  DECLARE_FUNCTION(ccmdPrepend)
  DECLARE_FUNCTION(ccmdPrependQuoted)
  DECLARE_FUNCTION(ccmdAppend)
  DECLARE_FUNCTION(ccmdAppendQuoted)
