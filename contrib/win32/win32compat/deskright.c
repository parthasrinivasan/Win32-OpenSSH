/*
 * Author: NoMachine <developers@nomachine.com>
 *
 * Copyright (c) 2009, 2011 NoMachine
 * All rights reserved
 *
 * Support functions and system calls' replacements needed to let the
 * software run on Win32 based operating systems.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "deskright.h"

/*
 * Retrieve SID from access token.
 *
 * hToken - access token (IN)
 * psid   - user's SID (OUT)
 *
 * RETURNS: TRUE if OK.
 */

BOOL ObtainSid(HANDLE hToken, PSID *psid)
{
  debug3("-> ObtainSid()...");
  
  BOOL bSuccess = FALSE;
  
  DWORD dwIndex;
  
  DWORD dwLength = 0;

  TOKEN_INFORMATION_CLASS tic = TokenGroups;

  PTOKEN_GROUPS ptg = NULL;

  /* 
   * determine the size of the buffer
   */
  
  if (!GetTokenInformation(hToken, tic, (LPVOID) ptg, 0, &dwLength))
  {
    FAIL(GetLastError() != ERROR_INSUFFICIENT_BUFFER);

    ptg = (PTOKEN_GROUPS) HeapAlloc(GetProcessHeap(), 
                                        HEAP_ZERO_MEMORY, dwLength);

    FAIL(ptg == NULL);  
  }

  /*
   * obtain the groups the access token belongs to
   */

  FAIL(GetTokenInformation(hToken, tic, (LPVOID) ptg, 
                               dwLength, &dwLength) == FALSE);

  /*
   * determine which group is the logon sid
   */

  for (dwIndex = 0; dwIndex < ptg -> GroupCount; dwIndex++)
  {
    if ((ptg -> Groups[dwIndex].Attributes & SE_GROUP_LOGON_ID) ==  SE_GROUP_LOGON_ID)
    {
      /*
       * determine the length of the sid
       */
      
      dwLength = GetLengthSid(ptg -> Groups[dwIndex].Sid);

      /*
       * allocate a buffer for the logon sid
       */
  
      *psid = (PSID) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLength);
      
       FAIL(*psid == NULL);

       /*
        * obtain a copy of the logon sid
        */
  
       FAIL(CopySid(dwLength, *psid, ptg -> Groups[dwIndex].Sid) == FALSE);

       /*
        * Break out of the loop because the logon sid has been
        * found.
        */
  
       break;
     }
   }

   /*
    * Indicate success.
    */
   
   bSuccess = TRUE;

 fail:

 /*
  * Free the buffer for the token group.
  */
 
 if (ptg != NULL)
 {
   HeapFree(GetProcessHeap(), 0, (LPVOID)ptg);
 }

  debug3("<- ObtainSid()...");
  
  return bSuccess;
}

/*
 * Gives or removes user rights to use given WinStation object.
 *
 * WARNING. This rights is given only for login session, i.e,
 *          acount's properties are not be changed.
 *
 * hwinsta - handle to WindowsStation object (IN)
 * psid    - pointer to user's SID (IN)
 * mode    - 1 for add, 0 for remove right (IN)
 *
 * RETURNS: TRUE if OK.
 */
   
BOOL ModifyTheAceWindowStation(HWINSTA hwinsta, PSID psid, int mode)
{
  debug2("-> ModifyTheAceWindowStation(mode = %d)...", mode);
  
  ACCESS_ALLOWED_ACE *pace = NULL;
  
  ACL_SIZE_INFORMATION aclSizeInfo;
  
  BOOL bDaclExist;
  BOOL bDaclPresent;
  BOOL bSuccess = FALSE;
                        
  DWORD dwNewAclSize;
  DWORD dwSidSize = 0;
  DWORD dwSdSizeNeeded;
  
  PACL pacl;
  PACL pNewAcl = NULL;
  
  PSECURITY_DESCRIPTOR psd    = NULL;
  PSECURITY_DESCRIPTOR psdNew = NULL;
  
  ACCESS_ALLOWED_ACE *pTempAce;
  
  SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
  
  unsigned int i;
  
  /*
   * is input SID valid?
   */
  
  debug3("Testing is SID valid...");

  FAIL(psid == NULL);
  
  FAIL(IsValidSid(psid) == FALSE);

  /*
   * obtain the dacl for the windowstation
   */

  debug3("GetUserObjectSecurity()...");
  
  if (!GetUserObjectSecurity(hwinsta, &si, psd, dwSidSize, &dwSdSizeNeeded))
  {
    FAIL(GetLastError() != ERROR_INSUFFICIENT_BUFFER);

    psd = (PSECURITY_DESCRIPTOR) HeapAlloc(GetProcessHeap(), 
                                               HEAP_ZERO_MEMORY, 
                                                   dwSdSizeNeeded);

    FAIL(psd == NULL);

    psdNew = (PSECURITY_DESCRIPTOR) HeapAlloc(GetProcessHeap(), 
                                                  HEAP_ZERO_MEMORY, 
                                                      dwSdSizeNeeded);
      
    FAIL(psdNew == NULL);

    dwSidSize = dwSdSizeNeeded;

    FAIL(GetUserObjectSecurity(hwinsta, &si, psd, 
                                   dwSidSize, &dwSdSizeNeeded) == FALSE);
  }

  /*
   * Create a new dacl.
   */
  
  debug3("InitializeSecurityDescriptor()...");
  
  FAIL(InitializeSecurityDescriptor(psdNew, SECURITY_DESCRIPTOR_REVISION) == FALSE);

  /*
   * get dacl from the security descriptor.
   */
 
  debug3("GetSecurityDescriptorDacl()...");
  
  FAIL(GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl, &bDaclExist) == FALSE);

  /*
   * Initialize.
   */
 
  ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
  aclSizeInfo.AclBytesInUse = sizeof(ACL);

  /*
   * Call only if the dacl is not NULL.
   */

  if (pacl != NULL)
  {
    /*
     * Get the file ACL size info.
     */
 
    debug3("GetAclInformation()...");
    
    FAIL(GetAclInformation(pacl, (LPVOID) &aclSizeInfo,
                               sizeof(ACL_SIZE_INFORMATION), 
                                   AclSizeInformation) == FALSE);
  }

  /*
   * Compute the size of the new acl.
   */
  
  debug3("Calculating dwNewAclSize...");
    
  dwNewAclSize = aclSizeInfo.AclBytesInUse;
               
  if (mode == ADD_RIGHT)
  {
    dwNewAclSize = dwNewAclSize + (2 * GetLengthSid(psid)) 
                 + (2 * sizeof(ACCESS_ALLOWED_ACE))
                 - (2 * sizeof(DWORD));
  }
  else
  {
    dwNewAclSize = dwNewAclSize + (2 * GetLengthSid(psid)) 
                 - (2 * sizeof(ACCESS_ALLOWED_ACE))
                 + (2 * sizeof(DWORD));
  }

  debug3("dwNewAclSize = %d", dwNewAclSize);

  /*
   * Allocate memory for the new acl.
   */

  debug3("HeapAlloc()...");
  
  pNewAcl = (PACL) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNewAclSize);

  FAIL(pNewAcl == NULL);

  /*
   * Initialize the new dacl.
   */

  debug3("InitializeAcl()...");
  
  FAIL(InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION) == FALSE);

  /*
   * If DACL is present, copy it to a new DACL.
   */
 
  if (bDaclPresent) 
  {
    /*
     * Copy the ACEs from old to new ACL.
     */
    
    if (aclSizeInfo.AceCount)
    {

      debug3("aclSizeInfo.AceCount = %d", aclSizeInfo.AceCount);
       
      for (i = 0; i < aclSizeInfo.AceCount; i++)
      {
        /*
         * Get next ACE from old ACL.
         */
  
        FAIL(GetAce(pacl, i, &pTempAce) == FALSE);

        /*
         * Add the ACE to the new ACL.
         *
         * We copy all original list for RIGHT_ADD mode and
         * skip ACE with given input SID in RIGHT_REMOVE mode.
         */

        if (mode == ADD_RIGHT || EqualSid(psid, &pTempAce -> SidStart) == 0)
        {        
          FAIL(AddAce(pNewAcl, ACL_REVISION, MAXDWORD, pTempAce, 
                          ((PACE_HEADER) pTempAce) -> AceSize) == FALSE);
        }                  
      }
    }
  }

  if (mode == ADD_RIGHT)
  {
    /*
     * Add the first ACE to the windowstation.
     */
  
    pace = (ACCESS_ALLOWED_ACE *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 
                                                sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) - sizeof(DWORD));
  
    FAIL(pace == NULL);

    pace -> Header.AceType  = ACCESS_ALLOWED_ACE_TYPE;
    pace -> Header.AceFlags = CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;
    pace -> Header.AceSize  = sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) - sizeof(DWORD);
    pace -> Mask            = GENERIC_ACCESS;

    debug3("CopySid()...");
      
    FAIL(CopySid(GetLengthSid(psid), &pace -> SidStart, psid) == FALSE);

    debug3("AddAce()...");
  
    FAIL(AddAce(pNewAcl, ACL_REVISION, MAXDWORD, 
                    (LPVOID)pace, pace -> Header.AceSize) == FALSE);

    /*
     * Add the second ACE to the windowstation.
     */
 
    pace -> Header.AceFlags = NO_PROPAGATE_INHERIT_ACE;
    pace -> Mask = WINSTA_ALL;

    debug3("AddAce()...");
  
    FAIL(AddAce(pNewAcl, ACL_REVISION, MAXDWORD, 
                    (LPVOID) pace, pace -> Header.AceSize) == FALSE);
  }
  
  /*
   * Set new dacl for the security descriptor.
   */
 
  debug3("SetSecurityDescriptorDacl()...");
  
  FAIL(SetSecurityDescriptorDacl(psdNew, TRUE, pNewAcl, FALSE) == FALSE);

  /*
   * Set the new security descriptor for the windowstation.
   */
  
  debug3("SetUserObjectSecurity()...");
  
  FAIL(SetUserObjectSecurity(hwinsta, &si, psdNew) == FALSE);

  /*
   * Indicate success.
   */
 
  bSuccess = TRUE;

fail:

  /*
   * Free the allocated buffers.
   */
  
  if (pace != NULL)
  {
    HeapFree(GetProcessHeap(), 0, (LPVOID)pace);
  }

  if (pNewAcl != NULL)
  {
    HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);
  }

  if (psd != NULL)
  {
    HeapFree(GetProcessHeap(), 0, (LPVOID)psd); 
  }

  if (psdNew != NULL)
  {
    HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
  }

  return bSuccess;
}

/*
 * Gives ore removes user right to use given desktop.
 *
 * WARNING. This right is given only for login session, i.e,
 *          account's properties are not be changed.
 *
 * hdesk - handle to desktop (IN)
 * psid  - pointer to user's SID (IN)
 * mode  - 1 for add, 0 for remove (IN)
 *
 * RETURNS: TRUE if OK.
 */

BOOL ModifyTheAceDesktop(HDESK hdesk, PSID psid, int mode)
{
  debug2("-> ModifyTheAceDesktop(mode = %d)...", mode);
  
  ACL_SIZE_INFORMATION aclSizeInfo;
  
  BOOL bDaclExist;
  BOOL bDaclPresent;
  BOOL bSuccess = FALSE;
                                                
  DWORD dwNewAclSize;
  DWORD dwSidSize = 0;
  DWORD dwSdSizeNeeded;

  PACL pacl;
  PACL pNewAcl;

  PSECURITY_DESCRIPTOR psd    = NULL;
  PSECURITY_DESCRIPTOR psdNew = NULL;
  
  HANDLE procHeap = NULL;
  
  ACCESS_ALLOWED_ACE *pTempAce;

  SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;

  unsigned int i;

  /*
   * is input SID valid?
   */
  
  debug3("Testing is SID valid...");

  FAIL(psid == NULL);
  
  FAIL(IsValidSid(psid) == FALSE);

  /*
   * Obtain process heap. 
   */
  
  procHeap = GetProcessHeap();
  
  /*
   * Obtain the security descriptor for the desktop object.
   */

  debug3("GetUserObjectSecurity()...");
  
  if (!GetUserObjectSecurity(hdesk, &si, psd, 
                                 dwSidSize, &dwSdSizeNeeded))
  {
    FAIL(GetLastError() != ERROR_INSUFFICIENT_BUFFER);

    psd = (PSECURITY_DESCRIPTOR) HeapAlloc(procHeap,
                                               HEAP_ZERO_MEMORY, 
                                                   dwSdSizeNeeded);
      
    FAIL(psd == NULL); 
     
    psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(procHeap, 
                                                 HEAP_ZERO_MEMORY, 
                                                     dwSdSizeNeeded);

    FAIL(psdNew == NULL);
      
    dwSidSize = dwSdSizeNeeded;

    FAIL(GetUserObjectSecurity(hdesk, &si, psd, dwSidSize, 
                                   &dwSdSizeNeeded) == FALSE);
  }
  
  /*
   * create a new security descriptor.
   */

  debug3("InitializeSecurityDescriptor()...");
  
  FAIL(InitializeSecurityDescriptor(psdNew, 
                                        SECURITY_DESCRIPTOR_REVISION) == FALSE);

  /*
   * obtain the dacl from the security descriptor.
   */

  debug3("GetSecurityDescriptorDacl()...");
  
  FAIL(GetSecurityDescriptorDacl(psd, &bDaclPresent, 
                                     &pacl, &bDaclExist) == FALSE);

  /*
   * Initialize.
   */

  ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
 
  aclSizeInfo.AclBytesInUse = sizeof(ACL);

  /*
   * Call only if NULL dacl.
   */
 
  if (pacl != NULL)
  {
    /*
     * determine the size of the ACL info.
     */

    debug3("GetAclInformation()..");
  
    FAIL(GetAclInformation(pacl, (LPVOID)&aclSizeInfo, 
                               sizeof(ACL_SIZE_INFORMATION), 
                                   AclSizeInformation) == FALSE);
  }

  /*
   * Compute the size of the new acl.
   */
 
  dwNewAclSize = aclSizeInfo.AclBytesInUse;
  
  if (mode == ADD_RIGHT)
  {
    dwNewAclSize = dwNewAclSize + sizeof(ACCESS_ALLOWED_ACE) 
                 + GetLengthSid(psid) - sizeof(DWORD);
  }
  else
  {
    dwNewAclSize = dwNewAclSize - sizeof(ACCESS_ALLOWED_ACE) 
                 - GetLengthSid(psid) + sizeof(DWORD);
  }

  /*
   * Allocate buffer for the new acl.
   */

  pNewAcl = (PACL) HeapAlloc(procHeap, 
                                 HEAP_ZERO_MEMORY, dwNewAclSize);

  FAIL(pNewAcl == NULL);

  /*
   * Initialize the new acl.
   */
 
  debug3("InitializeAcl()..");
    
  FAIL(InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION) == FALSE);

  /*
   * If DACL is present, copy it to a new DACL.
   */

  if (bDaclPresent) // only copy if DACL was present
  {
    /*
     * Copy the ACEs to our new ACL.
     */
      
    if (aclSizeInfo.AceCount)
    {

      for (i=0; i < aclSizeInfo.AceCount; i++)
      {
        /*
         * Get next ACE from old ACL.
         */
  
        FAIL(GetAce(pacl, i, &pTempAce) == FALSE);

        /*
         * Add the ACE to the new ACL.
         *
         * We copy all original list for RIGHT_ADD mode and
         * skip ACE with given input SID in RIGHT_REMOVE mode.
         */

        if (mode == ADD_RIGHT || EqualSid(psid, &pTempAce -> SidStart) == 0)
        {        
          FAIL(AddAce(pNewAcl, ACL_REVISION, MAXDWORD, pTempAce, 
                          ((PACE_HEADER) pTempAce) -> AceSize) == FALSE);
        }                  
      }
    }
  }

  if (mode == ADD_RIGHT)
  {
    /*
     * Add one additional ace to the dacl.
     */
  
    debug3("AccessAllowedAce()...");
  
    FAIL(AddAccessAllowedAce(pNewAcl, ACL_REVISION, 
                                 DESKTOP_ALL, psid) == FALSE);
  } 

  /*
   * Set new dacl to the new security descriptor.
   */

  debug3("AddSecurityDescriptiorDacl()..");
  
  FAIL(SetSecurityDescriptorDacl(psdNew, TRUE, pNewAcl, FALSE) == FALSE);

  /*
   * Set the new security descriptor for the desktop object.
   */
   
  debug3("SetUserObjectSecurity()..");
  
  FAIL(SetUserObjectSecurity(hdesk, &si, psdNew) == FALSE);

  /*
   * Indicate success.
   */

  bSuccess = TRUE;

fail:

  /*
   * Free buffers.
   */

  debug3("Freeing buffers...");
  
  if (pNewAcl != NULL)
  {
    HeapFree(procHeap, 0, (LPVOID) pNewAcl);
  }

  if (psd != NULL)
  {
    HeapFree(procHeap, 0, (LPVOID) psd);
  }

  if (psdNew != NULL)
  {
    HeapFree(procHeap, 0, (LPVOID) psdNew);
  }

  debug3("<- AddTheAceDesktop()...");
  
  return bSuccess;
}

void RemoveSid(PSID *psid)
{
  HeapFree(GetProcessHeap(), 0, (LPVOID) *psid);
}

/*
 * Gives user rights to use 'WinStation0' and 'default' desktop.
 *
 * psid - pointer to SID for acount SID (IN)
 * mode - 1 for add, 0 for remove (IN)
 *
 * RETURNS: 0 if OK.
 */

int ModifyRightsToDesktopBySid(PSID psid, int mode)
{
  debug3("-> ModifyRightsToDesktopBySid(mode = %d)...", mode);
  
  HDESK hdesk = NULL;

  HWINSTA hwinsta = NULL;
  
  int exitCode = -1;
    
  /*
   * obtain a handle to the interactive windowstation.
   */
  
  debug3("OpenWindowStation()...");
  
  hwinsta = OpenWindowStation("winsta0", FALSE, READ_CONTROL | WRITE_DAC);
  
  FAIL(hwinsta == NULL);

  debug3("GetProcessWindowStation()...");
  
  HWINSTA hwinstaold = GetProcessWindowStation();

  /*
   * Set the windowstation to winsta0 so that you obtain the
   * correct default desktop.
   */
  
  debug3("SetProcessWindowStation()...");
  
  FAIL(!SetProcessWindowStation(hwinsta));

  /*
   * Obtain a handle to the "default" desktop.
   */

  debug3("OpenDesktop()...");
  
  hdesk = OpenDesktop("default", 0, FALSE, READ_CONTROL | WRITE_DAC |
                          DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS);
  
  FAIL(hdesk == NULL);

  /*
   * Add the user to interactive windowstation.
   */
  
  debug3("ModifyTheAceWindowStation()...");
  
  FAIL(!ModifyTheAceWindowStation(hwinsta, psid, mode));

  /*
   * Add user to "default" desktop.
   */
  
  debug3("AddTheAceDesktop()...");
  
  FAIL(!ModifyTheAceDesktop(hdesk, psid, mode));

  exitCode = 0;

fail:

  /*
   * Close the handles to the interactive windowstation and desktop.
   */
  
  debug3("CloseWindowStation()...");
  
  if (hwinsta)
  {
    CloseWindowStation(hwinsta);
  }  

  debug3("CloseDesktop()...");
  
  if (hdesk)
  {
    CloseDesktop(hdesk);
  }
  
  debug3("<- ModifyRightsToDesktopBySid()...");
  
  return exitCode;
}

/*
 * Gives or removes user rights to use 'WinStation0' and 'default' desktop.
 *
 * hToken - logged user's token (IN)
 * mode   - 1 for add, 0 for remove (IN)
 *
 * RETURNS: 0 if OK.
 */

int ModifyRightsToDesktop(HANDLE hToken, int mode)
{
  debug2("-> ModifyRightsToDesktop(mode = %d)...", mode);
  
  PSID psid = NULL;
  
  int exitCode = -1;
  
  /*
   * Obtain the logon sid of the user fester.
   */
  
  debug3("ObtainSid()...");
  
  FAIL(!ObtainSid(hToken, &psid));
  
  FAIL(ModifyRightsToDesktopBySid(psid, mode));
  
  if (psid)
  {
    RemoveSid(&psid);
  }
  
  exitCode = 0;
  
fail:

  debug2("<- ModifyRightsToDesktop()...");

  return exitCode;
}
