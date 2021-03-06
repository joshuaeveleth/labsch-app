/* 
** File:
**   $Id: $
**
** Purpose: Implement the table manager.
**
** Notes
**   1. 
**
** References:
**   1. Core Flight Executive Application Developers Guide.
**   2. The GN&C FSW Framework Programmer's Guide
**
**
** $Date: $
** $Revision: $
** $Log: $
*/

/*
** Include Files:
*/

#include <string.h>
#include "tblmgr.h"


/*
** Global File Data
*/

static TBLMGR_Class* TblMgr = NULL;

static char TblFileBuff[TBLMGR_BUFFSIZE];  /* XML table file buffer could be large so don't put on the stack */

/*
** Local File Function Prototypes
*/

typedef void (*DumpTableFuncPtr)(int32 FileHandle);   /* File-specific dump table function */

/******************************************************************************
** Function: DumpTableToFile
**
** General purpose function that takes a file pointer to the table-specific
** processing.
**
*/
static boolean DumpTableToFile(const char* FileName, const char* FileDescr, DumpTableFuncPtr DumpTableFunc);


/******************************************************************************
** Function: ParseXmlFile
**
** General purpose XML file parser.  The Start and End element callback functions
** do the file-specific processing.
**
*/
static boolean ParseXmlFile(const char* FileName,
                            XML_StartElementHandler StartElementHandler,
                            XML_EndElementHandler   EndElementHandler);

/******************************************************************************
** Functions: DumpMsgTable, MsgTblStartElement, MsgTblEndElement
**
** These functions provide the message table specific processing for XML parsing
** and for dump file formatting.
**
*/
static void DumpMsgTable(int32 FileHandle);
static void XMLCALL MsgTblStartElement(void *data, const char *el, const char **attr);
static void XMLCALL MsgTblEndElement(void *data, const char *el);

/******************************************************************************
** Functions: DumpSchTable, SchTblStartElement, SchTblEndElement
**
** These functions provide the scheduler table specific processing for XML parsing
** and for dump file formatting.
**
*/
static void DumpSchTable(int32 FileHandle);
static void XMLCALL SchTblStartElement(void *data, const char *el, const char **attr);
static void XMLCALL SchTblEndElement(void *data, const char *el);


/******************************************************************************
** Function: TBLMGR_Constructor
**
** Notes:
**    1. This must be called prior to any other functions
**
*/
void TBLMGR_Constructor(TBLMGR_Class* TblMgrPtr,
                        char* MsgTblFilePathName,
                        char* SchTblFilePathName)
{

   TblMgr = TblMgrPtr;

   /*
    * Set all data and flags to zero.
    * AttrErrCnt - Incremented by XML parser
    */
   CFE_PSP_MemSet(&TblMgr->MsgTbl, 0, sizeof(TBLMGR_MsgTbl));
   CFE_PSP_MemSet(&TblMgr->SchTbl, 0, sizeof(TBLMGR_SchTbl));
   TblMgr->MsgTbl.LoadActive = TRUE;

   /* TODO - printf ("TBLMGR_Constructor() - before Parse MsgTbl\n"); */
   /* CFE_EVS_SendEvent(999,CFE_EVS_INFORMATION,"TBLMGR_Constructor() - before Parse MsgTbl"); */

   if (ParseXmlFile(MsgTblFilePathName, MsgTblStartElement, MsgTblEndElement) &&
       TblMgr->MsgTbl.AttrErrCnt == 0)
   {

      MSGTBL_LoadTable(&(TblMgr->MsgTbl.Local));
      TblMgr->MsgTbl.LastLoadValid = TRUE;

      TblMgr->SchTbl.LoadActive = TRUE;

      /* TODO - printf ("TBLMGR_Constructor() - before Parse SchTbl\n"); */
      /* CFE_EVS_SendEvent(999,CFE_EVS_INFORMATION,"TBLMGR_Constructor() - before Parse SchTbl"); */
      if (ParseXmlFile(SchTblFilePathName, SchTblStartElement, SchTblEndElement) &&
          TblMgr->SchTbl.AttrErrCnt == 0)
      {

         SCHTBL_LoadTable(&(TblMgr->SchTbl.Local));
         TblMgr->SchTbl.LastLoadValid = TRUE;

      } /* End if successful SCHTBL parse */

      TblMgr->SchTbl.LoadActive = FALSE;

   } /* End if successful MSGTBL parse */

   TblMgr->MsgTbl.LoadActive    = FALSE;
 
} /* End TBLMGR_Constructor() */


/******************************************************************************
** Function: TBLMGR_ResetStatus
**
*/
void TBLMGR_ResetStatus()
{

   CFE_PSP_MemSet((void *)TblMgr, 0, sizeof(TBLMGR_Class));

} /* End TBLMGR_ResetStatus() */


/******************************************************************************
** Function: TBLMGR_LoadMsgTable
**
*/
boolean TBLMGR_LoadMsgTable(const CFE_SB_MsgPtr_t MsgPtr)
{
   const  TBLMGR_LoadTblCmd *LoadTblCmd = (const TBLMGR_LoadTblCmd *) MsgPtr;
   int    entry;


   /*
    * Set all data and flags to zero.
    * AttrErrCnt - Incremented by XML parser
    */
   CFE_PSP_MemSet(&TblMgr->MsgTbl, 0, sizeof(TBLMGR_MsgTbl));
   TblMgr->MsgTbl.LoadActive    = TRUE;

   if (ParseXmlFile(LoadTblCmd->FileName, MsgTblStartElement, MsgTblEndElement) &&
       TblMgr->MsgTbl.AttrErrCnt == 0)
   {

      if (LoadTblCmd->LoadType == TBLMGR_LOAD_TBL_REPLACE)
      {
         MSGTBL_LoadTable(&(TblMgr->MsgTbl.Local));
         TblMgr->MsgTbl.LastLoadValid = TRUE;

      } /* End if replace entire table */
      else if (LoadTblCmd->LoadType == TBLMGR_LOAD_TBL_UPDATE)
      {
         for (entry=0; entry < MSGTBL_MAX_ENTRY_ID; entry++)
         {
            if (TblMgr->MsgTbl.Modified[entry])
               MSGTBL_LoadTableEntry(entry, &(TblMgr->MsgTbl.Local.Entry[entry]));
         }
         TblMgr->MsgTbl.LastLoadValid = TRUE;

      } /* End if update individual records */
      else
      {
         CFE_EVS_SendEvent(666,CFE_EVS_ERROR,"TBLMGR: Invalid message table command type %d",LoadTblCmd->LoadType);
      }

   } /* End if successful MSGTBL parse */
   else
   {
      CFE_EVS_SendEvent(666,CFE_EVS_ERROR,"TBLMGR: Message table command file parsing failed");
   }

   return TblMgr->MsgTbl.LastLoadValid;

} /* End of TBLMGR_LoadMsgTable() */

/******************************************************************************
** Function: TBLMGR_DumpMsgTable
**
*/
boolean TBLMGR_DumpMsgTable(const CFE_SB_MsgPtr_t MsgPtr)
{
   const  TBLMGR_DumpTblCmd *DumpTblCmd = (const TBLMGR_DumpTblCmd *) MsgPtr;
   char   FileName[OS_MAX_PATH_LEN];

   /* Copy the commanded filename into local buffer to ensure size limitation and to allow for modification */
   CFE_PSP_MemCpy(FileName, (void *)DumpTblCmd->FileName, OS_MAX_PATH_LEN);

   /* Check to see if a default filename should be used */
   if (FileName[0] == '\0')
   {
      strncpy(FileName, TBLMGR_DEF_MSG_TBL_DUMP_FILE, OS_MAX_PATH_LEN);
   }

   /* Make sure all strings are null terminated before attempting to process them */
   FileName[OS_MAX_PATH_LEN-1] = '\0';

   return DumpTableToFile(FileName,"Scheduler App's Message Table",DumpMsgTable);

} /* End of TBLMGR_DumpMsgTable() */


/******************************************************************************
** Function: TBLMGR_ConfigSchEntryCmd
**
*/
boolean TBLMGR_ConfigSchEntryCmd(const CFE_SB_MsgPtr_t MsgPtr)
{
   const  TBLMGR_ConfigSchCmd *ConfigSchEntryCmd = (const TBLMGR_ConfigSchCmd *) MsgPtr;

   if (ConfigSchEntryCmd->Slot < SCHTBL_TOTAL_SLOTS)
   {

      if (ConfigSchEntryCmd->EntryInSlot < SCHTBL_ENTRIES_PER_SLOT)
      {
         SCHTBL_ConfigureTableEntry(ConfigSchEntryCmd->Slot, ConfigSchEntryCmd->EntryInSlot, ConfigSchEntryCmd->ConfigFLag);
      }
      else
      {
         CFE_EVS_SendEvent (666, CFE_EVS_ERROR, "Configure command error. Invalid entry %d greater than max %d",
                            ConfigSchEntryCmd->EntryInSlot,SCHTBL_ENTRIES_PER_SLOT);
      }

   } /* End if valid slot ID */
   else
   {
      CFE_EVS_SendEvent (666, CFE_EVS_ERROR, "Configure command error. Invalid slot %d greater than max %d",
                         ConfigSchEntryCmd->Slot,SCHTBL_TOTAL_SLOTS);

   } /* End if invalid slot ID */

   return TRUE;


} /* End TBLMGR_ConfigSchEntryCmd() */


/******************************************************************************
** Function: TBLMGR_LoadSchTable
**
*/
boolean TBLMGR_LoadSchTable(const CFE_SB_MsgPtr_t MsgPtr)
{
   const  TBLMGR_LoadTblCmd *LoadTblCmd = (const TBLMGR_LoadTblCmd *) MsgPtr;
   int    slot, entry;

   /*
    * Set all data and flags to zero.
    * AttrErrCnt - Incremented by XML parser
    */
   CFE_PSP_MemSet(&TblMgr->SchTbl, 0, sizeof(TBLMGR_SchTbl));
   TblMgr->SchTbl.LoadActive    = TRUE;

   if (ParseXmlFile(LoadTblCmd->FileName, SchTblStartElement, SchTblEndElement) &&
       TblMgr->SchTbl.AttrErrCnt == 0)
   {

      if (LoadTblCmd->LoadType == TBLMGR_LOAD_TBL_REPLACE)
      {
         SCHTBL_LoadTable(&(TblMgr->SchTbl.Local));
         TblMgr->SchTbl.LastLoadValid = TRUE;

      } /* End if replace entire table */
      else if (LoadTblCmd->LoadType == TBLMGR_LOAD_TBL_UPDATE)
      {
         for (slot=0; slot < SCHTBL_TOTAL_SLOTS; slot++)
         {
            for (entry=0; entry < SCHTBL_ENTRIES_PER_SLOT; entry++)
            {

               if (TblMgr->SchTbl.Modified[SCHTBL_INDEX(slot,entry)])
                  SCHTBL_LoadTableEntry(slot, entry, &(TblMgr->SchTbl.Local.Entry[SCHTBL_INDEX(slot,entry)]));
            }
            TblMgr->SchTbl.LastLoadValid = TRUE;
         }

      } /* End if update individual records */
      else
      {
         CFE_EVS_SendEvent(666,CFE_EVS_ERROR,"TBLMGR: Invalid scheduler table command type %d",LoadTblCmd->LoadType);
      }

   } /* End if successful MSGTBL parse */
   else
   {
      CFE_EVS_SendEvent(666,CFE_EVS_ERROR,"TBLMGR: Scheduler table command file parsing failed");
   }

   return TblMgr->SchTbl.LastLoadValid;

} /* End of TBLMGR_LoadSchTable() */


/******************************************************************************
** Function: TBLMGR_DumpSchTable
**
*/
boolean TBLMGR_DumpSchTable(const CFE_SB_MsgPtr_t MsgPtr)
{
   const  TBLMGR_DumpTblCmd *DumpTblCmd = (const TBLMGR_DumpTblCmd *) MsgPtr;
   char   FileName[OS_MAX_PATH_LEN];

   /* Copy the commanded filename into local buffer to ensure size limitation and to allow for modification */
   CFE_PSP_MemCpy(FileName, (void *)DumpTblCmd->FileName, OS_MAX_PATH_LEN);

   /* Check to see if a default filename should be used */
   if (FileName[0] == '\0')
   {
      strncpy(FileName, TBLMGR_DEF_MSG_TBL_DUMP_FILE, OS_MAX_PATH_LEN);
   }

   /* Make sure all strings are null terminated before attempting to process them */
   FileName[OS_MAX_PATH_LEN-1] = '\0';

   return DumpTableToFile(FileName,"Scheduler App's Scheduler Table",DumpSchTable);

} /* End of TBLMGR_DumpSchTable() */


/******************************************************************************
**
** ParseXmlFile
**
** Parse an XML file
*/
static boolean ParseXmlFile(const char* FilePathName,
                            XML_StartElementHandler StartElementHandler,
                            XML_EndElementHandler   EndElementHandler)
{

   int      FileHandle;
   int32    ReadStatus;
   boolean  Done;
   boolean  ParseErr  = FALSE;
   boolean  RetStatus = FALSE;

   XML_Parser XmlParser = XML_ParserCreate(NULL);
   if (! XmlParser) {
      CFE_EVS_SendEvent(TBLMGR_CREATE_ERR_EID, CFE_EVS_ERROR, "Failed to allocate memory for XML parser");
   }
   else {

      XML_SetElementHandler(XmlParser, StartElementHandler, EndElementHandler);

      /* CFE_EVS_SendEvent(999,CFE_EVS_INFORMATION,"ParseXmlFile() - About to open file %s",FilePathName); */
      FileHandle = OS_open(FilePathName, OS_READ_ONLY, 0);
      
      if (FileHandle >= 0) {

         /* CFE_EVS_SendEvent(999,CFE_EVS_INFORMATION,"ParseXmlFile() - Processing File"); */

         Done = FALSE;

         while (!Done) {

            ReadStatus = OS_read(FileHandle, TblFileBuff, TBLMGR_BUFFSIZE);

            if ( ReadStatus == OS_FS_ERROR )
            {
               CFE_EVS_SendEvent(TBLMGR_FILE_READ_ERR_EID, CFE_EVS_ERROR, "File read error, EC = 0x%08X",ReadStatus);
               Done = TRUE;
               ParseErr = TRUE;
            }
            else if ( ReadStatus == 0 ) /* EOF reached */
            {
                Done = TRUE;
            }
            else {

               /* ReadStatus contains numbebr of bytes read */
               if (XML_Parse(XmlParser, TblFileBuff, ReadStatus, Done) == XML_STATUS_ERROR) {

                  CFE_EVS_SendEvent(TBLMGR_PARSE_ERR_EID, CFE_EVS_ERROR, "Parse error at line %l, error code = %s",
                                    XML_GetCurrentLineNumber(XmlParser),
                                    XML_ErrorString(XML_GetErrorCode(XmlParser)));
                  Done = TRUE;
                  ParseErr = TRUE;

               } /* End if valid parse */
            } /* End if valid fread */
         } /* End file read loop */

         RetStatus = !ParseErr;
         
         OS_close(FileHandle);

      } /* End if file opened */
      else
      {
      
          CFE_EVS_SendEvent(TBLMGR_FILE_OPEN_ERR_EID, CFE_EVS_ERROR, "File open error for %s, Error = %d",
                            FilePathName, FileHandle );
      }

      XML_ParserFree(XmlParser);

   } /* end if parser allocated */

   return RetStatus;

} /* End ParseXmlFile() */

/******************************************************************************
**
** SchTblStartElement
**
** Callback function for the XML parser. It assumes global variables have been
** properly setup before the parsing begins.
**
*/
static void XMLCALL SchTblStartElement(void *data, const char *el, const char **attr)
{
  int     i;
  uint16  ProcessedAttr = 0x3F;  /* 6 Attribute bits */
  uint16  SlotId = 0;
  uint16  Entry = 0;
  boolean Enabled = FALSE;
  uint16  Frequency = 0;
  uint16  Offset = 0;
  uint16  MsgTblEntryId = 0;

  if (strcmp(el,TBLMGR_XML_EL_SCH_SLOT) == 0)
  {

     for (i = 0; attr[i]; i += 2) {
        if (strcmp(attr[i],TBLMGR_XML_AT_ID)==0) {
           SlotId = atoi(attr[i + 1]);
           ProcessedAttr &= 0xFE;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_ENTRY)==0)
        {
           Entry = atoi(attr[i + 1]);
           ProcessedAttr &= 0xFD;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_ENABLE)==0)
        {
           Enabled = (strcmp(attr[i+1],TBLMGR_XML_AT_VAL_TRUE)==0);
           ProcessedAttr &= 0xFB;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_FREQ)==0)
        {
           Frequency = atoi(attr[i + 1]);
           ProcessedAttr &= 0xF7;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_OFFSET)==0)
        {
           Offset = atoi(attr[i + 1]);
           ProcessedAttr &= 0xEF;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_MSG_ID)==0)
        {
           MsgTblEntryId = atoi(attr[i + 1]);
           ProcessedAttr &= 0xDF;
        }

     } /* End attribute loop */

     if (ProcessedAttr == 0)
     {
        /* TODO - Add error checking */
        i = SCHTBL_INDEX(SlotId,Entry);
        TblMgr->SchTbl.Local.Entry[i].Enabled       = Enabled;
        TblMgr->SchTbl.Local.Entry[i].Frequency     = Frequency;
        TblMgr->SchTbl.Local.Entry[i].Offset        = Offset;
        TblMgr->SchTbl.Local.Entry[i].MsgTblEntryId = MsgTblEntryId;
        TblMgr->SchTbl.Modified[i] = TRUE;
     }
     else
     {
        TblMgr->SchTbl.AttrErrCnt++;
     }

     /* CFE_EVS_SendEvent(999, CFE_EVS_INFORMATION, "SchTbl File Slot: Id=%d,Entry=%d,Ena=%d,Freq=%d,Offset=%d,MsgId=%d", SlotId,Entry,Enabled,Frequency,Offset,MsgTblEntryId); */

     /*
     printf("SchTbl File Slot: Id=%d,Entry=%d,Ena=%d,Freq=%d,Offset=%d,MsgId=%d\n",
                       SlotId,Entry,Enabled,Frequency,Offset,MsgTblEntryId);
     */

  } /* End if Slot Element found */

} /* End SchTblStartElement() */

static void XMLCALL SchTblEndElement(void *data, const char *el)
{

} /* End SchTblEndElement() */


/******************************************************************************
**
** MsgTblStartElement
**
** Callback function for the XML parser. It assumes global variables have been
** properly setup before the parsing begins.
**
*/
static void XMLCALL MsgTblStartElement(void *data, const char *el, const char **attr)
{
  int i;
  uint16  ProcessedAttr = 0x0F;  /* 4 Attribute bits */
  uint16  Id =0;
  uint16  StreamId = 0;
  uint16  SeqSeg = 0;
  uint16  MsgLen = 0;

  if (strcmp(el,TBLMGR_XML_EL_MSG_ENTRY) == 0)
  {
     /* printf ("MsgTblStartElement() - Entry: "); */
     for (i = 0; attr[i]; i += 2) {
        if (strcmp(attr[i],TBLMGR_XML_AT_ID)==0) {
           /* printf("%s, ", attr[i + 1]); */
           Id = atoi(attr[i + 1]);
           ProcessedAttr &= 0xFE;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_STREAM_ID)==0)
        {
           StreamId = atoi(attr[i + 1]);
           ProcessedAttr &= 0xFD;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_SEQ_SEG)==0)
        {
           SeqSeg = atoi(attr[i + 1]);
           ProcessedAttr &= 0xFB;
        } else if (strcmp(attr[i],TBLMGR_XML_AT_MSG_LEN)==0)
        {
           MsgLen = atoi(attr[i + 1]);
           ProcessedAttr &= 0xF7;
        }
     } /* End attribute loop */
     /* printf("\n"); */

     if (ProcessedAttr == 0)
     {
        /* TODO - Add error checking */
        TblMgr->MsgTbl.Local.Entry[Id].Buffer[0] = StreamId;
        TblMgr->MsgTbl.Local.Entry[Id].Buffer[1] = SeqSeg;
        TblMgr->MsgTbl.Local.Entry[Id].Buffer[2] = MsgLen;
        TblMgr->MsgTbl.Modified[Id] = TRUE;
     }
     else
     {
        TblMgr->MsgTbl.AttrErrCnt++;
     }

     /* CFE_EVS_SendEvent(999, CFE_EVS_INFORMATION, "MsgTbl File Entry: Id=%d,StreamId=%d,SeqSeg=%d,MsgLen=%d",Id,StreamId,SeqSeg,MsgLen); */

  } /* End if MSGTBL_XML_EL_ENTRY found */

} /* End MsgTblStartElement() */

static void XMLCALL MsgTblEndElement(void *data, const char *el)
{

} /* End MsgTblEndElement() */

/******************************************************************************
**
** DumpMsgTable
**
** Dump the message table contents to a file as text. The function signature
** must correspond to the DumpTableFuncPtr type.
**
*/
static void DumpMsgTable(int32 FileHandle)
{
   const  MSGTBL_Table  *MsgTblPtr;
   char   DumpRecord[256];
   int    i;

   sprintf(DumpRecord,"\nEntry: Stream ID, Sequence-Segment, Length\n");
   OS_write(FileHandle,DumpRecord,strlen(DumpRecord));

   MsgTblPtr = MSGTBL_GetTblPtr();

   for (i=0; i < MSGTBL_MAX_ENTRY_ID; i++)
   {
      sprintf(DumpRecord,"%03d: 0x%04X, 0x%04X, 0x%04X\n",
              i, MsgTblPtr->Entry[i].Buffer[0], MsgTblPtr->Entry[i].Buffer[1], MsgTblPtr->Entry[i].Buffer[2]);
      OS_write(FileHandle,DumpRecord,strlen(DumpRecord));
   }

} /* End DumpMsgTable() */

/******************************************************************************
**
** DumpSchTable
**
** Dump the message table contents to a file as text. The function signature
** must correspond to the DumpTableFuncPtr type.
**
*/
static void DumpSchTable(int32 FileHandle)
{
   const  SCHTBL_Table  *SchTblPtr;
   char   DumpRecord[256];
   int    i,slot,entry;

   sprintf(DumpRecord,"\n(Slot,Entry): Enabled, Frequency, Offset, MsgTblEntryId\n");
   OS_write(FileHandle,DumpRecord,strlen(DumpRecord));

   SchTblPtr = SCHTBL_GetTblPtr();

   for (slot=0; slot < SCHTBL_TOTAL_SLOTS; slot++)
   {
      for (entry=0; entry < SCHTBL_ENTRIES_PER_SLOT; entry++)
      {

         i = SCHTBL_INDEX(slot,entry);
         sprintf(DumpRecord,"(%03d,%02d): %d, %d, %d, %d\n", slot, entry,
                 SchTblPtr->Entry[i].Enabled,SchTblPtr->Entry[i].Frequency,
                 SchTblPtr->Entry[i].Offset,SchTblPtr->Entry[i].MsgTblEntryId);
         OS_write(FileHandle,DumpRecord,strlen(DumpRecord));

      } /* End entry loop */
   } /* End slot loop */

} /* End DumpSchTable() */

/******************************************************************************
** Function: DumpTableToFile
**
*/
static boolean DumpTableToFile(const char* FileName, const char* FileDescr, DumpTableFuncPtr DumpTableFunc)
{

   CFE_FS_Header_t  CfeStdFileHeader;
   int32            FileHandle;
   int32            FileStatus;
   boolean          RetStatus = FALSE;

   /* Create a new dump file, overwriting anything that may have existed previously */
   FileHandle = OS_creat(FileName, OS_WRITE_ONLY);

   if (FileHandle >= OS_FS_SUCCESS)
   {
      /* Initialize the standard cFE File Header for the Dump File */
      CfeStdFileHeader.SubType = 0x74786574;
      strcpy(&CfeStdFileHeader.Description[0], FileDescr);

      /* Output the Standard cFE File Header to the Dump File */
      FileStatus = CFE_FS_WriteHeader(FileHandle, &CfeStdFileHeader);

      if (FileStatus == sizeof(CFE_FS_Header_t))
      {
         (DumpTableFunc)(FileHandle);
         RetStatus = TRUE;

      } /* End if successfully wrote file header */
      else
      {
          CFE_EVS_SendEvent(TBLMGR_WRITE_CFE_HDR_ERR_EID,
                            CFE_EVS_ERROR,
                            "Error writing cFE File Header to '%s', Status=0x%08X",
                            FileName, FileStatus);

      }
   } /* End if file create */
   else
   {

        CFE_EVS_SendEvent(TBLMGR_CREATE_MSG_DUMP_ERR_EID,
                          CFE_EVS_ERROR,
                          "Error creating CDS dump file '%s', Status=0x%08X",
                          FileName, FileHandle);

    }

   OS_close(FileHandle);

   return RetStatus;

} /* End of DumpTableToFile() */


/* end of file */
